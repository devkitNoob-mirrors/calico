#pragma once
#include "../types.h"

#if defined(__GBA__)
#include "../gba/irq.h"
#elif defined(__NDS__)
#include "../nds/irq.h"
#else
#error "Unsupported platform."
#endif

MK_EXTERN_C_START

typedef void (*IrqHandler)(void);

extern volatile IrqMask __irq_flags;

void irqSet(IrqMask mask, IrqHandler handler);

MK_INLINE void irqClear(IrqMask mask)
{
	irqSet(mask, NULL);
}

MK_INLINE void irqEnable(IrqMask mask)
{
	IrqState st = irqLock();
	REG_IE |= mask;
	irqUnlock(st);
}

MK_INLINE void irqDisable(IrqMask mask)
{
	IrqState st = irqLock();
	REG_IE &= ~mask;
	irqUnlock(st);
}

#if MK_IRQ_NUM_HANDLERS > 32

extern volatile IrqMask __irq_flags2;

void irqSet2(IrqMask mask, IrqHandler handler);

MK_INLINE void irqClear2(IrqMask mask)
{
	irqSet2(mask, NULL);
}

MK_INLINE void irqEnable2(IrqMask mask)
{
	IrqState st = irqLock();
	REG_IE2 |= mask;
	irqUnlock(st);
}

MK_INLINE void irqDisable2(IrqMask mask)
{
	IrqState st = irqLock();
	REG_IE2 &= ~mask;
	irqUnlock(st);
}

#endif

MK_EXTERN_C_END
