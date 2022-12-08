#include <calico/types.h>
#include <calico/nds/mm.h>
#include <calico/nds/mm_env.h>
#include <calico/nds/env.h>

#define s_debugBuf ((DebugBuffer*) MM_ENV_FREE_D000)

#define s_transferRegion ((TransferRegion*) MM_ENV_FREE_FF60)
#if defined(ARM9)
#define s_pxiLocalPxiMask  s_transferRegion->arm9_pxi_mask
#define s_pxiRemotePxiMask s_transferRegion->arm7_pxi_mask
#elif defined(ARM7)
#define s_pxiLocalPxiMask  s_transferRegion->arm7_pxi_mask
#define s_pxiRemotePxiMask s_transferRegion->arm9_pxi_mask
#else
#error "Must be ARM9 or ARM7"
#endif

#define DBG_BUF_ALIVE (1U << 0)
#define DBG_BUF_BUSY  (1U << 1)

typedef struct DebugBuffer {
	vu16 flags;
	u16  size;
	char data[MM_ENV_FREE_D000_SZ - 4];
} DebugBuffer;

typedef struct TransferRegion {
	u32 arm9_pxi_mask;
	u32 arm7_pxi_mask;
} TransferRegion;
