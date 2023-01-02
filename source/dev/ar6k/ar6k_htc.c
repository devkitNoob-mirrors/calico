#include "common.h"
#include <string.h>

typedef struct _Ar6kHtcCtrlPktMem {
	alignas(4) u8 mem[SDIO_BLOCK_SZ];
} _Ar6kHtcCtrlPktMem;

MEOW_CONSTEXPR Ar6kHtcEndpointId _ar6kHtcLookaheadGetEndpointId(u32 lookahead)
{
	return (Ar6kHtcEndpointId)(lookahead & 0xff);
}

MEOW_CONSTEXPR u16 _ar6kHtcLookaheadGetPayloadLen(u32 lookahead)
{
	return lookahead >> 16;
}

static bool _ar6kHtcProcessTrailer(Ar6kDev* dev, void* trailer, size_t size)
{
	while (size) {
		// Ensure the record header fits
		if (size < sizeof(Ar6kHtcRecordHdr)) {
			dietPrint("[AR6K] bad trailer record hdr\n");
			return false;
		}

		// Retrieve record header
		Ar6kHtcRecordHdr* hdr = (Ar6kHtcRecordHdr*)trailer;
		trailer = hdr+1;
		size -= sizeof(Ar6kHtcRecordHdr);

		// Ensure the record data fits
		if (size < hdr->length) {
			dietPrint("[AR6K] bad trailer record size\n");
			return false;
		}

		// Retrieve record data
		void* data = trailer;
		trailer = (u8*)data + hdr->length;
		size -= hdr->length;

		// Process record
		switch (hdr->record_id) {
			default: {
				dietPrint("[AR6K] unk record id: %.2X\n", hdr->record_id);
				break; // ignore (without failing)
			}

			case Ar6kHtcRecordId_Null:
				break; // nop

			case Ar6kHtcRecordId_Credit: {
				// TODO: handle
				dietPrint("[AR6K] received credits\n");
				break;
			}

			case Ar6kHtcRecordId_Lookahead: {
				Ar6kHtcLookaheadReport* rep = (Ar6kHtcLookaheadReport*)data;
				if ((rep->pre_valid ^ rep->post_valid) == 0xff) {
					// This report is valid - update lookahead
					memcpy(&dev->lookahead, rep->lookahead, sizeof(u32));
				}
				break;
			}
		}
	}

	return true;
}

static bool _ar6kHtcWaitforControlMessage(Ar6kDev* dev, _Ar6kHtcCtrlPktMem* mem)
{
	u32 lookahead = 0;
	if (!_ar6kDevPollMboxMsgRecv(dev, &lookahead, 200)) {
		return false;
	}

	Ar6kHtcEndpointId epid = _ar6kHtcLookaheadGetEndpointId(lookahead);
	if (epid != Ar6kHtcEndpointId_Control) {
		dietPrint("[AR6K] Unexpected epID: %u\n", epid);
		return false;
	}

	size_t len = sizeof(Ar6kHtcFrameHdr) + _ar6kHtcLookaheadGetPayloadLen(lookahead);
	if (!_ar6kDevRecvPacket(dev, mem, len)) {
		return false;
	}

	if (*(u32*)mem != lookahead) {
		dietPrint("[AR6K] Lookahead mismatch\n");
		return false;
	}

	return true;
}

// TODO: Temporarily using static storage for received packets
alignas(4) static u8 s_tempRxBuf[sizeof(NetBuf) + AR6K_HTC_MAX_PACKET_SZ];

bool _ar6kHtcRecvMessagePendingHandler(Ar6kDev* dev)
{
	// TODO: this should really be a parameter
	NetBuf* pPacket = (NetBuf*)s_tempRxBuf;
	pPacket->capacity = AR6K_HTC_MAX_PACKET_SZ;

	do {
		// Grab lookahead
		u32 lookahead = dev->lookahead;
		dev->lookahead = 0;

		// Retrieve and validate endpoint ID
		Ar6kHtcEndpointId epid = _ar6kHtcLookaheadGetEndpointId(lookahead);
		if (epid >= Ar6kHtcEndpointId_Last) {
			dietPrint("[AR6K] RX invalid epid = %u\n", epid);
			return false;
		}

		// Retrieve service ID
		Ar6kHtcSrvId srvid = Ar6kHtcSrvId_HtcControl;
		if (epid != Ar6kHtcEndpointId_Control) {
			srvid = dev->endpoints[epid-1].service_id;
			if (!srvid) {
				dietPrint("[AR6K] RX unconnected epid = %u\n", epid);
				return false;
			}
		}

		// Retrieve and validate packet length
		size_t len = sizeof(Ar6kHtcFrameHdr) + _ar6kHtcLookaheadGetPayloadLen(lookahead);
		if (len > AR6K_HTC_MAX_PACKET_SZ) {
			dietPrint("[AR6K] RX pkt too big = %zu\n", len);
			return false;
		}

		// XX: always using statically allocated netbuf
		pPacket->pos = 0;
		pPacket->len = len;

		// Read packet!
		if (!_ar6kDevRecvPacket(dev, netbufGet(pPacket), len)) {
			dietPrint("[AR6K] RX pkt fail\n");
			return false;
		}

		// Compiler optimization
		MEOW_ASSUME(pPacket->pos == 0 && pPacket->len == len);

		// Validate packet against lookahead
		if (*(u32*)netbufGet(pPacket) != lookahead) {
			dietPrint("[AR6K] RX lookahead mismatch\n");
			return false;
		}

		// Handle trailer
		Ar6kHtcFrameHdr* htchdr = netbufPopHeaderType(pPacket, Ar6kHtcFrameHdr);
		if (htchdr->flags & AR6K_HTC_FLAG_RECV_TRAILER) {
			void* trailer = netbufPopTrailer(pPacket, htchdr->recv_trailer_len);
			if (!trailer || !_ar6kHtcProcessTrailer(dev, trailer, htchdr->recv_trailer_len)) {
				return false;
			}
		}

		// Drop the packet if it's empty
		if (pPacket->len == 0) {
			continue;
		}

		// Handle packet!
		switch (srvid) {
			case Ar6kHtcSrvId_HtcControl: {
				dietPrint("[AR6K] Unexpected HTC ctrl pkt\n");
				// Ignore the packet
				break;
			}

			case Ar6kHtcSrvId_WmiControl: {
				_ar6kWmiEventRx(dev, pPacket);
				break;
			}

			default: /* Ar6kHtcSrvId_WmiDataXX */ {
				dietPrint("[AR6K] RX data srv%.4X len%u\n", srvid, pPacket->len);
				// TODO: do something with data packet...
				break;
			}
		}

	} while (dev->lookahead != 0);

	return true;
}

bool _ar6kHtcSendPacket(Ar6kDev* dev, Ar6kHtcEndpointId epid, NetBuf* pPacket)
{
	if (epid == Ar6kHtcEndpointId_Control) {
		dietPrint("[AR6K] TX attempt on HTC ctrl\n");
		return false;
	}

	Ar6kEndpoint* ep = &dev->endpoints[epid-1];
	if (!ep->service_id) {
		dietPrint("[AR6K] TX attempt on unconn ep\n");
		return false;
	}

	u16 payload_len = pPacket->len;
	Ar6kHtcFrameHdr* htchdr = netbufPushHeaderType(pPacket, Ar6kHtcFrameHdr);
	if (!htchdr) {
		dietPrint("[AR6K] TX insufficient headroom\n");
		return false;
	}

	// Fill in HTC frame header
	htchdr->endpoint_id = epid;
	htchdr->flags = AR6K_HTC_FLAG_NEED_CREDIT_UPDATE; // TODO: smarter logic
	htchdr->payload_len = payload_len;

	// XX: Check we actually have credits

#if 0
	// XX: Dump packet
	dietPrint("[AR6K] TX");
	for (unsigned i = 0; i < pPacket->len; i ++) {
		dietPrint(" %.2X", ((u8*)netbufGet(pPacket))[i]);
	}
	dietPrint("\n");
#endif

	// Send the packet!
	return _ar6kDevSendPacket(dev, netbufGet(pPacket), pPacket->len);
}

bool ar6kHtcInit(Ar6kDev* dev)
{
	union {
		_Ar6kHtcCtrlPktMem mem;
		struct {
			Ar6kHtcFrameHdr hdr;
			Ar6kHtcCtrlMsgReady msg;
		};
	} u;

	if (!_ar6kHtcWaitforControlMessage(dev, &u.mem)) {
		return false;
	}

	if (u.msg.hdr.id != Ar6kHtcCtrlMsgId_Ready) {
		dietPrint("[AR6K] Unexpected HTC msg\n");
		return false;
	}

	dev->credit_size  = u.msg.credit_size;
	dev->credit_count = u.msg.credit_count;

	dietPrint("[AR6K] Max endpoints: %u\n", u.msg.max_endpoints);
	dietPrint("[AR6K]   Credit size: %u\n", dev->credit_size);
	dietPrint("[AR6K]  Credit count: %u\n", dev->credit_count);

	return true;
}

Ar6kHtcSrvStatus ar6kHtcConnectService(Ar6kDev* dev, Ar6kHtcSrvId service_id, u16 flags, Ar6kHtcEndpointId* out_ep)
{
	union {
		_Ar6kHtcCtrlPktMem mem;
		struct {
			Ar6kHtcFrameHdr hdr;
			union {
				Ar6kHtcCtrlCmdConnSrv req;
				Ar6kHtcCtrlCmdConnSrvReply resp;
			};
		};
	} u;

	u.hdr.endpoint_id = Ar6kHtcEndpointId_Control;
	u.hdr.flags = 0;
	u.hdr.payload_len = sizeof(Ar6kHtcFrameHdr) + sizeof(Ar6kHtcCtrlCmdConnSrv);
	u.req.hdr.id = Ar6kHtcCtrlMsgId_ConnectService;
	u.req.service_id = service_id;
	u.req.flags = flags;
	u.req.extra_len = 0;

	if (!_ar6kDevSendPacket(dev, &u.mem, u.hdr.payload_len)) {
		return Ar6kHtcSrvStatus_Failed;
	}

	if (!_ar6kHtcWaitforControlMessage(dev, &u.mem)) {
		return Ar6kHtcSrvStatus_Failed;
	}

	if (u.resp.hdr.id != Ar6kHtcCtrlMsgId_ConnectServiceReply) {
		dietPrint("[AR6K] Unexpected HTC msg\n");
		return Ar6kHtcSrvStatus_Failed;
	}

	if (u.resp.service_id != service_id) {
		dietPrint("[AR6K] Unexp. srv%.4X (%.4X)\n", u.resp.service_id, service_id);
		return Ar6kHtcSrvStatus_Failed;
	}

	if (u.resp.status != Ar6kHtcSrvStatus_Success) {
		dietPrint("[AR6K] srv%.4X fail (%u)\n", service_id, u.resp.status);
		return (Ar6kHtcSrvStatus)u.resp.status;
	}

	if (u.resp.endpoint_id == Ar6kHtcEndpointId_Control || u.resp.endpoint_id > Ar6kHtcEndpointId_Last) {
		dietPrint("[AR6K] srv%.4X bad ep (%u)\n", service_id, u.resp.endpoint_id);
		return Ar6kHtcSrvStatus_Failed;
	}

	if (out_ep) {
		*out_ep = (Ar6kHtcEndpointId)u.resp.endpoint_id;
	}

	Ar6kEndpoint* ep = &dev->endpoints[u.resp.endpoint_id-1];
	ep->service_id = service_id;
	ep->max_msg_size = u.resp.max_msg_size;

	return Ar6kHtcSrvStatus_Success;
}

bool ar6kHtcSetupComplete(Ar6kDev* dev)
{
	union {
		_Ar6kHtcCtrlPktMem mem;
		struct {
			Ar6kHtcFrameHdr hdr;
			Ar6kHtcCtrlMsgHdr cmd;
		};
	} u;

	u.hdr.endpoint_id = Ar6kHtcEndpointId_Control;
	u.hdr.flags = 0;
	u.hdr.payload_len = sizeof(Ar6kHtcFrameHdr) + sizeof(Ar6kHtcCtrlMsgHdr);
	u.cmd.id = Ar6kHtcCtrlMsgId_SetupComplete;

	return _ar6kDevSendPacket(dev, &u.mem, u.hdr.payload_len);
}
