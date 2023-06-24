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

MEOW_NOINLINE static void _mwlRxBeaconFrame(NetBuf* pPacket, unsigned rssi)
{
	WlanMacHdr* dot11hdr = netbufPopHeaderType(pPacket, WlanMacHdr);
	if (!dot11hdr) {
		dietPrint("[MWL] RX bad beacon\n");
		return;
	}

	// Parse beacon body
	WlanBssDesc desc;
	WlanBssExtra extra;
	wlanParseBeacon(&desc, &extra, pPacket);
	__builtin_memcpy(desc.bssid, dot11hdr->tx_addr, 6);

	// Apply contention-free duration if needed
	if (extra.cfp && (dot11hdr->duration & 0x8000)) {
		MWL_REG(W_CONTENTFREE) = wlanDecode16(extra.cfp->dur_remaining);
	}

	if (s_mwlState.mlme_state == MwlMlmeState_ScanBusy && s_mwlState.mlme_cb.onBssInfo) {
		// Apply SSID filter if necessary
		unsigned filter_len = s_mwlState.mlme.scan.filter.target_ssid_len;
		if (filter_len == 0 || (filter_len == desc.ssid_len &&
			__builtin_memcmp(desc.ssid, s_mwlState.mlme.scan.filter.target_ssid, filter_len) == 0)) {
			s_mwlState.mlme_cb.onBssInfo(&desc, &extra, rssi);
		}
	}
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

		if (rxhdr.status & (1U<<9)) {
			// TODO: is this worth supporting?
			dietPrint("[RX] fragmented packet!\n");
		} else switch (pkt_type) {
			default:
				dietPrint("[RX:%02X] t=%X len=%u ieee=%.4X\n", rxhdr.rssi&0xff, pkt_type, rxhdr.mpdu_len, *(u16*)netbufGet(pPacket));
				break;

			case MwlRxType_IeeeBeacon:
				_mwlRxBeaconFrame(pPacket, rxhdr.rssi&0xff);
				break;
		}

		// Free packet if necessary
		if (!static_rx && pPacket) {
			netbufFree(pPacket);
		}
	}
}
