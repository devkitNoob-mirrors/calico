#pragma once
#if !defined(__NDS__) || !defined(ARM9)
#error "This header file is only for NDS ARM9"
#endif

#include "../mic.h"

/*! @addtogroup mic
	@{
*/

MK_EXTERN_C_START

typedef void (*MicBufferFn)(void* user, void* buf, size_t byte_sz);

void micInit(void);

bool micSetCpuTimer(unsigned prescaler, unsigned period);

bool micSetDmaRate(MicRate rate);

void micSetCallback(MicBufferFn fn, void* user);

bool micStart(void* buf, size_t byte_sz, MicFmt fmt, MicMode mode);

void micStop(void);

MK_EXTERN_C_END

//! @}
