#include "common.h"

bool mwlDevWlanToDix(NetBuf* pPacket)
{
	WlanMacHdr* dot11hdr = netbufPopHeaderType(pPacket, WlanMacHdr);
	if (!dot11hdr) {
		return false; // shouldn't happen
	}

	NetLlcSnapHdr* llcsnap = netbufPopHeaderType(pPacket, NetLlcSnapHdr);
	if (!llcsnap) {
		return false; // shouldn't happen
	}

	NetMacHdr dix;
	dix.len_or_ethertype_be = llcsnap->ethertype_be;

	if (dot11hdr->fc.to_ds) {
		__builtin_memcpy(dix.dst_mac, dot11hdr->xtra_addr, 6);
	} else {
		__builtin_memcpy(dix.dst_mac, dot11hdr->rx_addr, 6);
	}

	if (dot11hdr->fc.from_ds) {
		__builtin_memcpy(dix.src_mac, dot11hdr->xtra_addr, 6);
	} else {
		__builtin_memcpy(dix.src_mac, dot11hdr->tx_addr, 6);
	}

	*netbufPushHeaderType(pPacket, NetMacHdr) = dix;
	return true;
}

bool mwlDevDixToWlan(NetBuf* pPacket)
{
	NetMacHdr* pDix = netbufPopHeaderType(pPacket, NetMacHdr);
	if (!pDix) {
		return false; // shouldn't happen
	}

	NetMacHdr dix = *pDix;
	NetLlcSnapHdr* llcsnap = netbufPushHeaderType(pPacket, NetLlcSnapHdr);
	if (!llcsnap) {
		return false; // shouldn't happen
	}

	// Fill in LLC-SNAP header
	llcsnap->dsap = 0xaa;
	llcsnap->ssap = 0xaa;
	llcsnap->control = 0x03;
	llcsnap->oui[0] = 0;
	llcsnap->oui[1] = 0;
	llcsnap->oui[2] = 0;
	llcsnap->ethertype_be = dix.len_or_ethertype_be;

	WlanMacHdr* dot11hdr = netbufPushHeaderType(pPacket, WlanMacHdr);
	if (!dot11hdr) {
		return false; // shouldn't happen
	}

	// Fill in 802.11 header
	dot11hdr->fc.value = 0;
	dot11hdr->fc.type = WlanFrameType_Data;
	dot11hdr->fc.to_ds = 1;
	dot11hdr->fc.wep = s_mwlState.wep_enabled;
	dot11hdr->duration = 0;
	__builtin_memcpy(dot11hdr->rx_addr, s_mwlState.bssid, 6);
	__builtin_memcpy(dot11hdr->tx_addr, dix.src_mac, 6);
	__builtin_memcpy(dot11hdr->xtra_addr, dix.dst_mac, 6);
	dot11hdr->sc.value = 0;

	return true;
}
