#include "common.h"
#include <string.h>

static void _ar6kWmixEventRx(Ar6kDev* dev, NetBuf* pPacket)
{
	Ar6kWmiGeneric32* evthdr = netbufPopHeaderType(pPacket, Ar6kWmiGeneric32);
	if (!evthdr) {
		dietPrint("[AR6K] WMIX event RX too small\n");
		return;
	}

	switch (evthdr->value) {
		default: {
			dietPrint("[AR6K] WMIX unkevt %.lX (%u)\n", evthdr->value, pPacket->len);
			break;
		}

		case Ar6kWmixEventId_DbgLog: {
			dietPrint("[AR6K] WMIX dbglog size=%u\n", pPacket->len);
			break;
		}
	}
}

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

		case Ar6kWmiEventId_Extension: {
			_ar6kWmixEventRx(dev, pPacket);
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
			u32 mask = 0;
			for (unsigned i = 0; i < list->num_channels; i ++) {
				mask |= 1U << wlanFreqToChannel(list->channel_mhz[i]);
			}
			dev->wmi_channel_mask = mask;
			break;
		}

		case Ar6kWmiEventId_BssInfo: {
			Ar6kWmiBssInfoHdr* hdr = netbufPopHeaderType(pPacket, Ar6kWmiBssInfoHdr);
			if (hdr && dev->cb_onBssInfo) {
				dev->cb_onBssInfo(dev, hdr, pPacket);
			}
			break;
		}

		case Ar6kWmiEventId_ScanComplete: {
			Ar6kWmiGeneric32* hdr = netbufPopHeaderType(pPacket, Ar6kWmiGeneric32);
			if (hdr && dev->cb_onScanComplete) {
				dev->cb_onScanComplete(dev, hdr->value);
			}
			break;
		}

		case Ar6kWmiEventId_Connected: {
			dietPrint("[AR6K] Associated!!!!\n");
			break;
		}

		case Ar6kWmiEventId_Disconnected: {
			dietPrint("[AR6K] Deassociated\n");
			Ar6kWmiEvtDisconnected* hdr = netbufPopHeaderType(pPacket, Ar6kWmiEvtDisconnected);
			if (hdr) {
				dietPrint("IEEE reason = %u\n", hdr->reason_ieee);
				dietPrint("Reason = %u\n", hdr->reason);
			}
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

	// Shut up! :p
	if (!ar6kWmixConfigDebugModuleCmd(dev, UINT32_MAX, 0)) {
		dietPrint("[AR6K] can't set dbglog mask\n");
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

	dietPrint("[AR6K] channel mask %.8lX\n", dev->wmi_channel_mask);

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

bool ar6kWmiSimpleCmdWithParam8(Ar6kDev* dev, Ar6kWmiCmdId cmdid, u8 param)
{
	NetBuf* pPacket = _ar6kWmiAllocCmdPacket(dev);
	netbufPushTrailerType(pPacket, Ar6kWmiGeneric8)->value = param;
	return _ar6kWmiSendCmdPacket(dev, cmdid, pPacket);
}

bool ar6kWmiSimpleCmdWithParam32(Ar6kDev* dev, Ar6kWmiCmdId cmdid, u32 param)
{
	NetBuf* pPacket = _ar6kWmiAllocCmdPacket(dev);
	netbufPushTrailerType(pPacket, Ar6kWmiGeneric32)->value = param;
	return _ar6kWmiSendCmdPacket(dev, cmdid, pPacket);
}

bool ar6kWmiConnect(Ar6kDev* dev, Ar6kWmiConnectParams const* params)
{
	NetBuf* pPacket = _ar6kWmiAllocCmdPacket(dev);
	*netbufPushTrailerType(pPacket, Ar6kWmiConnectParams) = *params;
	return _ar6kWmiSendCmdPacket(dev, Ar6kWmiCmdId_Connect, pPacket);
}

bool ar6kWmiCreatePstream(Ar6kDev* dev, Ar6kWmiPstreamConfig const* config)
{
	NetBuf* pPacket = _ar6kWmiAllocCmdPacket(dev);
	*netbufPushTrailerType(pPacket, Ar6kWmiPstreamConfig) = *config;
	return _ar6kWmiSendCmdPacket(dev, Ar6kWmiCmdId_CreatePstream, pPacket);
}

bool ar6kWmiStartScan(Ar6kDev* dev, Ar6kWmiScanType type, u32 home_dwell_time_ms)
{
	NetBuf* pPacket = _ar6kWmiAllocCmdPacket(dev);
	Ar6kWmiCmdStartScan* cmd = netbufPushTrailerType(pPacket, Ar6kWmiCmdStartScan);
	// XX: maybe make some more of these configurable?
	cmd->force_bg_scan = 0;
	cmd->is_legacy = 0;
	cmd->home_dwell_time_ms = home_dwell_time_ms;
	cmd->force_scan_interval_ms = 0;
	cmd->scan_type = type;
	cmd->num_channels = 0;
	return _ar6kWmiSendCmdPacket(dev, Ar6kWmiCmdId_StartScan, pPacket);
}

bool ar6kWmiSetScanParams(Ar6kDev* dev, Ar6kWmiScanParams const* params)
{
	NetBuf* pPacket = _ar6kWmiAllocCmdPacket(dev);
	*netbufPushTrailerType(pPacket, Ar6kWmiScanParams) = *params;
	return _ar6kWmiSendCmdPacket(dev, Ar6kWmiCmdId_SetScanParams, pPacket);
}

bool ar6kWmiSetBssFilter(Ar6kDev* dev, Ar6kWmiBssFilter filter, u32 ie_mask)
{
	NetBuf* pPacket = _ar6kWmiAllocCmdPacket(dev);
	Ar6kWmiCmdBssFilter* cmd = netbufPushTrailerType(pPacket, Ar6kWmiCmdBssFilter);
	cmd->bss_filter = filter;
	cmd->ie_mask = ie_mask;
	return _ar6kWmiSendCmdPacket(dev, Ar6kWmiCmdId_SetBssFilter, pPacket);
}

bool ar6kWmiSetProbedSsid(Ar6kDev* dev, Ar6kWmiProbedSsid const* probed_ssid)
{
	NetBuf* pPacket = _ar6kWmiAllocCmdPacket(dev);
	*netbufPushTrailerType(pPacket, Ar6kWmiProbedSsid) = *probed_ssid;
	return _ar6kWmiSendCmdPacket(dev, Ar6kWmiCmdId_SetProbedSsid, pPacket);
}

bool ar6kWmiSetChannelParams(Ar6kDev* dev, u8 scan_param, u32 chan_mask)
{
	static const u32 chan_mask_a = 0x7fc000; // Channels 14..22
	static const u32 chan_mask_g = 0x003fff; // Channels  0..13
	chan_mask &= chan_mask_a|chan_mask_g;

	NetBuf* pPacket = _ar6kWmiAllocCmdPacket(dev);
	u16 num_channels = __builtin_popcount(chan_mask);
	Ar6kWmiChannelParams* cmd = (Ar6kWmiChannelParams*)netbufPushTrailer(pPacket, sizeof(Ar6kWmiChannelParams) + num_channels*sizeof(u16));

	cmd->scan_param = scan_param;
	cmd->num_channels = num_channels;

	if (chan_mask) {
		if (chan_mask & chan_mask_g) {
			if (cmd->phy_mode & chan_mask_a) {
				cmd->phy_mode = Ar6kWmiPhyMode_11AG;
			} else {
				cmd->phy_mode = Ar6kWmiPhyMode_11G;
			}
		} else /* if (chan_mask & chan_mask_a) */ {
			cmd->phy_mode = Ar6kWmiPhyMode_11A;
		}

		for (unsigned i = 0, j = 0; j < 23; j ++) {
			if (chan_mask & (1U << j)) {
				cmd->channel_mhz[i++] = wlanChannelToFreq(j);
			}
		}
	}

	return _ar6kWmiSendCmdPacket(dev, Ar6kWmiCmdId_SetChannelParams, pPacket);
}

bool ar6kWmiAddCipherKey(Ar6kDev* dev, Ar6kWmiCipherKey const* key)
{
	NetBuf* pPacket = _ar6kWmiAllocCmdPacket(dev);
	*netbufPushTrailerType(pPacket, Ar6kWmiCipherKey) = *key;
	return _ar6kWmiSendCmdPacket(dev, Ar6kWmiCmdId_AddCipherKey, pPacket);
}

bool ar6kWmiSetFrameRate(Ar6kDev* dev, unsigned ieee_frame_type, unsigned ieee_frame_subtype, unsigned rate_mask)
{
	NetBuf* pPacket = _ar6kWmiAllocCmdPacket(dev);
	Ar6kWmiCmdSetFrameRate* cmd = netbufPushTrailerType(pPacket, Ar6kWmiCmdSetFrameRate);
	cmd->enable_frame_mask = rate_mask ? 1 : 0;
	cmd->frame_type = ieee_frame_type;
	cmd->frame_subtype = ieee_frame_subtype;
	cmd->frame_rate_mask = rate_mask;
	return _ar6kWmiSendCmdPacket(dev, Ar6kWmiCmdId_SetFrameRate, pPacket);
}

bool ar6kWmiSetBitRate(Ar6kDev* dev, Ar6kWmiBitRate data_rate, Ar6kWmiBitRate mgmt_rate, Ar6kWmiBitRate ctrl_rate)
{
	NetBuf* pPacket = _ar6kWmiAllocCmdPacket(dev);
	Ar6kWmiCmdSetBitRate* cmd = netbufPushTrailerType(pPacket, Ar6kWmiCmdSetBitRate);
	cmd->data_rate_idx = data_rate;
	cmd->mgmt_rate_idx = mgmt_rate;
	cmd->ctrl_rate_idx = ctrl_rate;
	return _ar6kWmiSendCmdPacket(dev, Ar6kWmiCmdId_SetBitRate, pPacket);
}

bool ar6kWmixConfigDebugModuleCmd(Ar6kDev* dev, u32 cfgmask, u32 config)
{
	NetBuf* pPacket = _ar6kWmiAllocCmdPacket(dev);
	netbufPushTrailerType(pPacket, Ar6kWmiGeneric32)->value = Ar6kWmixCmdId_DbgLogCfgModule;
	Ar6kWmixDbgLogCfgModule* cmd = netbufPushTrailerType(pPacket, Ar6kWmixDbgLogCfgModule);
	cmd->cfgvalid = cfgmask;
	cmd->dbglog_config = config;
	return _ar6kWmiSendCmdPacket(dev, Ar6kWmiCmdId_Extension, pPacket);
}
