#pragma once
#include "types.h"

#define MWL_MAC_RAM    0x4000
#define MWL_MAC_RAM_SZ (0x2000 - sizeof(MwlMacVars))

#if defined(__NDS__) && defined(ARM7)
#include "../../nds/io.h"

#define MWL_MAC_RAM_ADDR (MM_IO + IO_MITSUMI_WS0 + MWL_MAC_RAM)
#define g_mwlMacVars     ((volatile MwlMacVars*)(MWL_MAC_RAM_ADDR + MWL_MAC_RAM_SZ))
#endif

MEOW_EXTERN_C_START

typedef struct MwlMacVars {
	u16 unk_0x00[8];
	u16 unk_0x10;
	u16 unk_0x12;
	u16 unk_0x14;
	u16 unk_0x16;
	u16 unk_0x18;
	u16 unk_0x1a;
	u16 unk_0x1c;
	u16 unk_0x1e;
	u16 wep_keys[4][0x10];
} MwlMacVars;

void mwlDevWakeUp(void);
void mwlDevReset(void);
void mwlDevSetChannel(unsigned ch);
void mwlDevShutdown(void);

MEOW_EXTERN_C_END
