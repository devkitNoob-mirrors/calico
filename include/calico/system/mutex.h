#pragma once
#include "../types.h"
#include "thread.h"

/*! @addtogroup sync
	@{
*/
/*! @name Mutex
	The quintessential synchronization primitive, with priority inheritance.
	@{
*/

MK_EXTERN_C_START

typedef struct Mutex {
	Thread* owner;
} Mutex;

typedef struct RMutex {
	Mutex mutex;
	u32 counter;
} RMutex;

MK_INLINE bool mutexIsLockedByCurrentThread(Mutex* m)
{
	return m->owner == threadGetSelf();
}

void mutexLock(Mutex* m);
void mutexUnlock(Mutex* m);

MK_INLINE void rmutexLock(RMutex* m)
{
	if (mutexIsLockedByCurrentThread(&m->mutex)) {
		m->counter ++;
	} else {
		mutexLock(&m->mutex);
		m->counter = 1;
	}
}

MK_INLINE void rmutexUnlock(RMutex* m)
{
	if (!--m->counter) {
		mutexUnlock(&m->mutex);
	}
}

MK_EXTERN_C_END

//! @}

//! @}
