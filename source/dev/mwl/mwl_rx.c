#include "common.h"
#include <calico/arm/common.h>
#include <calico/system/thread.h>
#include <calico/nds/dma.h>

#define MWL_RX_STATIC_MEM_SZ 1024

static struct {
	NetBuf hdr;
	u8 body[MWL_RX_STATIC_MEM_SZ];
} s_mwlRxStaticMem;

MEOW_CONSTEXPR bool _mwlRxCanUseStaticMem(MwlRxType type, unsigned len)
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

MEOW_NOINLINE static void _mwlRxRead(void* dst, unsigned len)
{
	u16* dst16 = (u16*)dst;
	while (len > 1) {
		*dst16++ = MWL_REG(W_RXBUF_RD_DATA);
		len -= 2;
	}
}

MEOW_NOINLINE static void _mwlRxBeaconFrame(NetBuf* pPacket, MwlDataRxHdr* rxhdr)
{
	// Pop 802.11 header
	MEOW_ASSUME(pPacket->len >= sizeof(WlanMacHdr));
	WlanMacHdr* dot11hdr = netbufPopHeaderType(pPacket, WlanMacHdr);

	// Parse beacon body
	WlanBssDesc desc;
	WlanBssExtra extra;
	wlanParseBeacon(&desc, &extra, pPacket);
	__builtin_memcpy(desc.bssid, dot11hdr->tx_addr, 6);

	// Apply contention-free duration if needed
	if (extra.cfp && (dot11hdr->duration & 0x8000)) {
		MWL_REG(W_CONTENTFREE) = wlanDecode16(extra.cfp->dur_remaining);
	}

	// If a BSSID filter is active, ignore non-matching beacons
	if (!(s_mwlState.bssid[0] & 1) && !(rxhdr->status & (1U<<15))) {
		return;
	}

	// Forward BSS description to MLME if we are scanning
	if (s_mwlState.mlme_state == MwlMlmeState_ScanBusy) {
		_mwlMlmeOnBssInfo(&desc, &extra, rxhdr->rssi & 0xff);
	}
}

MEOW_NOINLINE static void _mwlRxProbeResFrame(NetBuf* pPacket, MwlDataRxHdr* rxhdr)
{
	// Ignore this packet if we are not scanning
	if (s_mwlState.mlme_state != MwlMlmeState_ScanBusy) {
		return;
	}

	// Pop 802.11 header
	MEOW_ASSUME(pPacket->len >= sizeof(WlanMacHdr));
	WlanMacHdr* dot11hdr = netbufPopHeaderType(pPacket, WlanMacHdr);

	dietPrint("PrbRsp BSSID=%.2X:%.2X:%.2X:%.2X:%.2X:%.2X\n",
		dot11hdr->tx_addr[0],dot11hdr->tx_addr[1],dot11hdr->tx_addr[2],
		dot11hdr->tx_addr[3],dot11hdr->tx_addr[4],dot11hdr->tx_addr[5]);

	// Parse probe response body
	WlanBssDesc desc;
	WlanBssExtra extra;
	wlanParseBeacon(&desc, &extra, pPacket);
	__builtin_memcpy(desc.bssid, dot11hdr->tx_addr, 6);

	// Forward BSS description to MLME
	_mwlMlmeOnBssInfo(&desc, &extra, rxhdr->rssi & 0xff);
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
				dietPrint("[RX:%02X] t=%X len=%u ieee=%.4X\n", rxhdr.rssi&0xff, pkt_type, rxhdr.mpdu_len, dot11hdr->fc.value);
				break;
			}

			case MwlRxType_IeeeMgmtOther: {
				if (dot11hdr->fc.type == WlanFrameType_Management && dot11hdr->fc.subtype == WlanMgmtType_ProbeResp) {
					_mwlRxProbeResFrame(pPacket, &rxhdr);
				}
				// XX: Handle other management frames in a separate task
				break;
			}

			case MwlRxType_IeeeBeacon: {
				if (dot11hdr->fc.type == WlanFrameType_Management && dot11hdr->fc.subtype == WlanMgmtType_Beacon) {
					_mwlRxBeaconFrame(pPacket, &rxhdr);
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
