#include <calico/types.h>
#include <calico/dev/wpa.h>
#include <string.h>

#define WPA_DEBUG

#ifdef WPA_DEBUG
#include <calico/system/dietprint.h>
#else
#define dietPrint(...) ((void)0)
#endif

typedef struct WpaWork {
	NetMacHdr* machdr;
	WpaEapolHdr* eapolhdr;
	WpaEapolKeyHdr* hdr;
	void* key_data;
	unsigned key_data_len;
	u16 key_info;
	bool is_rsn;
	u8 descr_ver;
} WpaWork;

MEOW_NOINLINE static void _wpaPairwiseHandshake12(WpaState* st, WpaWork* wk)
{
	dietPrint("[WPA] Handshake 1/4\n");
}

MEOW_NOINLINE static void _wpaPairwiseHandshake34(WpaState* st, WpaWork* wk)
{
	dietPrint("[WPA] Handshake 3/4\n");
}

MEOW_NOINLINE static void _wpaGroupHandshake(WpaState* st, WpaWork* wk)
{
	dietPrint("[WPA] Group renew 1/2\n");
}

static void _wpaEapolRx(WpaState* st, NetBuf* pPacket)
{
	WpaWork wk;

	wk.machdr = netbufPopHeaderType(pPacket, NetMacHdr);
	if (!wk.machdr) {
		dietPrint("[WPA] Missing MAC header\n");
		return;
	}

	wk.eapolhdr = netbufPopHeaderType(pPacket, WpaEapolHdr);
	if (!wk.eapolhdr) {
		dietPrint("[WPA] Missing EAPOL header\n");
		return;
	}

	// XX: validate EAPOL version/packet type?

	if (pPacket->len < __builtin_bswap16(wk.eapolhdr->packet_body_len_be)) {
		dietPrint("[WPA] Truncated packet\n");
		return;
	}

	wk.hdr = netbufPopHeaderType(pPacket, WpaEapolKeyHdr);
	if (!wk.hdr) {
		dietPrint("[WPA] Missing EAPOL-Key header\n");
		return;
	}

	switch (wk.hdr->descr_type) {
		default:
			dietPrint("[WPA] Bad EAPOL-Key descr %.2X\n", wk.hdr->descr_type);
			return;

		case WpaEapolDescrType_RSN: // aka WPA2
			wk.is_rsn = true;
			break;

		case WpaEapolDescrType_WPA:
			wk.is_rsn = false;
			break;
	}

	if (wk.is_rsn) {
		dietPrint("[WPA] Using WPA2/RSN\n");
	}

	wk.key_info = __builtin_bswap16(wk.hdr->key_info_be);
	wk.descr_ver = WPA_EAPOL_DESCR_VER(wk.key_info);
	wk.key_data_len = __builtin_bswap16(wk.hdr->key_data_len_be);

	if (wk.descr_ver != 1 && wk.descr_ver != 2) {
		dietPrint("[WPA] Bad EAPOL-Key ver %u\n", wk.descr_ver);
		return;
	}

	if (!(wk.key_info & WPA_EAPOL_KEY_ACK)) {
		dietPrint("[WPA] Bad EAPOL-Key direction\n");
		return;
	}

	if (pPacket->len < wk.key_data_len) {
		dietPrint("[WPA] Truncated packet\n");
		return;
	}

	if (wk.key_info & WPA_EAPOL_KEY_MIC) {
		// TODO: Verify MIC
		dietPrint("[WPA] TODO: verify MIC\n");
	}

	if (wk.key_info & WPA_EAPOL_ENCRYPTED) {
		// TODO: Decrypt keydata
		// XX: WPA does not use this bit, instead it is assumed to be always true?
		dietPrint("[WPA] TODO: decrypt keydata\n");
		wk.key_data = netbufGet(pPacket);
	}

	switch (wk.key_info & (WPA_EAPOL_KEY_TYPE_MASK|WPA_EAPOL_KEY_MIC)) {
		default: {
			dietPrint("[WPA] Bad EAPOL-Key state %.4X\n", wk.key_info);
			break;
		}

		case WPA_EAPOL_KEY_TYPE_PAIRWISE: {
			_wpaPairwiseHandshake12(st, &wk);
			break;
		}

		case WPA_EAPOL_KEY_TYPE_PAIRWISE | WPA_EAPOL_KEY_MIC: {
			_wpaPairwiseHandshake34(st, &wk);
			break;
		}

		case WPA_EAPOL_KEY_TYPE_GROUP | WPA_EAPOL_KEY_MIC: {
			_wpaGroupHandshake(st, &wk);
			break;
		}
	}
}

void wpaPrepare(WpaState* st)
{
	*st = (WpaState){0};
	mailboxPrepare(&st->mbox, &st->mbox_storage, 1);
}

int wpaSupplicantThreadMain(WpaState* st)
{
	for (;;) {
		NetBuf* pPacket = (NetBuf*)mailboxRecv(&st->mbox);
		if (!pPacket) {
			break;
		}

		_wpaEapolRx(st, pPacket);
	}

	return 0;
}
