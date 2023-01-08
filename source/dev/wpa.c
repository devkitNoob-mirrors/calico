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
	WpaEapolHdr* replyeapol;
	WpaEapolKeyHdr* reply;
	void* key_data;
	unsigned key_data_len;
	u16 key_info;
	bool is_rsn;
	u8 descr_ver;
} WpaWork;

void wpaPseudoRandomFunction(void* out, size_t out_len, const void* key, size_t key_size, void* pad, size_t pad_len)
{
	u8 temp[WPA_SHA1_LEN];
	u8* pCounter = (u8*)pad + pad_len - 1;

	*pCounter = 0;
	while (out_len) {
		size_t cur_sz = out_len > sizeof(temp) ? sizeof(temp) : out_len;
		out_len -= cur_sz;

		wpaHmacSha1(temp, key, key_size, pad, pad_len);
		(*pCounter)++;

		memcpy(out, temp, cur_sz);
		out = (u8*)out + cur_sz;
	}
}

static void _wpaEapolCalcMic(WpaState* st, WpaWork* wk, WpaEapolHdr* eapolhdr)
{
	WpaEapolKeyHdr* hdr = (WpaEapolKeyHdr*)(eapolhdr+1);
	size_t totalsz = sizeof(WpaEapolHdr) + sizeof(WpaEapolKeyHdr) + __builtin_bswap16(hdr->key_data_len_be);
	memset(hdr->mic, 0, WPA_EAPOL_MIC_LEN);

	if (wk->descr_ver == 2) {
		u8 temp[WPA_SHA1_LEN];
		wpaHmacSha1(temp, st->ptk.kck, sizeof(st->ptk.kck), eapolhdr, totalsz);
		memcpy(hdr->mic, temp, WPA_EAPOL_MIC_LEN);
	} else {
		// XX: same thing but with HMAC-MD5
	}
}

MEOW_NOINLINE static NetBuf* _wpaEapolCreateReplyPacket(WpaState* st, WpaWork* wk, size_t keydatalen, unsigned info)
{
	NetBuf* pPacket = st->cb_alloc_packet(st, sizeof(WpaEapolHdr) + sizeof(WpaEapolKeyHdr) + keydatalen);
	if (!pPacket) {
		dietPrint("[WPA] Bad packet alloc\n");
		return NULL;
	}

	// Fill in EAPOL header
	WpaEapolHdr* eapolhdr = netbufGet(pPacket);
	eapolhdr->protocol_version = wk->eapolhdr->protocol_version;
	eapolhdr->packet_type = wk->eapolhdr->packet_type;
	eapolhdr->packet_body_len_be = __builtin_bswap16(pPacket->len - sizeof(WpaEapolHdr));
	wk->replyeapol = eapolhdr;

	// Fill in EAPOL-Key header
	WpaEapolKeyHdr* hdr = (WpaEapolKeyHdr*)(eapolhdr+1);
	memset(hdr, 0, sizeof(*hdr));
	hdr->descr_type = wk->hdr->descr_type;
	hdr->key_info_be = __builtin_bswap16(info);
	memcpy(hdr->key_replay_cnt, st->replay, WPA_EAPOL_REPLAY_LEN);
	hdr->key_data_len_be = __builtin_bswap16(keydatalen);
	wk->reply = hdr;

	// Prepend MAC header
	MEOW_ASSUME(pPacket->pos >= sizeof(NetMacHdr));
	NetMacHdr* machdr = netbufPushHeaderType(pPacket, NetMacHdr);
	memcpy(machdr->dst_mac, wk->machdr->src_mac, 6);
	memcpy(machdr->src_mac, wk->machdr->dst_mac, 6);
	machdr->len_or_ethertype_be = wk->machdr->len_or_ethertype_be;

	return pPacket;
}

MEOW_NOINLINE static void _wpaPairwiseHandshake12(WpaState* st, WpaWork* wk)
{
	dietPrint("[WPA] Handshake 1/4\n");

	// Update key replay counter (TODO: validate)
	memcpy(st->replay, wk->hdr->key_replay_cnt, WPA_EAPOL_REPLAY_LEN);

	// Generate a nonce for ourselves
	u8 snonce[WPA_EAPOL_NONCE_LEN] = "TEMP key please use random one";

	// Initialize the pad used to generate the PTK
	WpaPtkPad pad;
	memcpy(pad.magic, "Pairwise key expansion", sizeof(pad.magic));
	pad.zero = 0;

	// Copy the BSSIDs
	if (memcmp(wk->machdr->dst_mac, wk->machdr->src_mac, 6) < 0) {
		memcpy(pad.min_bssid, wk->machdr->dst_mac, 6);
		memcpy(pad.max_bssid, wk->machdr->src_mac, 6);
	} else {
		memcpy(pad.min_bssid, wk->machdr->src_mac, 6);
		memcpy(pad.max_bssid, wk->machdr->dst_mac, 6);
	}

	// Copy the nonces
	if (memcmp(snonce, wk->hdr->key_nonce, WPA_EAPOL_NONCE_LEN) < 0) {
		memcpy(pad.min_nonce, snonce, WPA_EAPOL_NONCE_LEN);
		memcpy(pad.max_nonce, wk->hdr->key_nonce, WPA_EAPOL_NONCE_LEN);
	} else {
		memcpy(pad.min_nonce, wk->hdr->key_nonce, WPA_EAPOL_NONCE_LEN);
		memcpy(pad.max_nonce, snonce, WPA_EAPOL_NONCE_LEN);
	}

	// Calculate the PTK
	wpaPseudoRandomFunction(&st->ptk, sizeof(st->ptk), st->pmk, sizeof(st->pmk), &pad, sizeof(pad));

	// Prepare packet 2
	unsigned info =
		wk->descr_ver |
		WPA_EAPOL_KEY_MIC |
		(wk->key_info & (WPA_EAPOL_KEY_TYPE_MASK|WPA_EAPOL_SECURE|WPA_EAPOL_ERROR|WPA_EAPOL_REQUEST));
	NetBuf* pPacket = _wpaEapolCreateReplyPacket(st, wk, st->ie_len, info);
	if (!pPacket) {
		return;
	}

	// Copy data to packet 2 and calc MIC
	WpaEapolKeyHdr* hdr = wk->reply;
	memcpy(hdr->key_nonce, snonce, WPA_EAPOL_NONCE_LEN);
	memcpy(hdr+1, st->ie_data, st->ie_len);
	_wpaEapolCalcMic(st, wk, wk->replyeapol);

	// Send packet 2
	st->cb_tx(st, pPacket);
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

	/*
	if (wk.is_rsn) {
		dietPrint("[WPA] Using WPA2/RSN\n");
	}
	*/

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
		// Verify MIC
		u8 micbkup[WPA_EAPOL_MIC_LEN];
		memcpy(micbkup, wk.hdr->mic, WPA_EAPOL_MIC_LEN);
		_wpaEapolCalcMic(st, &wk, wk.eapolhdr);
		if (memcmp(micbkup, wk.hdr->mic, WPA_EAPOL_MIC_LEN) != 0) {
			dietPrint("[WPA] Invalid EAPOL MIC\n");
			return;
		}
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
