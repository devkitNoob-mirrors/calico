#include <calico/types.h>
#include <calico/dev/wpa.h>
#include <calico/nds/bios.h>

void wpaHmacSha1(void* out, const void* key, size_t key_len, const void* data, size_t data_len)
{
	SvcSha1Context ctx;
	ctx.hash_block = NULL;

	u8 keyblock[64];
	const u8* keyp = (const u8*)key;

	// Clamp down key length when larger than block size
	if (key_len > sizeof(keyblock)) {
		svcSha1CalcTWL(keyblock, key, key_len);
		keyp = keyblock;
		key_len = SVC_SHA1_DIGEST_SZ;
	}

	// Copy key (zeropadded) and XOR it with 0x36
	size_t i;
	for (i = 0; i < key_len; i ++) {
		keyblock[i] = keyp[i] ^ 0x36;
	}
	for (; i < sizeof(keyblock); i ++) {
		keyblock[i] = 0x36;
	}

	// Inner hash
	svcSha1InitTWL(&ctx);
	svcSha1UpdateTWL(&ctx, keyblock, sizeof(keyblock));
	svcSha1UpdateTWL(&ctx, data, data_len);
	svcSha1DigestTWL(out, &ctx);

	// Convert inner key into outer key
	for (i = 0; i < sizeof(keyblock); i ++) {
		keyblock[i] ^= 0x36 ^ 0x5c;
	}

	// Outer hash
	svcSha1InitTWL(&ctx);
	svcSha1UpdateTWL(&ctx, keyblock, sizeof(keyblock));
	svcSha1UpdateTWL(&ctx, out, SVC_SHA1_DIGEST_SZ);
	svcSha1DigestTWL(out, &ctx);
}
