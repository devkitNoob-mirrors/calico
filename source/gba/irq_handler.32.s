#include <calico/asm.inc>
#include <calico/arm/psr.h>
#include <calico/gba/mm.h>
#include <calico/gba/io.h>

FUNC_START32 __irq_handler

	@ Retrieve active interrupt mask
	mov   r12, #MM_IO
	ldr   r0, [r12, #IO_IE]   @ read both IE and IF at the same time
	ands  r0, r0, r0, lsr #16 @ r0 <- IO_IE & IO_IF
	bxeq  lr                  @ If r0 is empty we can't do anything

	@ Select an interrupt (LSB has priority)
	mov   r3, #1              @ dummy 1 needed for shifting
	mov   r1, #0              @ bit counter
1:	ands  r2, r0, r3, lsl r1  @ r2 -> cur_irq_mask
	addeq r1, r1, #1          @ r1 -> cur_irq_id
	beq   1b

	@ Acknowledge the interrupt
	add   r3, r12, #IO_IE
	strh  r2, [r3, #IO_IF-IO_IE]

	@ __irq_flags |= cur_irq_mask (using the same mirror as the BIOS)
	ldrh  r3, [r12, #-8]
	orr   r3, r3, r2
	strh  r3, [r12, #-8]

	@ Load the handler and call it
	ldr   r3, =__irq_table
	ldr   r3, [r3, r1, lsl #2]
	push  {r2, lr} @ save irq_mask & BIOS return address
	cmp   r3, #0
	adr   lr, 1f
	moveq r3, lr @ avoid crashing if no handler is registered
	bx    r3

	@ Check if thread rescheduling is needed
1:	ldr   r3, [sp, #0]       @ r3 <- cur_irq_mask
	ldr   r2, =__sched_state
	ldrh  r1, [r2, #3*4]     @ r1 <- s_irqWaitMask
	ands  r3, r3, r1         @ cur_irq_mask &= s_irqWaitMask
	beq   1f                 @ if (!cur_irq_mask) -> skip this section

	@ s_irqWaitMask &= ~cur_irq_mask
	bic   r1, r1, r3
	strh  r1, [r2, #3*4]

	@ __irq_flags &= ~cur_irq_mask
	mov   r12, #MM_IO
	ldrh  r1, [r12, #-8]
	bic   r1, r1, r3
	strh  r1, [r12, #-8]

	@ threadUnblock(&irqWaitList, -1, ThrUnblockMode_ByMask, cur_irq_mask)
	add   r0, r2, #4*4
	@mov   r1, #-1
	@mov   r2, #2
	@bl    threadUnblock
	mov   r1, r3
	bl    threadUnblockAllByMask

	@ Check if we have a pending reschedule
	ldr   r2, =__sched_state
1:	add   sp, sp, #8
	ldr   r0, [r2, #4] @ r0 <- s_deferredThread
	cmp   r0, #0
	ldreq pc, [sp, #-4] @ Return to BIOS if not

	@ s_curThread = s_deferredThread; s_deferredThread = NULL;
	mov   r3, #0
	ldr   r1, [r2, #0]
	stm   r2, {r0, r3}

	@ Save old thread's context
	mrs  r2, spsr
	str  r2, [r1, #16*4]
	pop  {r2,r3}
	stm  r1!, {r2,r3}
	pop  {r2,r3,r12,lr}
	sub  lr, lr, #4
	stm  r1, {r2-r14}^
	str  lr, [r1, #(15-2)*4]
	msr  cpsr_c, #(ARM_PSR_I | ARM_PSR_F | ARM_PSR_MODE_SVC)
	str  sp, [r1, #(17-2)*4]

	@ Load new thread's context
	b    armContextLoadFromSvc

FUNC_END
