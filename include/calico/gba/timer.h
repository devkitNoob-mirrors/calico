#pragma once
#if !defined(__GBA__)
#error "This header file is only for GBA"
#endif

#include "../types.h"
#include "io.h"

#define REG_TMxCNT(_x)   MEOW_REG(u32, IO_TMxCNT(_x))
#define REG_TMxCNT_L(_x) MEOW_REG(u16, IO_TMxCNT(_x)+0)
#define REG_TMxCNT_H(_x) MEOW_REG(u16, IO_TMxCNT(_x)+2)

#define TIMER_FREQ 0x1000000 // GBA system bus speed is exactly 2^24 Hz ~= 16.78 MHz

#define TIMER_PRESCALER_1    (0<<0)
#define TIMER_PRESCALER_64   (1<<0)
#define TIMER_PRESCALER_256  (2<<0)
#define TIMER_PRESCALER_1024 (3<<0)
#define TIMER_CASCADE        (1<<2)
#define TIMER_ENABLE_IRQ     (1<<6)
#define TIMER_ENABLE         (1<<7)

MEOW_CONSTEXPR u16 timerCalcReload(unsigned prescaler, unsigned freq)
{
	unsigned basefreq = TIMER_FREQ;
	if (prescaler) basefreq >>= prescaler*2 + 4;
	unsigned period = (basefreq + freq/2) / freq;
	return period < 0x10000 ? (0x10000-period) : 0;
}

MEOW_INLINE void timerStop(unsigned id)
{
	REG_TMxCNT_H(id) = 0;
}

MEOW_INLINE void timerStart(unsigned id, unsigned prescaler, unsigned freq, bool wantIrq)
{
	REG_TMxCNT_L(id) = timerCalcReload(prescaler, freq);
	REG_TMxCNT_H(id) = prescaler | TIMER_ENABLE | (wantIrq ? TIMER_ENABLE_IRQ : 0);
}

MEOW_INLINE void timerStartCascade(unsigned id, bool wantIrq)
{
	REG_TMxCNT_L(id) = 0;
	REG_TMxCNT_H(id) = TIMER_CASCADE | TIMER_ENABLE | (wantIrq ? TIMER_ENABLE_IRQ : 0);
}

MEOW_INLINE u16 timerRead(unsigned id)
{
	return REG_TMxCNT_L(id);
}
