#pragma once
#if !defined(__NDS__)
#error "This header file is only for NDS"
#endif

#include "../types.h"
#include "io.h"

#if defined(ARM9)

#define REG_POWCNT MEOW_REG(u32, IO_POWCNT9)

#define POWCNT_LCD         (1U<<0) // supposedly "do not write 0"
#define POWCNT_2D_GFX_A    (1U<<1)
#define POWCNT_3D_RENDER   (1U<<2)
#define POWCNT_3D_GEOMETRY (1U<<3)
#define POWCNT_2D_GFX_B    (1U<<9)
#define POWCNT_ALL         (POWCNT_2D_GFX_A|POWCNT_3D_RENDER|POWCNT_3D_GEOMETRY|POWCNT_2D_GFX_B)
#define POWCNT_LCD_SWAP    (1U<<15)

#elif defined(ARM7)

#define REG_POWCNT MEOW_REG(u32, IO_POWCNT7)

#define POWCNT_SOUND       (1U<<0)
#define POWCNT_WL_MITSUMI  (1U<<1)
#define POWCNT_ALL         (POWCNT_SOUND|POWCNT_WL_MITSUMI)

#else
#error "ARM9 or ARM7 must be defined"
#endif

MEOW_INLINE void pmSetControl(unsigned mask, unsigned value)
{
	mask &= POWCNT_ALL;
	REG_POWCNT = (REG_POWCNT &~ mask) | (value & mask);
}

MEOW_INLINE void pmPowerOn(unsigned mask)
{
	pmSetControl(mask, mask);
}

MEOW_INLINE void pmPowerOff(unsigned mask)
{
	pmSetControl(mask, 0);
}

#if defined(ARM9)

typedef enum PmLcdLayout {
	PmLcdLayout_BottomIsA = 0,
	PmLcdLayout_BottomIsB = 1,
	PmLcdLayout_TopIsB    = PmLcdLayout_BottomIsA,
	PmLcdLayout_TopIsA    = PmLcdLayout_BottomIsB,
} PmLcdLayout;

MEOW_INLINE void pmGfxLcdSwap(void)
{
	REG_POWCNT ^= POWCNT_LCD_SWAP;
}

MEOW_INLINE void pmGfxSetLcdLayout(PmLcdLayout layout)
{
	unsigned reg = REG_POWCNT &~ POWCNT_LCD_SWAP;
	if (layout != PmLcdLayout_BottomIsA) {
		reg |= POWCNT_LCD_SWAP;
	}
	REG_POWCNT = reg;
}

#endif
