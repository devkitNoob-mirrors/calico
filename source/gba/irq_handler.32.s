#include <isan/asm.inc>
#include <isan/arm.h>
#include <isan/hardware.h>

FUNC_START32 __irq_handler

	@ Retrieve active interrupt mask
	mov   r12, #MM_IO
	ldr   r0, [r12, #IO_IE]   @ read both IE and IF at the same time
	ands  r0, r0, r0, lsr #16 @ r0 = IO_IE & IO_IF
	bxeq  lr                  @ If r0 is empty we can't do anything

	@ Select an interrupt (LSB has priority)
	mov   r3, #1              @ dummy 1 needed for shifting
	mov   r1, #0              @ bit counter
1:	ands  r2, r0, r3, lsl r1  @ r2 => bitmask
	addeq r1, r1, #1          @ r1 => interrupt id
	beq   1b

	@ Acknowledge the interrupt
	add   r3, r12, #IO_IE
	strh  r2, [r3, #IO_IF-IO_IE]

	@ Update __irq_flags (using the same mirror as the BIOS)
	ldrh  r3, [r12, #-8]
	orr   r3, r3, r2
	strh  r3, [r12, #-8]

	@ Load the handler and jump to it
	ldr   r3, =__irq_table
	ldr   r3, [r3, r1, lsl #2]
	cmp   r3, #0 @ avoid crashing if no handler is registered
	bxne  r3
	bx    lr

FUNC_END

SECT_BSS __irq_table

.global __irq_table
__irq_table:
	.space 16*4
