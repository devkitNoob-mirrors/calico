#include <calico/asm.inc>
#include <calico/arm/cp15.h>
#include <calico/nds/mm.h>

FUNC_START32 crt0SetupMPU

	push  {r4-r11}

	adr   r3, .LmpuRegions
	cmp   r0, #0
	ldm   r3, {r4-r11}

	orreq r7, r7, #CP15_PU_4M  @ TODO: debug DS
	orrne r7, r7, #CP15_PU_16M

	mcr   p15, 0, r4,  c6, c0, 0
	mcr   p15, 0, r5,  c6, c1, 0
	mcr   p15, 0, r6,  c6, c2, 0
	mcr   p15, 0, r7,  c6, c3, 0
	mcr   p15, 0, r8,  c6, c4, 0
	mcr   p15, 0, r9,  c6, c5, 0
	mcr   p15, 0, r10, c6, c6, 0
	mcr   p15, 0, r11, c6, c7, 0

	mov   r3, #(1<<3)
	mcr   p15, 0, r3, c3, c0, 0
	mov   r3, #(1<<3) | (1<<6)
	mcr   p15, 0, r3, c2, c0, 0
	mcr   p15, 0, r3, c2, c0, 1
	ldr   r3, =0x66606000
	mcr   p15, 0, r3, c5, c0, 3
	ldr   r3, =0x06333003
	mcr   p15, 0, r3, c5, c0, 2

	mrc   p15, 0, r3, c1, c0, 0
	ldr   r2, =(CP15_CR_PU_ENABLE | CP15_CR_DCACHE_ENABLE | CP15_CR_ICACHE_ENABLE | CP15_CR_ROUND_ROBIN)
	orr   r3, r3, r2
	mcr   p15, 0, r3, c1, c0, 0

	pop   {r4-r11}
	bx    lr

.LmpuRegions:
	.word CP15_PU_ENABLE | CP15_PU_64M | MM_IO
	.word 0
	.word 0
	.word CP15_PU_ENABLE | MM_MAINRAM
	.word CP15_PU_ENABLE | CP15_PU_64K | MM_DTCM
	.word CP15_PU_ENABLE | CP15_PU_32K | MM_ITCM
	.word CP15_PU_ENABLE | CP15_PU_32K | MM_BIOS
	.word CP15_PU_ENABLE | ((5-1)<<1)

FUNC_END

FUNC_START32 crt0FlushCaches

	@ Flush the data cache (store dirty lines + invalidate)
	mov  r1, #0
.Lclean_set:
	mov  r0, #0
.Lclean_line:
	orr  r2, r1, r0
	mcr  p15, 0, r2, c7, c14, 2
	add  r0, r0, #ARM_CACHE_LINE_SZ
	cmp  r0, #ARM_DCACHE_SZ/ARM_DCACHE_SETS
	bne  .Lclean_line
	adds r1, r1, #(1 << (32 - ARM_DCACHE_SETS_LOG2))
	bne  .Lclean_set

	@ Drain the write buffer
	mcr  p15, 0, r1, c7, c10, 4

	@ Invalidate the instruction cache
	mcr  p15, 0, r1, c7, c5, 0

	bx   lr

FUNC_END

