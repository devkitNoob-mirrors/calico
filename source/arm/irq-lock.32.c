#include <calico/arm/common.h>

ArmIrqState armIrqLockByPsrFromThumb(void)
{
	return armIrqLockByPsr();
}

void armIrqUnlockByPsrFromThumb(ArmIrqState st)
{
	armIrqUnlockByPsr(st);
}
