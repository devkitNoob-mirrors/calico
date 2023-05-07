#pragma once
#include "../types.h"
#include "thread.h"

MEOW_EXTERN_C_START

typedef struct Mutex {
	Thread* owner;
} Mutex;

typedef struct RMutex {
	Mutex mutex;
	u32 counter;
} RMutex;

MEOW_INLINE bool mutexIsLockedByCurrentThread(Mutex* m)
{
	return m->owner == threadGetSelf();
}

void mutexLock(Mutex* m);
void mutexUnlock(Mutex* m);

MEOW_INLINE void rmutexLock(RMutex* m)
{
	if (mutexIsLockedByCurrentThread(&m->mutex)) {
		m->counter ++;
	} else {
		mutexLock(&m->mutex);
		m->counter = 1;
	}
}

MEOW_INLINE void rmutexUnlock(RMutex* m)
{
	if (!--m->counter) {
		mutexUnlock(&m->mutex);
	}
}

MEOW_EXTERN_C_END
