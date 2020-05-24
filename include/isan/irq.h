#pragma once
#include "types.h"

#if defined(IS_GBA)
#include "gba/irq.h"
#else
#error "Unsupported platform."
#endif

typedef void (*IrqHandler)(void);

void irqSet(IrqMask mask, IrqHandler handler);

IS_INLINE void irqClear(IrqMask mask)
{
	irqSet(mask, NULL);
}

IS_INLINE void irqEnable(IrqMask mask)
{
	IrqState st = irqLock();
	REG_IE |= mask;
	irqUnlock(st);
}

IS_INLINE void irqDisable(IrqMask mask)
{
	IrqState st = irqLock();
	REG_IE &= ~mask;
	irqUnlock(st);
}
