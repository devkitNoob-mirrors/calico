// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#include <calico/asm.inc>
#include <calico/arm/psr.h>

.global armContextLoadFromSvc
.type armContextLoadFromSvc, %function

@---------------------------------------------------------------------------------
FUNC_START32 armContextSave
@---------------------------------------------------------------------------------
	mrs   r12, cpsr
	str   r2, [r0], #15*4 @ store a non-null value into r0 (r1-r3 never saved)
	bic   r3, r12, #(ARM_PSR_I | ARM_PSR_F)
	adr   r2, 1f
	orr   r3, r3, r1
	msr   cpsr_c, #(ARM_PSR_I | ARM_PSR_F | ARM_PSR_MODE_SVC)
	stmdb r0, {r4-r14}^   @ save r4-r14 -- XX: r12 is saved, even though it's volatile
	stmia r0, {r2,r3,sp}  @ r2=pc, r3=psr, sp=sp_svc
	msr   cpsr_c, r12
	mov   r0, #0
1:	bx    lr
FUNC_END

@---------------------------------------------------------------------------------
FUNC_START32 armContextLoad
@---------------------------------------------------------------------------------
	msr   cpsr_c, #(ARM_PSR_I | ARM_PSR_F | ARM_PSR_MODE_SVC)
armContextLoadFromSvc:
	ldr   lr, [r0, #15*4]
	ldr   r3, [r0, #16*4]
	ldr   sp, [r0, #17*4]
	msr   spsr, r3
	ldm   r0, {r0-r14}^
	nop   @ Architecturally required before accessing a banked reg (pre-ARMv6)
	movs  pc, lr
FUNC_END
