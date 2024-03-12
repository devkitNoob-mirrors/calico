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

//! Arm CPU register context (GPRs and SPRs)
typedef struct ArmContext {
	u32 r[16];      //!< R0-R15
	u32 psr;        //!< CPSR (or SPSR)
	u32 sp_svc;     //!< Supervisor-mode SP value used for context switching
} ArmContext;

typedef unsigned ArmIrqState;

/*! @brief Barrier preventing the compiler from reordering memory access around it (memory clobber)
	@note Because the Arm946E-S is a single-core Armv5 processor, this can be used as memory barrier
	for atomic operations
	@note @ref armSwapWord, @ref armSwapByte and @ref armWaitForIrq already include this barrier
*/
MK_INLINE void armCompilerBarrier(void)
{
	__asm__ __volatile__ ("" ::: "memory");
}

/*! @brief No$GBA-only optional software breakpoint
	@note Only breaks if it enabled in emulator settings
*/
MK_INLINE void armSoftBreakpoint(void)
{
	__asm__ __volatile__ ("mov r11, r11");
}

#if !__thumb__

//! @brief Reads the value of the CPSR register
MK_EXTINLINE u32 armGetCpsr(void)
{
	u32 reg;
	__asm__ __volatile__ ("mrs %0, cpsr" : "=r" (reg));
	return reg;
}

//! @brief Writes the value of the CPSR register control bits (execution mode, IRQ/FIQ mask)
MK_EXTINLINE void armSetCpsrC(u32 value)
{
	__asm__ __volatile__ ("msr cpsr_c, %0" :: "r" (value) : "memory");
}

//! @brief Reads the value of the SPSR register
MK_EXTINLINE u32 armGetSpsr(void)
{
	u32 reg;
	__asm__ __volatile__ ("mrs %0, spsr" : "=r" (reg));
	return reg;
}

//! @brief Writes the value of the SPSR register
MK_EXTINLINE void armSetSpsr(u32 value)
{
	__asm__ __volatile__ ("msr spsr, %0" :: "r" (value));
}

/*! @brief Atomically always swaps a 32-bit value in memory
	@param value Value to write
	@param addr Address to update
	@note Includes a memory barrier
	@note Unless you have only 2 threads accessing `*addr` (regardless of core), this *must not*
	be used for anything but spinlocks (in other words, `num_threads * num_states <= 2`), as forward
	progress cannot otherwise be guaranteed. For Arm<>Arm9 sync, consider using @ref SMutex instead
	@returns The previous value of `*addr`
*/
MK_EXTINLINE u32 armSwapWord(u32 value, vu32* addr)
{
	u32 ret;
	__asm__ __volatile__ ("swp %[Rd], %[Rm], [%[Rn]]" : [Rd]"=r"(ret) : [Rm]"[Rd]"(value), [Rn]"r"(addr) : "memory");
	return ret;
}

/*! @brief Atomically always swaps a 8-bit value in memory
	@param value Value to write
	@param addr Address to update
	@note Includes a memory barrier
	@note Unless you have only 2 threads accessing `*addr` (regardless of core), this *must not*
	be used for anything but spinlocks (in other words, `num_threads * num_states <= 2`), as forward
	progress cannot otherwise be guaranteed. For Arm<>Arm9 sync, consider using @ref SMutex instead
	@returns The previous value of `*addr`
*/
MK_EXTINLINE u8 armSwapByte(u8 value, vu8* addr)
{
	u8 ret;
	__asm__ __volatile__ ("swpb %[Rd], %[Rm], [%[Rn]]" : [Rd]"=r"(ret) : [Rm]"[Rd]"(value), [Rn]"r"(addr) : "memory");
	return ret;
}

#if __ARM_ARCH >= 5

/*! @brief Wait for an interrupt request to be signaled, putting the CPU in a low-power state
	@note Instruction may complete even when interrupts are masked on the CPU
	@note Includes memory barrier (@ref armCompilerBarrier)
*/
MK_EXTINLINE void armWaitForIrq(void)
{
	__asm__ __volatile__ ("mcr p15, 0, r0, c7, c0, 4" ::: "memory");
}

//! @brief Reads the value of the CPU's Control Register (@ref cp15 c1,c0)
MK_EXTINLINE u32 armGetCp15Cr(void)
{
	u32 reg;
	__asm__ __volatile__ ("mrc p15, 0, %0, c1, c0, 0" : "=r" (reg));
	return reg;
}

//! @brief Writes the value of the CPU's Control Register (@ref cp15 c1,c0)
MK_EXTINLINE void armSetCp15Cr(u32 value)
{
	__asm__ __volatile__ ("mcr p15, 0, %0, c1, c0, 0" :: "r" (value) : "memory");
}

#endif

/*! @brief Masks interrupts on the CPU
	@returns Old IRQ/FIQ mask state for use with @ref armIrqUnlockByPsr
*/
MK_EXTINLINE ArmIrqState armIrqLockByPsr(void)
{
	u32 psr = armGetCpsr();
	armSetCpsrC(psr | ARM_PSR_I | ARM_PSR_F);
	return psr & (ARM_PSR_I | ARM_PSR_F);
}

/*! @brief Unmasks interrupts on the CPU
	@param st IRQ/FIQ mask state to restore, obtained with @ref armIrqLockByPsr
*/
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

/*! @brief Copies a chunk of 4-byte-aligned memory
	@param[out] dst Destination address, must be 4-byte-aligned
	@param[in] src Source address, must be 4-byte-aligned
	@param size Size of the chunk to copy, must also be 4-byte-aligned
*/
MK_EXTERN32 void armCopyMem32(void* dst, const void* src, size_t size);

/*! @brief Fills a chunk of 4-byte-aligned memory
	@param[out] dst Destination address, must be 4-byte-aligned
	@param value 32-bit pattern to write to each word of the chunk
	@param size Size of the chunk, must also be 4-byte-aligned
*/
MK_EXTERN32 void armFillMem32(void* dst, u32 value, size_t size);

//! @private
MK_EXTERN32 u32 armContextSave(ArmContext* ctx, ArmIrqState st, u32 ret);

//! @private
MK_EXTERN32 void armContextLoad(const ArmContext* ctx) MK_NORETURN;

MK_EXTERN_C_END

//! @}
