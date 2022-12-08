#pragma once
#if !defined(__NDS__)
#error "This header file is only for NDS"
#endif

#include "../types.h"
#include "io.h"

MEOW_INLINE bool systemIsTwlMode(void)
{
	extern bool g_isTwlMode;
	return g_isTwlMode;
}
