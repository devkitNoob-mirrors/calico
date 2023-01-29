#include <calico/types.h>
#include <calico/system/mutex.h>
#include <calico/system/thread.h>
#include <sys/iosupport.h>

#if defined(__NDS__)
#include "../nds/transfer.h"

MEOW_INLINE time_t _getUnixTime(void)
{
	return s_transferRegion->unix_time;
}

#else

MEOW_INLINE time_t _getUnixTime(void)
{
	return 0;
}

#endif

// Dummy symbol referenced by crt0 so that this object file is pulled in by the linker
const u32 __newlib_syscalls = 0xdeadbeef;

int __SYSCALL(gettod_r)(struct _reent* ptr, struct timeval* tp, struct timezone* tz)
{
	if (tp) {
		tp->tv_sec = _getUnixTime();
		tp->tv_usec = 0;
	}

	if (tz) {
		tz->tz_minuteswest = 0;
		tz->tz_dsttime = 0;
	}

	return 0;
}

MEOW_CODE32 int __SYSCALL(nanosleep)(const struct timespec* req, struct timespec* rem)
{
	threadSleep((unsigned)req->tv_sec*1000000U + (unsigned)req->tv_nsec/1000U);
	return 0;
}

void __SYSCALL(lock_init)(_LOCK_T* lock)
{
	*lock = (_LOCK_T){0};
}

void __SYSCALL(lock_acquire)(_LOCK_T* lock)
{
	mutexLock((Mutex*)lock);
}

void __SYSCALL(lock_release)(_LOCK_T* lock)
{
	mutexUnlock((Mutex*)lock);
}

void __SYSCALL(lock_init_recursive)(_LOCK_RECURSIVE_T* lock)
{
	*lock = (_LOCK_RECURSIVE_T){0};
}

void __SYSCALL(lock_acquire_recursive)(_LOCK_RECURSIVE_T* lock)
{
	rmutexLock((RMutex*)lock);
}

void __SYSCALL(lock_release_recursive)(_LOCK_RECURSIVE_T* lock)
{
	rmutexUnlock((RMutex*)lock);
}
