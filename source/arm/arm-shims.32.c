// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#include <calico/arm/common.h>

extern u32 armGetCpsr(void);
extern void armSetCpsrC(u32 value);
extern u32 armGetSpsr(void);
extern void armSetSpsr(u32 value);
extern u32 armSwapWord(u32 value, vu32* addr);
extern u8 armSwapByte(u8 value, vu8* addr);

#if __ARM_ARCH >= 5
extern void armWaitForIrq(void);
extern u32 armGetCp15Cr(void);
extern void armSetCp15Cr(u32 value);
#endif

extern ArmIrqState armIrqLockByPsr(void);
extern void armIrqUnlockByPsr(ArmIrqState st);
