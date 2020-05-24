#include <isan/types.h>
#include <isan/irq.h>

extern IrqHandler __irq_table[IS_IRQ_NUM_HANDLERS];

IS_INLINE bool _irqMaskUnpack(IrqMask* pmask, unsigned* pid)
{
#if (__ARM_ARCH < 5) || __thumb__
	if (!*pmask)
		return false;
	while (!(*pmask & (1U << *pid)))
		++*pid;
#else
	int id = __builtin_ffs(*pmask)-1;
	if (id < 0)
		return false;
	*pid = id;
#endif
	*pmask &= ~(1U << *pid);
	return true;
}

void irqSet(IrqMask mask, IrqHandler handler)
{
	IrqState st = irqLock();
	unsigned id = 0;
	while (_irqMaskUnpack(&mask, &id))
		__irq_table[id] = handler;
	irqUnlock(st);
}
