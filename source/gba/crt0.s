#include <calico/asm.inc>
#include <calico/arm/psr.h>
#include <calico/gba/mm.h>
#include <calico/gba/io.h>

REFERENCE __newlib_syscalls

#ifndef MULTIBOOT
FUNC_START32 __gba_start, crt0
#else
FUNC_START32 __gba_start_mb, crt0
#endif

	@ Switch to supervisor mode and disable interrupts
	msr  cpsr_c, #(ARM_PSR_I | ARM_PSR_F | ARM_PSR_MODE_SVC)
	mov  r11, #MM_IO
	strb r11, [r11, #IO_IME]

#ifdef MULTIBOOT
	@ Check if we are running from ROM
	mov  r12, #0x08
	cmp  r12, pc, lsr #24
	bne  .LinEwram

	@ We are in ROM, so we have to copy ourselves into ewram
	mov   r0, #0x08000000
	mov   r1, #0x02000000
	mov   r2, #(256*1024)
1:	subs  r2, #8*4
	ldmia r0!, {r3-r10}
	stmia r1!, {r3-r10}
	bgt   1b

	@ Jump to EWRAM
	adr   r0, .LinEwram
	eor   r0, r0, #(0x08000000 ^ 0x02000000)
	bx    r0

.LinEwram:
#endif

	@ Set up supervisor mode stack
	ldr  sp, =__sp_svc

	@ Set up interrupt mode stack
	msr  cpsr_c, #(ARM_PSR_I | ARM_PSR_F | ARM_PSR_MODE_IRQ)
	ldr  sp, =__sp_irq

	@ Switch to system mode and load a temporary stack pointer
	@ (IMPORTANT because the BIOS switches to the user stack and this gets CLEARED otherwise)
	msr  cpsr_c, #ARM_PSR_MODE_SYS
	ldr  sp, =MM_IWRAM + MM_IWRAM_SZ - 0x100

	@ Clear memory
#ifndef MULTIBOOT
	mov  r0, #0xff @ clear everything
#else
	mov  r0, #0xfe @ clear everything except for ewram
#endif
	svc  0x01<<16 @ RegisterRamReset

	@ Finally, set up system mode stack
	ldr  sp, =__sp_usr

#ifndef MULTIBOOT
	@ Set cart speed
	ldr  r12, =0x4317
	str  r12, [r11, #IO_WAITCNT]
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

#ifndef MULTIBOOT
	@ Copy EWRAM into place
	ldr  r0, =__ewram_lma
	ldr  r1, =__ewram_start
	ldr  r2, =__ewram_end
	bl   .Lcopy
#else
	@ TODO: Clear EWRAM BSS
#endif

	@ Set up heap
	ldr  r0, =fake_heap_start
	ldr  r1, =__heap_start
	str  r1, [r0]
	ldr  r0, =fake_heap_end
	ldr  r1, =__heap_end
	str  r1, [r0]

	@ Initialize threading system
	bl   _threadInit

	@ Call static ctors
	bl   __libc_init_array

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
