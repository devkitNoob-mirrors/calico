#pragma once
#include "../types.h"
#include "../system/sysclock.h"

#if defined(__GBA__)
#include "io.h"
#elif defined(__NDS__)
#include "../nds/io.h"
#else
#error "This header file is only for GBA and NDS"
#endif

#define REG_TMxCNT(_x)   MEOW_REG(u32, IO_TMxCNT(_x))
#define REG_TMxCNT_L(_x) MEOW_REG(u16, IO_TMxCNT(_x)+0)
#define REG_TMxCNT_H(_x) MEOW_REG(u16, IO_TMxCNT(_x)+2)

#define TIMER_FREQ SYSTEM_CLOCK

#define TIMER_PRESCALER_1    (0<<0)
#define TIMER_PRESCALER_64   (1<<0)
#define TIMER_PRESCALER_256  (2<<0)
#define TIMER_PRESCALER_1024 (3<<0)
#define TIMER_CASCADE        (1<<2)
#define TIMER_ENABLE_IRQ     (1<<6)
#define TIMER_ENABLE         (1<<7)

MEOW_EXTERN_C_START

MEOW_CONSTEXPR unsigned timerCalcPeriod(unsigned prescaler, unsigned freq)
{
	unsigned basefreq = TIMER_FREQ;
	if (prescaler) basefreq >>= prescaler*2 + 4;
	return (basefreq + freq/2) / freq;
}

MEOW_CONSTEXPR u16 timerCalcReload(unsigned prescaler, unsigned freq)
{
	unsigned period = timerCalcPeriod(prescaler, freq);
	return period < 0x10000 ? (0x10000-period) : 0;
}

MEOW_INLINE void timerEnd(unsigned id)
{
	REG_TMxCNT_H(id) = 0;
}

MEOW_INLINE void timerBegin(unsigned id, unsigned prescaler, unsigned freq, bool wantIrq)
{
	timerEnd(id);
	REG_TMxCNT_L(id) = timerCalcReload(prescaler, freq);
	REG_TMxCNT_H(id) = prescaler | TIMER_ENABLE | (wantIrq ? TIMER_ENABLE_IRQ : 0);
}

MEOW_INLINE void timerBeginCascade(unsigned id, bool wantIrq)
{
	timerEnd(id);
	REG_TMxCNT_L(id) = 0;
	REG_TMxCNT_H(id) = TIMER_CASCADE | TIMER_ENABLE | (wantIrq ? TIMER_ENABLE_IRQ : 0);
}

MEOW_INLINE u16 timerRead(unsigned id)
{
	return REG_TMxCNT_L(id);
}

MEOW_EXTERN_C_END
