#pragma once
#include "../types.h"
#include "psr.h"
#if __ARM_ARCH >= 5
#include "cp15.h"
#endif

/*! @addtogroup arm
	@{
*/

MK_EXTERN_C_START

typedef struct ArmContext {
	u32 r[16];
	u32 psr;
	u32 sp_svc;
} ArmContext;

typedef unsigned ArmIrqState;

MK_INLINE void armCompilerBarrier(void)
{
	__asm__ __volatile__ ("" ::: "memory");
}

MK_INLINE void armSoftBreakpoint(void)
{
	__asm__ __volatile__ ("mov r11, r11");
}

#if !__thumb__

MK_EXTINLINE u32 armGetCpsr(void)
{
	u32 reg;
	__asm__ __volatile__ ("mrs %0, cpsr" : "=r" (reg));
	return reg;
}

MK_EXTINLINE void armSetCpsrC(u32 value)
{
	__asm__ __volatile__ ("msr cpsr_c, %0" :: "r" (value) : "memory");
}

MK_EXTINLINE u32 armGetSpsr(void)
{
	u32 reg;
	__asm__ __volatile__ ("mrs %0, spsr" : "=r" (reg));
	return reg;
}

MK_EXTINLINE void armSetSpsr(u32 value)
{
	__asm__ __volatile__ ("msr spsr, %0" :: "r" (value));
}

MK_EXTINLINE u32 armSwapWord(u32 value, u32* addr)
{
	u32 ret;
	__asm__ __volatile__ ("swp %[Rd], %[Rm], [%[Rn]]" : [Rd]"=r"(ret) : [Rm]"[Rd]"(value), [Rn]"r"(addr) : "memory");
	return ret;
}

MK_EXTINLINE u8 armSwapByte(u8 value, u8* addr)
{
	u8 ret;
	__asm__ __volatile__ ("swpb %[Rd], %[Rm], [%[Rn]]" : [Rd]"=r"(ret) : [Rm]"[Rd]"(value), [Rn]"r"(addr) : "memory");
	return ret;
}

#if __ARM_ARCH >= 5

MK_EXTINLINE void armWaitForIrq(void)
{
	// Clobber is used, as user may expect variables to be updated by ISRs
	__asm__ __volatile__ ("mcr p15, 0, r0, c7, c0, 4" ::: "memory");
}

MK_EXTINLINE u32 armGetCp15Cr(void)
{
	u32 reg;
	__asm__ __volatile__ ("mrc p15, 0, %0, c1, c0, 0" : "=r" (reg));
	return reg;
}

MK_EXTINLINE void armSetCp15Cr(u32 value)
{
	__asm__ __volatile__ ("mcr p15, 0, %0, c1, c0, 0" :: "r" (value) : "memory");
}

#endif

MK_EXTINLINE ArmIrqState armIrqLockByPsr(void)
{
	u32 psr = armGetCpsr();
	armSetCpsrC(psr | ARM_PSR_I | ARM_PSR_F);
	return psr & (ARM_PSR_I | ARM_PSR_F);
}

MK_EXTINLINE void armIrqUnlockByPsr(ArmIrqState st)
{
	u32 psr = armGetCpsr() &~ (ARM_PSR_I | ARM_PSR_F);
	armSetCpsrC(psr | st);
}

#else

MK_EXTERN32 u32 armGetCpsr(void);
MK_EXTERN32 void armSetCpsrC(u32 value);
MK_EXTERN32 u32 armGetSpsr(void);
MK_EXTERN32 void armSetSpsr(u32 value);
MK_EXTERN32 u32 armSwapWord(u32 value, u32* addr);
MK_EXTERN32 u8 armSwapByte(u8 value, u8* addr);

#if __ARM_ARCH >= 5
MK_EXTERN32 void armWaitForIrq(void);
MK_EXTERN32 u32 armGetCp15Cr(void);
MK_EXTERN32 void armSetCp15Cr(u32 value);
#endif

MK_EXTERN32 ArmIrqState armIrqLockByPsr(void);
MK_EXTERN32 void armIrqUnlockByPsr(ArmIrqState st);

#endif

MK_EXTERN32 void armCopyMem32(void* dst, const void* src, size_t size);
MK_EXTERN32 void armFillMem32(void* dst, u32 value, size_t size);

MK_EXTERN32 u32 armContextSave(ArmContext* ctx, ArmIrqState st, u32 ret);
MK_EXTERN32 void armContextLoad(const ArmContext* ctx) MK_NORETURN;

MK_EXTERN_C_END

//! @}
