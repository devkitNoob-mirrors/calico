#include <calico/arm/common.h>

ArmIrqState armIrqLockByPsrFromThumb(void)
{
	return armIrqLockByPsr();
}

void armIrqUnlockByPsrFromThumb(ArmIrqState st)
{
	armIrqUnlockByPsr(st);
}

u32 armSwapWordFromThumb(u32 value, u32* addr)
{
	return armSwapWord(addr, value);
}

u8 armSwapByteFromThumb(u8 value, u8* addr)
{
	return armSwapByte(addr, value);
}
