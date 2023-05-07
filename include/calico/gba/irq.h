#pragma once
#if !defined(__GBA__)
#error "This header file is only for GBA"
#endif

#include "../types.h"
#include "../arm/common.h"
#include "io.h"

#define REG_IE  MEOW_REG(u16, IO_IE)
#define REG_IF  MEOW_REG(u16, IO_IF)
#define REG_IME MEOW_REG(u16, IO_IME)

#define IRQ_VBLANK (1U << 0)
#define IRQ_HBLANK (1U << 1)
#define IRQ_VCOUNT (1U << 2)
#define IRQ_TIMER0 (1U << 3)
#define IRQ_TIMER1 (1U << 4)
#define IRQ_TIMER2 (1U << 5)
#define IRQ_TIMER3 (1U << 6)
#define IRQ_SERIAL (1U << 7)
#define IRQ_DMA0   (1U << 8)
#define IRQ_DMA1   (1U << 9)
#define IRQ_DMA2   (1U << 10)
#define IRQ_DMA3   (1U << 11)
#define IRQ_KEYPAD (1U << 12)
#define IRQ_CART   (1U << 13)

#define IRQ_TIMER(_x) (1U << (3+(_x)))
#define IRQ_DMA(_x)   (1U << (8+(_x)))

#define MEOW_IRQ_NUM_HANDLERS 16

MEOW_EXTERN_C_START

typedef unsigned IrqState;
typedef u16 IrqMask;

MEOW_INLINE IrqState irqLock(void)
{
	armCompilerBarrier();
	IrqState saved = REG_IME;
	REG_IME = 0;
	armCompilerBarrier();
	return saved;
}

MEOW_INLINE void irqUnlock(IrqState state)
{
	armCompilerBarrier();
	REG_IME = state;
	armCompilerBarrier();
}

MEOW_EXTERN_C_END
