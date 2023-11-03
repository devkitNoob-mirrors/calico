#include <calico/arm/common.h>

extern u32 armGetCpsr(void);
extern void armSetCpsrC(u32 value);
extern u32 armGetSpsr(void);
extern void armSetSpsr(u32 value);
extern u32 armSwapWord(u32 value, u32* addr);
extern u8 armSwapByte(u8 value, u8* addr);

#if __ARM_ARCH >= 5
extern void armWaitForIrq(void);
#endif

extern ArmIrqState armIrqLockByPsr(void);
extern void armIrqUnlockByPsr(ArmIrqState st);
