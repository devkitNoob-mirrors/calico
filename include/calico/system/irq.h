#pragma once
#include "../types.h"

#if defined(__GBA__)
#include "../gba/irq.h"
#elif defined(__NDS__)
#include "../nds/irq.h"
#else
#error "Unsupported platform."
#endif

typedef void (*IrqHandler)(void);

extern volatile IrqMask __irq_flags;

void irqSet(IrqMask mask, IrqHandler handler);

MEOW_INLINE void irqClear(IrqMask mask)
{
	irqSet(mask, NULL);
}

MEOW_INLINE void irqEnable(IrqMask mask)
{
	IrqState st = irqLock();
	REG_IE |= mask;
	irqUnlock(st);
}

MEOW_INLINE void irqDisable(IrqMask mask)
{
	IrqState st = irqLock();
	REG_IE &= ~mask;
	irqUnlock(st);
}

#if MEOW_IRQ_NUM_HANDLERS > 32

extern volatile IrqMask __irq_flags2;

void irqSet2(IrqMask mask, IrqHandler handler);

MEOW_INLINE void irqClear2(IrqMask mask)
{
	irqSet2(mask, NULL);
}

MEOW_INLINE void irqEnable2(IrqMask mask)
{
	IrqState st = irqLock();
	REG_IE2 |= mask;
	irqUnlock(st);
}

MEOW_INLINE void irqDisable2(IrqMask mask)
{
	IrqState st = irqLock();
	REG_IE2 &= ~mask;
	irqUnlock(st);
}

#endif
