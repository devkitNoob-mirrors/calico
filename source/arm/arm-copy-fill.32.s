// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#include <calico/asm.inc>

FUNC_START32 armCopyMem32
	push  {r4-r10} @ <!> Not 8-byte aligned
	bics  r12, r2, #0x1f
	beq   .LcopyRemainder
	add   r12, r0, r12

1:	ldm   r1!, {r3-r10}
	stm   r0!, {r3-r10}
	cmp   r12, r0
	bne   1b

.LcopyRemainder:
	tst   r2, #0x10
	ldmne r1!, {r4-r7}
	stmne r0!, {r4-r7}

	tst   r2, #0x08
	ldmne r1!, {r4-r5}
	stmne r0!, {r4-r5}

	tst   r2, #0x04
	ldmne r1!, {r4}
	stmne r0!, {r4}

	pop   {r4-r10}
	bx    lr
FUNC_END

FUNC_START32 armFillMem32
	push  {r4-r9}
	bics  r12, r2, #0x1f

	@ This is super dumb
	mov   r4, r1
	mov   r5, r1
	mov   r6, r1
	mov   r7, r1

	beq   .LfillRemainder
	add   r12, r0, r12

	@ This is also super dumb
	mov   r3, r1
	mov   r8, r1
	mov   r9, r1

1:	stm   r0!, {r1,r3-r9}
	cmp   r12, r0
	bne   1b

.LfillRemainder:
	tst   r2, #0x10
	stmne r0!, {r4-r7}

	tst   r2, #0x08
	stmne r0!, {r4-r5}

	tst   r2, #0x04
	stmne r0!, {r4}

	pop   {r4-r9}
	bx    lr
FUNC_END
