#pragma once
#include "../types.h"
#include "system.h"
#include "env.h"

#define TLNC_MAGIC   0x434e4c54 // 'TLNC'
#define TLNC_VERSION 1

MK_EXTERN_C_START

typedef enum TlncAppType {
	TlncAppType_Unknown = 0, // Unspecified
	TlncAppType_Card    = 1, // Boot currently inserted NDS card in Slot 1
	TlncAppType_Temp    = 2, // Boot nand:/tmp/jump.app
	TlncAppType_Nand    = 3, // Boot DSiWare title with the given title ID
	TlncAppType_Ram     = 4, // Boot app directly from RAM (not fully implemented)
} TlncAppType;

typedef struct TlncData {
	u64 caller_tid;      // 0 = anonymous
	u64 target_tid;      // 0 = system menu
	u8  valid       : 1; // Not checked on DSi, but checked on 3DS
	u8  app_type    : 3; // TlncAppType
	u8  skip_intro  : 1; // Intro can only appear when target_tid=0 (sysmenu)
	u8  unk5        : 1; // Unused
	u8  skip_load   : 1; // Needed for TlncAppType_Ram
	u8  unk7        : 1; // Unused
} TlncData;

typedef struct TlncArea {
	u32 magic;
	u8  version;
	u8  data_len;
	u16 data_crc16;
	TlncData data;
} TlncArea;

bool tlncGetDataTWL(TlncData* data);
void tlncSetDataTWL(TlncData const* data);

MK_INLINE bool tlncGetData(TlncData* data)
{
	return systemIsTwlMode() && tlncGetDataTWL(data);
}

MK_INLINE void tlncSetData(TlncData const* data)
{
	if (systemIsTwlMode()) {
		tlncSetDataTWL(data);
	}
}

MK_EXTERN_C_END
