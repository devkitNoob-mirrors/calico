// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#include <calico/asm.inc>

@ Contrary to the AAPCS32, __aeabi_read_tp is NOT allowed to clobber r1-r3.
@ As a result of this, we cannot use C to implement this function.
@ The corresponding C pseudocode for the following ASM is:

@ void* __aeabi_read_tp(void) {
@     return threadGetSelf()->tp;
@ }

FUNC_START32 __aeabi_read_tp
	ldr r0, =__sched_state
	ldr r0, [r0, #0]    @ ThrSchedState::cur
	ldr r0, [r0, #18*4] @ Thread::tp
	bx  lr
FUNC_END
