#pragma once
#include "../types.h"

#if defined(__GBA__)
#include "io.h"
#elif defined(__NDS__)
#include "../nds/io.h"
#else
#error "This header file is only for GBA and NDS"
#endif

#if defined(__GBA__)
#define LCD_WIDTH  240
#define LCD_HEIGHT 160
#elif defined(__NDS__)
#define LCD_WIDTH  256
#define LCD_HEIGHT 192
#endif

#define REG_DISPSTAT MK_REG(u16, IO_DISPSTAT)
#define REG_VCOUNT   MK_REG(u16, IO_VCOUNT)

#define DISPSTAT_IF_VBLANK (1U<<0)
#define DISPSTAT_IF_HBLANK (1U<<1)
#define DISPSTAT_IF_VCOUNT (1U<<2)
#define DISPSTAT_IE_VBLANK (1U<<3)
#define DISPSTAT_IE_HBLANK (1U<<4)
#define DISPSTAT_IE_VCOUNT (1U<<5)
#define DISPSTAT_IE_ALL    (7U<<3)
#if defined(__GBA__)
#define DISPSTAT_LYC_MASK  (0xff<<8)
#define DISPSTAT_LYC(_x)   (((_x)&0xff)<<8)
#elif defined(__NDS__)
#define DISPSTAT_LCD_READY_TWL (1U<<6)
#define DISPSTAT_LYC_MASK  (0x1ff<<7)
#define DISPSTAT_LYC(_x)   _lcdCalcLyc(_x)

MK_CONSTEXPR unsigned _lcdCalcLyc(unsigned lyc)
{
	return ((lyc&0xff) << 8) | (((lyc>>8)&1) << 7);
}
#endif

MK_EXTERN_C_START

MK_INLINE void lcdSetIrqMask(unsigned mask, unsigned value)
{
	mask &= DISPSTAT_IE_ALL;
	REG_DISPSTAT = (REG_DISPSTAT &~ mask) | (value & mask);
}

MK_INLINE void lcdSetVBlankIrq(bool enable)
{
	lcdSetIrqMask(DISPSTAT_IE_VBLANK, enable ? DISPSTAT_IE_VBLANK : 0);
}

MK_INLINE void lcdSetHBlankIrq(bool enable)
{
	lcdSetIrqMask(DISPSTAT_IE_HBLANK, enable ? DISPSTAT_IE_HBLANK : 0);
}

MK_INLINE void lcdSetVCountCompare(bool enable, unsigned lyc)
{
	unsigned reg = REG_DISPSTAT &~ (DISPSTAT_IE_VCOUNT|DISPSTAT_LYC_MASK);
	if (enable) {
		reg |= DISPSTAT_IE_VCOUNT | DISPSTAT_LYC(lyc);
	}
	REG_DISPSTAT = reg;
}

MK_INLINE bool lcdInVBlank(void)
{
	return REG_DISPSTAT & DISPSTAT_IF_VBLANK;
}

MK_INLINE bool lcdInHBlank(void)
{
	return REG_DISPSTAT & DISPSTAT_IF_HBLANK;
}

MK_INLINE bool lcdInVCountMatch(void)
{
	return REG_DISPSTAT & DISPSTAT_IF_VCOUNT;
}

MK_INLINE unsigned lcdGetVCount(void)
{
	return REG_VCOUNT;
}

MK_EXTERN_C_END
