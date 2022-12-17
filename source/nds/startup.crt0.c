#include <calico/types.h>
#if defined(ARM9)
#include <calico/arm/cache.h>
#endif
#include <calico/nds/mm.h>
#include <calico/nds/irq.h>
#include <calico/nds/env.h>

bool g_isTwlMode;

extern void *fake_heap_start, *fake_heap_end;

void __libc_init_array(void);
void _threadInit(void);
void _pxiInit(void);

#if defined(ARM9)

void crt0SetupMPU(bool is_twl);

extern char __heap_start_ntr[], __heap_start_twl[];

#elif defined(ARM7)

void build_argv(EnvNdsArgvHeader*);

extern char __heap_start[], __heap_end[];

#endif

void crt0CopyMem32(uptr dst, uptr src, uptr size);
void crt0FillMem32(uptr dst, u32 value, uptr size);

typedef struct Crt0LoadListEntry {
	uptr start;
	uptr end;
	uptr bss_end;
} Crt0LoadListEntry;

typedef struct Crt0LoadList {
	uptr lma;
	Crt0LoadListEntry const* start;
	Crt0LoadListEntry const* end;
} Crt0LoadList;

typedef struct Crt0Header {
	u32 magic;
	Crt0LoadList ll_ntr;
	Crt0LoadList ll_twl;
} Crt0Header;

MEOW_NOINLINE void crt0ProcessLoadList(Crt0LoadList const* ll)
{
	uptr lma = ll->lma;
	for (Crt0LoadListEntry const* ent = ll->start; ent != ll->end; ent ++) {
		uptr size = ent->end - ent->start;
		uptr bss_start = ent->end;
		uptr bss_size = ent->bss_end - bss_start;
		if (size != 0) {
			crt0CopyMem32(ent->start, lma, size);
			lma += size;
		}
		if (bss_size != 0) {
			crt0FillMem32(bss_start, 0, bss_size);
		}
	}
}

#if defined(ARM7)

static void crt0SetupArgv(bool is_twl)
{
	uptr argmem = MM_ENV_FREE_F000; // TEMP TEMP: Maybe we should move this somewhere else

	// Check if we were supplied an argument string
	if (g_envNdsArgvHeader->magic == ENV_NDS_ARGV_MAGIC) {
		// Copy argument string to safe memory
		__builtin_memcpy((void*)argmem, g_envNdsArgvHeader->args_str, g_envNdsArgvHeader->args_str_size);
		g_envNdsArgvHeader->args_str = (char*)argmem;

		// Build argv!
		build_argv(g_envNdsArgvHeader);
	} else {
		// Initialize empty argv
		crt0FillMem32((uptr)g_envNdsArgvHeader, 0, sizeof(EnvNdsArgvHeader));
		g_envNdsArgvHeader->magic = ENV_NDS_ARGV_MAGIC;
		g_envNdsArgvHeader->argv = (char**)argmem;

		// Check if we have a boot filename from the DSi device list
		char* boot_name = NULL;
		if (is_twl && g_envTwlDeviceList->argv0[0] != 0) {
			boot_name = g_envTwlDeviceList->argv0;
		}

		// If we do: add it to argv
		if (boot_name) {
			g_envNdsArgvHeader->argc = 1;
			g_envNdsArgvHeader->argv[0] = boot_name;
		}

		// Terminate argv list
		g_envNdsArgvHeader->argv[g_envNdsArgvHeader->argc] = NULL;
	}
}

#endif

#if defined(ARM9)
#define _EXTRA_ARGS , void* heap_end
#else
#define _EXTRA_ARGS
#endif

void crt0Startup(Crt0Header const* hdr, bool is_twl _EXTRA_ARGS)
{
#if defined(ARM9)
	// Configure the protection unit
	crt0SetupMPU(is_twl);

	// Shelter the header on stack in order to prevent LMA/VMA overlaps from overwriting it
	Crt0Header _hdr = *hdr;
	hdr = &_hdr;
#endif

#if defined(ARM7)
	// Clear some memory
	crt0FillMem32(MM_ENV_FREE_D000, 0, MM_ENV_FREE_D000_SZ);
	crt0FillMem32(MM_ENV_FREE_F000, 0, MM_ENV_FREE_F000_SZ);
	crt0FillMem32(MM_ENV_FREE_FBE0, 0, MM_ENV_FREE_FBE0_SZ);
	crt0FillMem32(MM_ENV_FREE_FF60, 0, MM_ENV_FREE_FF60_SZ);

	if (is_twl) {
		// Ensure WRAM_A is mapped to the right location
		MEOW_REG(u32, IO_MBK_MAP_A) = g_envAppTwlHeader->arm7_wram_setting[0];
	}
#endif

	// Process load lists
	crt0ProcessLoadList(&hdr->ll_ntr);
	if (is_twl) {
		crt0ProcessLoadList(&hdr->ll_twl);
	}

#if defined(ARM9)
	// Flush data cache and invalidate instruction cache
	armDCacheFlushAll();
	armICacheInvalidateAll();
#endif

	// Back up DSi mode flag
	g_isTwlMode = is_twl;

	// Set up newlib heap
#if defined(ARM9)
	fake_heap_start = is_twl ? __heap_start_twl : __heap_start_ntr;
	fake_heap_end = heap_end;
#elif defined(ARM7)
	fake_heap_start = __heap_start;
	fake_heap_end = __heap_end;
#endif

#if defined(ARM7)
	// Set up argv
	crt0SetupArgv(is_twl);
#endif

	// Initialize the interrupt controller
	REG_IE = 0;
	REG_IF = ~0;
#if defined(ARM7)
	if (is_twl) {
		REG_IE2 = 0;
		REG_IF2 = ~0;
	}
#endif
	REG_IME = 1;

	// Initialize the threading system
	_threadInit();

	// Initialize the inter-processor communication system
	_pxiInit();

	// Call global constructors
	__libc_init_array();
}
