#pragma once
#include "../types.h"

#if defined(ARM9)
#define SMUTEX_MY_CPU_ID 9
#elif defined(ARM7)
#define SMUTEX_MY_CPU_ID 7
#else
#error "ARM9 or ARM7 must be defined"
#endif

MEOW_EXTERN_C_START

// Spin-mutex, or "Super-mutex"
typedef struct SMutex {
	u32 spinner;
	u32 thread_ptr : 28;
	u32 cpu_id : 4;
} SMutex;

MEOW_EXTERN32 void smutexLock(SMutex* m);
MEOW_EXTERN32 bool smutexTryLock(SMutex* m);
MEOW_EXTERN32 void smutexUnlock(SMutex* m);

MEOW_EXTERN_C_END
