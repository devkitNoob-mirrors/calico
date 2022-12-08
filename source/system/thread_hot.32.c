#include "thread-priv.h"

ThrSchedState __sched_state;
IrqHandler __irq_table[MEOW_IRQ_NUM_HANDLERS];

static void threadReschedule(Thread* t, ArmIrqState st)
{
	if_unlikely (s_curThread->status == ThrStatus_Finished)
		return;

	if_likely ((armGetCpsr() & ARM_PSR_MODE_MASK) == ARM_PSR_MODE_IRQ) {
		if (!s_deferredThread || t->prio < s_deferredThread->prio)
			s_deferredThread = t;
		armIrqUnlockByPsr(st);
		return;
	}

	threadSwitchTo(t, st);
}

void threadBlock(ThrListNode* queue, u32 token)
{
	ArmIrqState st = armIrqLockByPsr();
	s_curThread->status = ThrStatus_Waiting;
	s_curThread->token = token;
	threadLinkEnqueue(queue, s_curThread);
	threadReschedule(threadFindRunnable(s_firstThread), st);
}

MEOW_INLINE void _threadUnblockCommon(ThrListNode* queue, int max, ThrUnblockMode mode, u32 ref)
{
	//if (max == 0) return;
	//if (max < 0) max = -1;

	ArmIrqState st = armIrqLockByPsr();
	Thread* resched = NULL;
	Thread* next;

	for (Thread* cur = queue->next; max != 0 && cur; cur = next) {
		next = cur->link.next;
		if (threadTestUnblock(cur, mode, ref)) {
			if (!resched)
				resched = cur; // Remember the first unblocked (highest priority) thread
			cur->status = ThrStatus_Running;
			if (mode == ThrUnblockMode_ByMask)
				cur->token &= ref;
			threadLinkDequeue(queue, cur);
			if (max > 0) --max;
			//max = (max > 0) ? (max - 1) : max;
		}
	}

	if (resched && resched->prio < s_curThread->prio)
		threadReschedule(resched, st);
	else
		armIrqUnlockByPsr(st);
}

void threadUnblock(ThrListNode* queue, int max, ThrUnblockMode mode, u32 ref)
{
	_threadUnblockCommon(queue, max, mode, ref);
}

void threadUnblockOneByValue(ThrListNode* queue, u32 ref)
{
	_threadUnblockCommon(queue, +1, ThrUnblockMode_ByValue, ref);
}

void threadUnblockOneByMask(ThrListNode* queue, u32 ref)
{
	_threadUnblockCommon(queue, +1, ThrUnblockMode_ByMask, ref);
}

void threadUnblockAllByValue(ThrListNode* queue, u32 ref)
{
	_threadUnblockCommon(queue, -1, ThrUnblockMode_ByValue, ref);
}

void threadUnblockAllByMask(ThrListNode* queue, u32 ref)
{
	_threadUnblockCommon(queue, -1, ThrUnblockMode_ByMask, ref);
}
