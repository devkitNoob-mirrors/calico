#pragma once

#if defined(IS_GBA) || defined(IS_DS7)
.arch armv4t
#elif defined(IS_DS9) || defined(IS_3DS)
.arch armv5te
#endif

.fpu softvfp

.cfi_sections .debug_frame

.macro FUNC_START32 name, section=text, linkage=global
	.section  .\section\().\name, "ax", %progbits
	.align    2
	.\linkage \name
	.arm
	.type     \name, %function
\name:
	.fnstart
	.cfi_startproc
.endm

.macro FUNC_START16 name, section=text, linkage=global
	.section  .\section\().\name, "ax", %progbits
	.align    1
	.\linkage \name
	.thumb_func
	.type     \name, %function
\name:
	.fnstart
	.cfi_startproc
.endm

.macro FUNC_END
	.pool
	.cfi_endproc
	.cantunwind
	.fnend
.endm
