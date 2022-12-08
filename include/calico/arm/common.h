#pragma once
#include "../types.h"
#include "psr.h"
#if __ARM_ARCH >= 5
#include "cp15.h"
#endif

typedef struct ArmContext {
	u32 r[16];
	u32 psr;
	u32 sp_svc;
} ArmContext;

typedef unsigned ArmIrqState;

#if !__thumb__

MEOW_INLINE u32 armGetCpsr(void)
{
	u32 reg;
	__asm__ __volatile__ ("mrs %0, cpsr" : "=r" (reg));
	return reg;
}

MEOW_INLINE void armSetCpsrC(u32 value)
{
	__asm__ __volatile__ ("msr cpsr_c, %0" :: "r" (value));
}

MEOW_INLINE u32 armGetSpsr(void)
{
	u32 reg;
	__asm__ __volatile__ ("mrs %0, spsr" : "=r" (reg));
	return reg;
}

MEOW_INLINE void armSetSpsr(u32 value)
{
	__asm__ __volatile__ ("msr spsr, %0" :: "r" (value));
}

#if __ARM_ARCH >= 5

MEOW_INLINE void armWaitForIrq(void)
{
	// Clobber is used, as user may expect variables to be updated by ISRs
	__asm__ __volatile__ ("mcr p15, 0, r0, c7, c0, 4" ::: "memory");
}

#endif

MEOW_INLINE ArmIrqState armIrqLockByPsr(void)
{
	u32 psr = armGetCpsr();
	armSetCpsrC(psr | ARM_PSR_I | ARM_PSR_F);
	return psr & (ARM_PSR_I | ARM_PSR_F);
}

MEOW_INLINE void armIrqUnlockByPsr(ArmIrqState st)
{
	u32 psr = armGetCpsr() &~ (ARM_PSR_I | ARM_PSR_F);
	armSetCpsrC(psr | st);
}

#else

MEOW_EXTERN32 ArmIrqState armIrqLockByPsrFromThumb(void);
MEOW_EXTERN32 void armIrqUnlockByPsrFromThumb(ArmIrqState st);

MEOW_INLINE ArmIrqState armIrqLockByPsr(void)
{
	return armIrqLockByPsrFromThumb();
}

MEOW_INLINE void armIrqUnlockByPsr(ArmIrqState st)
{
	armIrqUnlockByPsrFromThumb(st);
}

#endif

MEOW_EXTERN32 void armCopyMem32(void* dst, const void* src, size_t size);
MEOW_EXTERN32 void armFillMem32(void* dst, u32 value, size_t size);

MEOW_EXTERN32 u32 armContextSave(ArmContext* ctx, ArmIrqState st, u32 ret);
MEOW_EXTERN32 void armContextLoad(const ArmContext* ctx) MEOW_NORETURN;
