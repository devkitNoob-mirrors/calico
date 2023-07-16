#pragma once
#if __ARM_ARCH < 5
#error "This header is for ARMv5+ only"
#endif

#include "../types.h"
#include "cp15.h"

MK_EXTERN_C_START

void armDrainWriteBuffer(void);

void armDCacheFlushAll(void);
void armDCacheFlush(void* addr, size_t size);
void armDCacheInvalidate(void* addr, size_t size);

void armICacheInvalidateAll(void);
void armICacheInvalidate(void* addr, size_t size);

MK_EXTERN_C_END
