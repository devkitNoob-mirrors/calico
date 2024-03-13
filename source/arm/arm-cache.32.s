#include <calico/asm.inc>
#include <calico/arm/cp15.h>

FUNC_START32 armDrainWriteBuffer

	mov  r0, #0
	mcr  p15, 0, r0, c7, c10, 4
	bx   lr

FUNC_END

FUNC_START32 armDCacheFlushAll

	@ Flush all cache lines
	mov  r1, #0
.Ldfa_clean_set:
	mov  r0, #0
.Ldfa_clean_line:
	orr  r2, r1, r0
	mcr  p15, 0, r2, c7, c14, 2 @ Clean+Invalidate DCache entry by Set/Way
	add  r0, r0, #ARM_CACHE_LINE_SZ
	cmp  r0, #ARM_DCACHE_SZ/ARM_DCACHE_WAYS
	bne  .Ldfa_clean_line
	adds r1, r1, #(1 << (32 - ARM_DCACHE_WAYS_LOG2))
	bne  .Ldfa_clean_set

	b    armDrainWriteBuffer

FUNC_END

FUNC_START32 armDCacheFlush

	add  r1, r0, r1
	bic  r0, r0, #(ARM_CACHE_LINE_SZ-1)
0:	mcr  p15, 0, r0, c7, c14, 1 @ Clean+Invalidate DCache entry by MVA
	add  r0, r0, #ARM_CACHE_LINE_SZ
	cmp  r0, r1
	bmi  0b

	b    armDrainWriteBuffer

FUNC_END


FUNC_START32 armDCacheInvalidate

	add  r1, r0, r1
	bic  r0, r0, #(ARM_CACHE_LINE_SZ-1)
0:	mcr  p15, 0, r0, c7, c6, 1 @ Invalidate DCache entry by MVA
	add  r0, r0, #ARM_CACHE_LINE_SZ
	cmp  r0, r1
	bmi  0b

	bx   lr

FUNC_END

FUNC_START32 armICacheInvalidateAll

	mov  r0, #0
	mcr  p15, 0, r0, c7, c5, 0
	bx   lr

FUNC_END

FUNC_START32 armICacheInvalidate

	add  r1, r0, r1
	bic  r0, r0, #(ARM_CACHE_LINE_SZ-1)
0:	mcr  p15, 0, r0, c7, c5, 1 @ Invalidate ICache entry by MVA
	add  r0, r0, #ARM_CACHE_LINE_SZ
	cmp  r0, r1
	bls  0b

	bx   lr

FUNC_END
