#include <calico/arm/common.h>

ArmIrqState armIrqLockByPsrFromThumb(void) __asm__("armIrqLockByPsr");
void armIrqUnlockByPsrFromThumb(ArmIrqState st) __asm__("armIrqUnlockByPsr");
u32 armSwapWordFromThumb(u32 value, u32* addr) __asm__("armSwapWord");
u8 armSwapByteFromThumb(u8 value, u8* addr) __asm__("armSwapByte");

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
