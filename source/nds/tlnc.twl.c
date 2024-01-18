#include <calico/nds/system.h>
#include <calico/nds/bios.h>
#include <calico/nds/tlnc.h>
#ifdef ARM9
#include <calico/arm/cache.h>
#endif

#define g_tlncArea ((TlncArea*)MM_ENV_TWL_AUTOLOAD)

bool tlncGetDataTWL(TlncData* data)
{
	// Check if we can expect the TLNC to exist
	if (!g_envTwlResetFlags->is_valid || !g_envTwlResetFlags->is_warmboot || g_envTwlResetFlags->skip_tlnc) {
		return false;
	}

#ifdef ARM9
	// Invalidate data cache before accessing TLNC
	armDCacheInvalidate(g_tlncArea, sizeof(*g_tlncArea));
#endif

	// Check if the TLNC has a valid header
	if (g_tlncArea->magic != TLNC_MAGIC || g_tlncArea->version != TLNC_VERSION || g_tlncArea->data_len != sizeof(*data)) {
		return false;
	}

	// Check if the TLNC data checksum is correct
	*data = g_tlncArea->data;
	if (svcGetCRC16(0xffff, data, sizeof(*data)) != g_tlncArea->data_crc16) {
		*data = (TlncData){0};
		return false;
	}

	return true;
}

void tlncSetDataTWL(TlncData const* data)
{
	g_tlncArea->magic = TLNC_MAGIC;
	g_tlncArea->version = TLNC_VERSION;
	g_tlncArea->data_len = sizeof(*data);
	g_tlncArea->data_crc16 = svcGetCRC16(0xffff, data, sizeof(*data));
	g_tlncArea->data = *data;

#ifdef ARM9
	// Flush data cache after writing TLNC
	armDCacheFlush(g_tlncArea, sizeof(*g_tlncArea));
#endif
}
