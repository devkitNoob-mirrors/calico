#pragma once
#include "../types.h"
#include "dldi_defs.h"

typedef struct DldiDiscIface {
	u32 io_type;
	u32 features;
	bool (* startup)(void);
	bool (* is_inserted)(void);
	bool (* read_sectors)(u32 sectors, u32 num_sectors, void* buffer);
	bool (* write_sectors)(u32 sectors, u32 num_sectors, const void* buffer);
	bool (* clear_status)(void);
	bool (* shutdown)(void);
} DldiDiscIface;

typedef struct DldiHeader {
	u32  magic_num;
	char magic_str[DLDI_MAGIC_STRING_LEN];
	u8   version_num;
	u8   driver_sz_log2;
	u8   fix_flags;
	u8   alloc_sz_log2;

	char iface_name[DLDI_FRIENDLY_NAME_LEN];

	uptr dldi_start;
	uptr dldi_end;
	uptr glue_start;
	uptr glue_end;
	uptr got_start;
	uptr got_end;
	uptr bss_start;
	uptr bss_end;

	DldiDiscIface disc;
} DldiHeader;
