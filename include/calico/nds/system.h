#pragma once
#if !defined(__NDS__)
#error "This header file is only for NDS"
#endif

#include "../types.h"
#include "io.h"

#define REG_EXMEMCNT  MEOW_REG(u16, IO_EXMEMCNT)
#ifdef ARM7
#define REG_EXMEMCNT2 MEOW_REG(u16, IO_EXMEMCNT2)
#endif

#define EXMEMCNT_GBA_SLOT_ARM9        (0U<<7)
#define EXMEMCNT_GBA_SLOT_ARM7        (1U<<7)
#define EXMEMCNT_NDS_SLOT_ARM9        (0U<<11)
#define EXMEMCNT_NDS_SLOT_ARM7        (1U<<11)
#define EXMEMCNT_MAIN_RAM_IFACE_ASYNC (0U<<14)
#define EXMEMCNT_MAIN_RAM_IFACE_SYNC  (1U<<14)
#define EXMEMCNT_MAIN_RAM_PRIO_ARM9   (0U<<15)
#define EXMEMCNT_MAIN_RAM_PRIO_ARM7   (1U<<15)

MEOW_INLINE bool systemIsTwlMode(void)
{
	extern bool g_isTwlMode;
	return g_isTwlMode;
}
