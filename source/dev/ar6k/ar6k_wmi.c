#include "common.h"
#include <string.h>

void _ar6kWmiEventRx(Ar6kDev* dev, NetBuf* pPacket)
{
	Ar6kWmiCtrlHdr* evthdr = netbufPopHeaderType(pPacket, Ar6kWmiCtrlHdr);
	if (!evthdr) {
		dietPrint("[AR6K] WMI event RX too small\n");
		return;
	}

	switch (evthdr->id) {
		default: {
			dietPrint("[AR6K] WMI unkevt %.4X (%u)\n", evthdr->id, pPacket->len);
			break;
		}

		case Ar6kWmiEventId_Ready: {
			if (dev->wmi_ready) {
				dietPrint("[AR6K] dupe WMI_READY\n");
				break;
			}

			Ar6kWmiEvtReady* evt = netbufPopHeaderType(pPacket, Ar6kWmiEvtReady);
			if (!evt) {
				dietPrint("[AR6K] invalid WMI event size\n");
				break;
			}

			// Retrieve address to app host interest area
			u32 hi_app_host_interest = 0;
			if (!ar6kDevReadRegDiag(dev, dev->hia_addr+0x00, &hi_app_host_interest)) {
				dietPrint("[AR6K] cannot get AHI area\n");
				break;
			}

			// Set WMI protocol version
			if (!ar6kDevWriteRegDiag(dev, hi_app_host_interest, AR6K_WMI_PROTOCOL_VER)) {
				dietPrint("[AR6K] cannot set WMI ver\n");
				break;
			}

			// WMI is now ready for use!
			memcpy(dev->macaddr, evt->macaddr, 6);
			dev->wmi_ready = true;
			break;
		}

		case Ar6kWmiEventId_GetChannelListReply: {
			Ar6kWmiChannelList* list = (Ar6kWmiChannelList*)netbufGet(pPacket);
			u16 mask = 0;
			for (unsigned i = 0; i < list->num_channels; i ++) {
				mask |= 1U << wlanFreqToChannel(list->channel_mhz[i]);
			}
			dev->wmi_channel_mask = mask;
			break;
		}
	}
}

bool ar6kWmiStartup(Ar6kDev* dev)
{
	// Wait for WMI to be ready
	while (!dev->wmi_ready) {
		threadSleep(1000);
	}

	dietPrint("[AR6K] WMI ready!\n       MAC %.2X:%.2X:%.2X:%.2X:%.2X:%.2X\n",
		dev->macaddr[0], dev->macaddr[1], dev->macaddr[2],
		dev->macaddr[3], dev->macaddr[4], dev->macaddr[5]);

	// Enable all error report events
	if (!ar6kWmiSetErrorReportBitmask(dev, AR6K_WMI_ERPT_ALL)) {
		dietPrint("[AR6K] can't set wmi erpt\n");
		return false;
	}

	// Retrieve channel list
	if (!ar6kWmiGetChannelList(dev)) {
		dietPrint("[AR6K] can't get chnlist\n");
		return false;
	}

	// Wait for the channel list (converted to bitmask) to be populated
	while (!dev->wmi_channel_mask) {
		threadSleep(1000);
	}

	dietPrint("[AR6K] channel mask %.4X\n", dev->wmi_channel_mask);

	return true;
}

// TODO: Temporarily using static storage for WMI commands
alignas(4) static u8 s_tempWmiCmdBuf[sizeof(NetBuf) + AR6K_HTC_MAX_PACKET_SZ];

static NetBuf* _ar6kWmiAllocCmdPacket(Ar6kDev* dev)
{
	// TODO: make this a buf
	NetBuf* pPacket = (NetBuf*)s_tempWmiCmdBuf;
	pPacket->capacity = AR6K_HTC_MAX_PACKET_SZ;
	pPacket->pos = sizeof(Ar6kHtcFrameHdr) + sizeof(Ar6kWmiCtrlHdr);
	pPacket->len = 0;

	return pPacket;
}

static bool _ar6kWmiSendCmdPacket(Ar6kDev* dev, Ar6kWmiCmdId cmdid, NetBuf* pPacket)
{
	Ar6kWmiCtrlHdr* hdr = netbufPushHeaderType(pPacket, Ar6kWmiCtrlHdr);
	if (!hdr) {
		return false; // Shouldn't happen
	}

	hdr->id = cmdid;
	return _ar6kHtcSendPacket(dev, dev->wmi_ctrl_epid, pPacket);
}

bool ar6kWmiSimpleCmd(Ar6kDev* dev, Ar6kWmiCmdId cmdid)
{
	NetBuf* pPacket = _ar6kWmiAllocCmdPacket(dev);
	return _ar6kWmiSendCmdPacket(dev, cmdid, pPacket);
}

bool ar6kWmiSimpleCmdWithParam32(Ar6kDev* dev, Ar6kWmiCmdId cmdid, u32 param)
{
	NetBuf* pPacket = _ar6kWmiAllocCmdPacket(dev);
	netbufPushTrailerType(pPacket, Ar6kWmiGeneric32)->value = param;
	return _ar6kWmiSendCmdPacket(dev, cmdid, pPacket);
}
