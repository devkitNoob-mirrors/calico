#pragma once
#include "../types.h"
#include "../arm/common.h"
#include "irq.h"
#include "tick.h"

/*! @addtogroup thread
	@{
*/

MK_EXTERN_C_START

typedef struct Thread Thread;

typedef struct ThrListNode {
	Thread* next;
	Thread* prev;
} ThrListNode;

typedef enum ThrStatus {
	ThrStatus_Uninitialized,
	ThrStatus_Finished,
	ThrStatus_Running,
	ThrStatus_Waiting,
	ThrStatus_WaitingOnMutex,
} ThrStatus;

typedef enum ThrUnblockMode {
	ThrUnblockMode_Any,
	ThrUnblockMode_ByValue,
	ThrUnblockMode_ByMask,
} ThrUnblockMode;

#define THREAD_MAX_PRIO 0x00
#define THREAD_MIN_PRIO 0x3F

#define MAIN_THREAD_PRIO 0x1C

typedef int (* ThreadFunc)(void* arg);

struct Thread {
	ArmContext ctx;

	void* tp;
	void* impure;

	Thread* next;
	ThrStatus status;
	u8 prio, baseprio;

	ThrListNode waiters;

	union {
		// Data for waiting threads
		struct {
			ThrListNode link;
			ThrListNode* queue;
			u32 token;
		};
		// Data for finished threads
		struct {
			int rc;
		};
	};
};

typedef struct ThrSchedState {
	Thread *cur;
	Thread *deferred;
	Thread *first;
	IrqMask irqWaitMask;
	ThrListNode irqWaitList;
#if MK_IRQ_NUM_HANDLERS > 32
	IrqMask irqWaitMask2;
	ThrListNode irqWaitList2;
#endif
} ThrSchedState;

void threadPrepare(Thread* t, ThreadFunc entrypoint, void* arg, void* stack_top, u8 prio);
size_t threadGetLocalStorageSize(void);
void threadAttachLocalStorage(Thread* t, void* storage);
void threadStart(Thread* t);
void threadFree(Thread* t);
int threadJoin(Thread* t);

void threadYield(void);
u32  threadIrqWait(bool next_irq, IrqMask mask);
#if MK_IRQ_NUM_HANDLERS > 32
u32  threadIrqWait2(bool next_irq, IrqMask mask);
#endif
void threadExit(int rc) MK_NORETURN;

MK_EXTERN32 void threadBlock(ThrListNode* queue, u32 token);
MK_EXTERN32 void threadUnblock(ThrListNode* queue, int max, ThrUnblockMode mode, u32 ref);

MK_EXTERN32 void threadUnblockOneByValue(ThrListNode* queue, u32 ref);
MK_EXTERN32 void threadUnblockOneByMask(ThrListNode* queue, u32 ref);
MK_EXTERN32 void threadUnblockAllByValue(ThrListNode* queue, u32 ref);
MK_EXTERN32 void threadUnblockAllByMask(ThrListNode* queue, u32 ref);

void threadSleepTicks(u32 ticks);
void threadTimerStartTicks(TickTask* task, u32 period_ticks);
void threadTimerWait(TickTask* task);

MK_INLINE void threadSleep(u32 usec)
{
	threadSleepTicks(ticksFromUsec(usec));
}

MK_INLINE void threadTimerStart(TickTask* task, u32 period_hz)
{
	threadTimerStartTicks(task, ticksFromHz(period_hz));
}

MK_INLINE Thread* threadGetSelf(void)
{
	extern ThrSchedState __sched_state;
	return __sched_state.cur;
}

MK_CONSTEXPR bool threadIsValid(Thread* t)
{
	return t->status != ThrStatus_Uninitialized;
}

MK_CONSTEXPR bool threadIsWaiting(Thread* t)
{
	return t->status >= ThrStatus_Waiting;
}

MK_CONSTEXPR bool threadIsWaitingOnMutex(Thread* t)
{
	return t->status == ThrStatus_WaitingOnMutex;
}

MK_CONSTEXPR bool threadIsRunning(Thread* t)
{
	return t->status == ThrStatus_Running;
}

MK_CONSTEXPR bool threadIsFinished(Thread* t)
{
	return t->status == ThrStatus_Finished;
}

#if defined(IRQ_VBLANK)

MK_INLINE void threadWaitForVBlank(void)
{
	threadIrqWait(true, IRQ_VBLANK);
}

#endif

MK_EXTERN_C_END

//! @}
