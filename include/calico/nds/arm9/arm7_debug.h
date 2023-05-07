#pragma once
#if !defined(__NDS__) || !defined(ARM9)
#error "This header file is only for NDS ARM9"
#endif

#include "../../types.h"

MEOW_EXTERN_C_START

typedef void (* Arm7DebugFn)(const char* buf, size_t size);

void installArm7DebugSupport(Arm7DebugFn fn, u8 thread_prio);

MEOW_EXTERN_C_END
