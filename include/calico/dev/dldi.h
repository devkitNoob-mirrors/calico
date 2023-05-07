#pragma once
#include "dldi_defs.h"
#include "disc_io.h"

MEOW_EXTERN_C_START

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

	DISC_INTERFACE disc;
} DldiHeader;

MEOW_EXTERN_C_END
