#include <calico/arm/common.h>

ArmIrqState armIrqLockByPsrFromThumb(void) __asm__("armIrqLockByPsr");
void armIrqUnlockByPsrFromThumb(ArmIrqState st) __asm__("armIrqUnlockByPsr");
extern u32 armSwapWord(u32 value, u32* addr);
extern u8 armSwapByte(u8 value, u8* addr);

#if __ARM_ARCH >= 5
extern void armWaitForIrq(void);
#endif

ArmIrqState armIrqLockByPsrFromThumb(void)
{
	return armIrqLockByPsr();
}

void armIrqUnlockByPsrFromThumb(ArmIrqState st)
{
	armIrqUnlockByPsr(st);
}
