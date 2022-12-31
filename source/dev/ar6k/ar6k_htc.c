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

	dietPrint("[AR6K] srv%.4X -> ep%u\n", service_id, u.resp.endpoint_id);
	if (out_ep) {
		*out_ep = (Ar6kHtcEndpointId)u.resp.endpoint_id;
	}

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
