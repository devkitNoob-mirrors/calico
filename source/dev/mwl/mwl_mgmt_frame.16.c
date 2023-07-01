#include "common.h"

#define MWL_MGMT_MAX_SZ 0x100

MEOW_INLINE NetBuf* _mwlMgmtAllocPacket(void)
{
	NetBuf* pPacket = netbufAlloc(0, MWL_MGMT_MAX_SZ, NetBufPool_Tx);
	if (pPacket) {
		pPacket->pos = 0;
		pPacket->len = 0;
	}
	return pPacket;
}

MEOW_INLINE WlanMacHdr* _mwlMgmtInitHeader(NetBuf* pPacket, WlanMgmtType subtype, void** pos)
{
	WlanMacHdr* hdr = (WlanMacHdr*)netbufGet(pPacket);
	__builtin_memset(hdr, 0, sizeof(*hdr));

	hdr->fc.type = WlanFrameType_Management;
	hdr->fc.subtype = subtype;

	__builtin_memcpy(hdr->tx_addr, mwlGetCalibData()->mac_addr, 6);
	__builtin_memcpy(hdr->xtra_addr, s_mwlState.bssid, 6);

	*pos = hdr+1;
	return hdr;
}

MEOW_INLINE void* _mwlMgmtAddData(void** pos, unsigned len)
{
	void* ret = *pos;
	*pos = (u8*)ret + len;
	return ret;
}

MEOW_INLINE void* _mwlMgmtAddIe(void** pos, WlanEid id, unsigned len)
{
	WlanIeHdr* ie = (WlanIeHdr*)*pos;
	ie->id = id;
	ie->len = len;
	void* ret = ie+1;
	*pos = (u8*)ret + len;
	return ret;
}

MEOW_INLINE NetBuf* _mwlMgmtFinalize(NetBuf* pPacket, void* pos)
{
	pPacket->len = pos - netbufGet(pPacket);
	return pPacket;
}

NetBuf* _mwlMgmtMakeProbeReq(const void* bssid, const char* ssid, unsigned ssid_len)
{
	void* wrpos;
	NetBuf* pPacket = _mwlMgmtAllocPacket();
	if (!pPacket) {
		return NULL;
	}

	// Add IEEE 802.11 header
	WlanMacHdr* hdr = _mwlMgmtInitHeader(pPacket, WlanMgmtType_ProbeReq, &wrpos);
	__builtin_memcpy(hdr->rx_addr, bssid, 6);

	// Add SSID element
	__builtin_memcpy(_mwlMgmtAddIe(&wrpos, WlanEid_SSID, ssid_len), ssid, ssid_len);

	// Add supported rates element
	// XX: As it turns out, the Mitsumi does not support DSSS/CCK rates, which is
	// in violation of 802.11b spec. While this is not a problem for local DS comms
	// (and in fact those only advertise 1mbps/2mbps rates in beacons), it requires
	// us to lie to access points about the rates we support. Despite this blatant
	// lie, the DS is clearly capable of connecting to WLANs and operating normally.
	// There may be two different explanations why this is the case:
	// - The Mitsumi actually supports DSSS/CCK, but only in RX (definitely not TX).
	// - APs initially try CCK rates and eventually fall back to Barker rates.
	// In addition, it has been observed that (despite what 802.11 spec mandates)
	// access points (including official code acting as multiplayer host) typically
	// ignore the selected basic rates in probe requests sent by STAs. For this reason
	// we decide to always lie about supporting the missing DSSS/CCK rates.
	u8* rates = (u8*)_mwlMgmtAddIe(&wrpos, WlanEid_SupportedRates, 4);
	rates[0] = 2  | 0x80; // 1mbps   basic (DSSS/Barker)
	rates[1] = 4  | 0x80; // 2mbps   basic (DSSS/Barker)
	rates[2] = 11 | 0x80; // 5.5mbps basic (DSSS/CCK - lie!)
	rates[3] = 22 | 0x80; // 11mbps  basic (DSSS/CCK - lie!)

	return _mwlMgmtFinalize(pPacket, wrpos);
}

NetBuf* _mwlMgmtMakeAuth(const void* target, WlanAuthHdr const* auth_hdr, const void* chal_text, unsigned chal_len)
{
	void* wrpos;
	NetBuf* pPacket = _mwlMgmtAllocPacket();
	if (!pPacket) {
		return NULL;
	}

	// Add IEEE 802.11 header
	WlanMacHdr* hdr = _mwlMgmtInitHeader(pPacket, WlanMgmtType_Auth, &wrpos);
	__builtin_memcpy(hdr->rx_addr, target, 6);

	// Add authentication header
	__builtin_memcpy(_mwlMgmtAddData(&wrpos, sizeof(*auth_hdr)), auth_hdr, sizeof(*auth_hdr));

	// Add challenge text if needed
	if (chal_len) {
		__builtin_memcpy(_mwlMgmtAddIe(&wrpos, WlanEid_ChallengeText, chal_len), chal_text, chal_len);

		// Enable WEP for challenge responses
		if (auth_hdr->sequence_num == 3) {
			hdr->fc.wep = 1;
		}
	}

	return _mwlMgmtFinalize(pPacket, wrpos);
}
