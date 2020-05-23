#pragma once

#if __ARM_ARCH_4T__
.arch armv4t
#elif __ARM_ARCH_5TE__
.arch armv5te
#else
#error "Unsupported architecture"
#endif

.fpu softvfp

.cfi_sections .debug_frame

.macro SECT_TEXT name, align=2, section=text
	.section .\section\().\name, "ax", %progbits
	.align   \align
.endm

.macro SECT_RODATA name, align=2, section=rodata
	.section .\section\().\name, "a"
	.align   \align
.endm

.macro SECT_DATA name, align=2, section=data
	.section .\section\().\name, "aw"
	.align   \align
.endm

.macro SECT_BSS name, align=2, section=bss
	.section .\section\().\name, "aw", %nobits
	.align   \align
.endm

.macro FUNC_START32 name, section=text, linkage=global
	SECT_TEXT \name
	.\linkage \name
	.arm
	.type     \name, %function
\name:
	.fnstart
	.cfi_startproc
.endm

.macro FUNC_START16 name, section=text, linkage=global
	SECT_TEXT \name, 1
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
