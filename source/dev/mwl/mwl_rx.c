// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#include "common.h"
#include <calico/arm/common.h>
#include <calico/system/thread.h>
#include <calico/nds/dma.h>

#define MWL_RX_STATIC_MEM_SZ 1024

static struct {
	NetBuf hdr;
	u8 body[MWL_RX_STATIC_MEM_SZ];
} s_mwlRxStaticMem;

MK_CONSTEXPR bool _mwlRxCanUseStaticMem(MwlRxType type, unsigned len)
{
	switch (type) {
		default:
			return len <= MWL_RX_STATIC_MEM_SZ;

		case MwlRxType_IeeeMgmtOther:
		case MwlRxType_IeeeCtrl:
		case MwlRxType_IeeeData:
			return false;
	}
}

MK_NOINLINE static void _mwlRxRead(void* dst, unsigned len)
{
	u16* dst16 = (u16*)dst;
	while (len > 1) {
		*dst16++ = MWL_REG(W_RXBUF_RD_DATA);
		len -= 2;
	}
}

MK_NOINLINE static void _mwlRxBeaconFrame(NetBuf* pPacket, MwlDataRxHdr* rxhdr)
{
	// Pop 802.11 header
	MK_ASSUME(pPacket->len >= sizeof(WlanMacHdr));
	WlanMacHdr* dot11hdr = netbufPopHeaderType(pPacket, WlanMacHdr);

	// Save beacon header for later
	WlanBeaconHdr* hdr = (WlanBeaconHdr*)netbufGet(pPacket);

	// Parse beacon body
	WlanBssDesc desc;
	WlanBssExtra extra;
	wlanParseBeacon(&desc, &extra, pPacket);
	__builtin_memcpy(desc.bssid, dot11hdr->tx_addr, 6);
	desc.rssi = mwlDecodeRssi(rxhdr->rssi);

	// Apply contention-free duration if needed
	if (extra.cfp && (dot11hdr->duration & 0x8000)) {
		MWL_REG(W_CONTENTFREE) = wlanDecode16(extra.cfp->dur_remaining);
	}

	// If a BSSID filter is active, ignore non-matching beacons
	if (!(s_mwlState.bssid[0] & 1) && !(rxhdr->status & (1U<<15))) {
		return;
	}

	// Forward BSS description to MLME if we are scanning or joining
	if (s_mwlState.mlme_state == MwlMlmeState_ScanBusy) {
		_mwlMlmeOnBssInfo(&desc, &extra);
	} else if (s_mwlState.mlme_state == MwlMlmeState_JoinBusy) {
		_mwlMlmeHandleJoin(hdr, &desc, &extra);
	}

	// Below section only relevant when we have joined a BSS
	if (!s_mwlState.has_beacon_sync) {
		return;
	}

	// Clear beacon loss counter for obvious reasons
	s_mwlState.beacon_loss_cnt = 0;

	// Calculate timestamp of next TBTT (target beacon transmission time).
	// Note: as per 802.11 spec, this formula accounts for beacons delayed due to
	// medium contention - next beacon is expected to be received at the usual time.
	u32 beaconPeriodUs = hdr->interval << 10;
	u64 nextTbttUs = hdr->timestamp[0] | ((u64)hdr->timestamp[1] << 32);
	nextTbttUs = ((nextTbttUs / beaconPeriodUs) + 1) * beaconPeriodUs;

	// Configure next TBTT interrupt.
	// Note: upon receipt of a beacon matching W_BSSID, the hardware automatically
	// updates our local clock (W_US_COUNT) with that of the BSS. Nice!
	MWL_REG(W_US_COMPARE3) = nextTbttUs >> 48;
	MWL_REG(W_US_COMPARE2) = nextTbttUs >> 32;
	MWL_REG(W_US_COMPARE1) = nextTbttUs >> 16;
	MWL_REG(W_US_COMPARE0) = nextTbttUs | 1;
}

MK_NOINLINE static void _mwlRxProbeResFrame(NetBuf* pPacket, MwlDataRxHdr* rxhdr)
{
	// Ignore this packet if we are not scanning
	if (s_mwlState.mlme_state != MwlMlmeState_ScanBusy) {
		return;
	}

	// Pop 802.11 header
	MK_ASSUME(pPacket->len >= sizeof(WlanMacHdr));
	WlanMacHdr* dot11hdr = netbufPopHeaderType(pPacket, WlanMacHdr);

	dietPrint("PrbRsp BSSID=%.2X:%.2X:%.2X:%.2X:%.2X:%.2X\n",
		dot11hdr->tx_addr[0],dot11hdr->tx_addr[1],dot11hdr->tx_addr[2],
		dot11hdr->tx_addr[3],dot11hdr->tx_addr[4],dot11hdr->tx_addr[5]);

	// Parse probe response body
	WlanBssDesc desc;
	WlanBssExtra extra;
	wlanParseBeacon(&desc, &extra, pPacket);
	__builtin_memcpy(desc.bssid, dot11hdr->tx_addr, 6);
	desc.rssi = mwlDecodeRssi(rxhdr->rssi);

	// Forward BSS description to MLME
	_mwlMlmeOnBssInfo(&desc, &extra);
}

void _mwlRxEndTask(void)
{
	for (;;) {
		// Read current read/write cursors
		unsigned rdcsr = MWL_REG(W_RXBUF_READCSR);
		unsigned wrcsr = s_mwlState.rx_wrcsr;
		armCompilerBarrier(); // avoid variable access reordering

		// If they are the same -> we're done
		if (rdcsr == wrcsr) {
			break;
		}

		// Set up RX transfer
		MWL_REG(W_RXBUF_RD_ADDR) = rdcsr*2;

		// Read hardware header
		MwlDataRxHdr rxhdr;
		_mwlRxRead(&rxhdr, sizeof(rxhdr));

		// Retrieve info from hardware header
		MwlRxType pkt_type = (MwlRxType)(rxhdr.status&0xf);
		unsigned pkt_read_sz = (rxhdr.mpdu_len + 1) &~ 1;

		// Calculate offset to next packet
		unsigned pkt_end = (rdcsr*2 + sizeof(MwlDataRxHdr) + rxhdr.mpdu_len + 3) &~ 3;
		pkt_end = pkt_end < MWL_MAC_RAM_SZ ? pkt_end : (pkt_end - (MWL_MAC_RAM_SZ-s_mwlState.rx_pos));

		// Decide whether to use static packet memory
		bool static_rx = _mwlRxCanUseStaticMem(pkt_type, pkt_read_sz);

		NetBuf* pPacket;
		if_likely (static_rx) {
			// Set up static memory
			pPacket = &s_mwlRxStaticMem.hdr;
			pPacket->capacity = MWL_RX_STATIC_MEM_SZ;
			pPacket->pos = 0;
			pPacket->len = pkt_read_sz;
		} else {
			// Allocate packet memory
			while (!(pPacket = netbufAlloc(0, pkt_read_sz, NetBufPool_Rx))) {
				// Try again after a little while
				threadSleep(1000);
			}
		}

		// Slurp packet into our memory
		_mwlRxRead(netbufGet(pPacket), pkt_read_sz);

		// Update read cursor
		MWL_REG(W_RXBUF_READCSR) = pkt_end/2;

		// Get pointer to 802.11 header
		WlanMacHdr* dot11hdr = (WlanMacHdr*)netbufGet(pPacket);

		if (rxhdr.status & (1U<<9)) {
			// TODO: is this worth supporting?
			dietPrint("[RX] fragmented packet!\n");
		} else if (pkt_type != MwlRxType_IeeeCtrl && pPacket->len < sizeof(WlanMacHdr)) {
			// Shouldn't happen, but just in case.
			dietPrint("[RX] missing 802.11 hdr!\n");
		} else switch (pkt_type) {
			default: {
				dietPrint("[RX:%02X] t=%X len=%u ieee=%.4X\n", mwlDecodeRssi(rxhdr.rssi), pkt_type, rxhdr.mpdu_len, dot11hdr->fc.value);
				break;
			}

			case MwlRxType_IeeeMgmtOther: {
				if (dot11hdr->fc.type == WlanFrameType_Management) {
					if (dot11hdr->fc.subtype == WlanMgmtType_ProbeResp) {
						_mwlRxProbeResFrame(pPacket, &rxhdr);
					} else {
						// Handle other management frames in a separate task
						netbufQueueAppend(&s_mwlState.rx_mgmt, pPacket);
						pPacket = NULL;
						_mwlPushTask(MwlTask_RxMgmtCtrlFrame);
					}
				}
				break;
			}

			case MwlRxType_IeeeBeacon: {
				if (dot11hdr->fc.type == WlanFrameType_Management && dot11hdr->fc.subtype == WlanMgmtType_Beacon) {
					_mwlRxBeaconFrame(pPacket, &rxhdr);
				}
				break;
			}

			case MwlRxType_IeeeData: {
				if (dot11hdr->fc.type == WlanFrameType_Data && !(dot11hdr->fc.subtype & WLAN_DATA_IS_NULL) && s_mwlState.status == MwlStatus_Class3) {
					// Handle data frames in a separate task
					pPacket->user[0] = mwlDecodeRssi(rxhdr.rssi);
					netbufQueueAppend(&s_mwlState.rx_data, pPacket);
					pPacket = NULL;
					_mwlPushTask(MwlTask_RxDataFrame);
				}
				break;
			}
		}

		// Free packet if necessary
		if (!static_rx && pPacket) {
			netbufFree(pPacket);
		}
	}
}

void _mwlRxMgmtCtrlTask(void)
{
	// Dequeue packet
	NetBuf* pPacket = netbufQueueRemoveOne(&s_mwlState.rx_mgmt);
	if (!pPacket) {
		return; // Shouldn't happen
	}

	// Pop 802.11 header
	MK_ASSUME(pPacket->len >= sizeof(WlanMacHdr));
	WlanMacHdr* dot11hdr = netbufPopHeaderType(pPacket, WlanMacHdr);

	switch (dot11hdr->fc.subtype) {
		default: {
			dietPrint("[RX] MGMT type %u\n", dot11hdr->fc.subtype);
			break;
		}

		case WlanMgmtType_AssocResp: {
			if (s_mwlState.mlme_state == MwlMlmeState_AssocBusy) {
				_mwlMlmeHandleAssocResp(pPacket);
			}
			break;
		}

		case WlanMgmtType_Disassoc: {
			if (s_mwlState.status >= MwlStatus_Class3) {
				_mwlMlmeHandleStateLoss(pPacket, dot11hdr->fc.subtype);
			}
			break;
		}

		case WlanMgmtType_Auth: {
			if (s_mwlState.mlme_state == MwlMlmeState_AuthBusy) {
				_mwlMlmeHandleAuth(pPacket);
			}
			break;
		}

		case WlanMgmtType_Deauth: {
			if (s_mwlState.status >= MwlStatus_Class2) {
				_mwlMlmeHandleStateLoss(pPacket, dot11hdr->fc.subtype);
			}
			break;
		}
	}

	// Free this packet and refire this task if there are more
	netbufFree(pPacket);
	if (s_mwlState.rx_mgmt.next) {
		_mwlPushTask(MwlTask_RxMgmtCtrlFrame);
	}
}

void _mwlRxDataTask(void)
{
	// Dequeue packet
	NetBuf* pPacket = netbufQueueRemoveOne(&s_mwlState.rx_data);
	if (!pPacket) {
		return; // Shouldn't happen
	}

	// Forward to callback
	if (s_mwlState.mlme_cb.maData) {
		s_mwlState.mlme_cb.maData(pPacket);
	} else {
		netbufFree(pPacket);
	}

	// Refire this task if there are more
	if (s_mwlState.rx_data.next) {
		_mwlPushTask(MwlTask_RxDataFrame);
	}
}

void _mwlRxQueueClear(void)
{
	NetBuf* pPacket;

	while ((pPacket = netbufQueueRemoveOne(&s_mwlState.rx_mgmt))) {
		netbufFree(pPacket);
	}

	while ((pPacket = netbufQueueRemoveOne(&s_mwlState.rx_data))) {
		netbufFree(pPacket);
	}
}
