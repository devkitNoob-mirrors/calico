#include <isan/asm.inc>
#include <isan/arm.h>
#include <isan/gba/mm.h>
#include <isan/gba/io.h>

#ifndef IS_GBA_MULTIBOOT
FUNC_START32 __gba_start, crt0
#else
FUNC_START32 __gba_start_mb, crt0
#endif

	@ Disable interrupts
	mov  r11, #MM_IO
	strb r11, [r11, #IO_IME]

	@ Set up supervisor mode stack
	mov  r12, #ARM_PSR_MODE_SVC
	msr  cpsr, r12
	ldr  sp, =__sp_svc

	@ Set up interrupt mode stack
	mov  r12, #ARM_PSR_MODE_IRQ
	msr  cpsr, r12
	ldr  sp, =__sp_irq

	@ Switch to system mode and load a temporary stack pointer
	@ (IMPORTANT because the BIOS switches to the user stack and this gets CLEARED otherwise)
	mov  r12, #ARM_PSR_MODE_SYS
	msr  cpsr, r12
	ldr  sp, =MM_IWRAM + MM_IWRAM_SZ - 0x100

	@ Clear memory
#ifndef IS_GBA_MULTIBOOT
	mov  r0, #0xff @ clear everything
#else
	mov  r0, #0xfe @ clear everything except for ewram
#endif
	svc  0x01<<16 @ RegisterRamReset

	@ Finally, set up system mode stack
	ldr  sp, =__sp_usr

#ifndef IS_GBA_MULTIBOOT
	@ Set cart speed
	ldr  r12, =0x4317
	str  r12, [r11, #IO_WAITCNT]
#else
	@ Check for loaded-from-ROM-and-copy-into-ewram
#endif

	@ Copy IWRAM into place
	ldr  r0, =__iwram_lma
	ldr  r1, =__iwram_start
	ldr  r2, =__iwram_end
	bl   .Lcopy

	@ Set interrupt vector
	ldr  r12, =__irq_vector
	ldr  r1,  =__irq_handler
	str  r1, [r12]

	@ Enable interrupts
	mov  r1, #1
	strb r1, [r11, #IO_IME]

#ifndef IS_GBA_MULTIBOOT
	@ Copy EWRAM into place
	ldr  r0, =__ewram_lma
	ldr  r1, =__ewram_start
	ldr  r2, =__ewram_end
	bl   .Lcopy
#else
	@ TODO: Clear EWRAM BSS
#endif

	@ TODO: Set up heap (fake_heap_start/end)
	@ TODO: Call __libc_init_array

	@ Call the main function
	adr  lr, .Lreset
	mov  r0, #0
	mov  r1, #0
	ldr  r12, =main
	bx   r12

.Lreset:
	@ If main() ever returns, we soft-reset
	svc  0x00<<16 @ SoftReset

.Lcopy:
	subs r2, r2, r1
	bxeq lr
.LcopyLoop:
	subs r2, r2, #4
	ldmia r0!, {r3}
	stmia r1!, {r3}
	bne .LcopyLoop
	bx lr

FUNC_END
