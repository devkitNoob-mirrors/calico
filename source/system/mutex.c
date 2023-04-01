#include <calico/types.h>
#include <calico/arm/common.h>
#include <calico/system/mutex.h>
#include "thread-priv.h"

static void threadUpdateDynamicPrio(Thread* t)
{
	for (;;) {
		// Calculate the expected dynamic priority of the thread
		unsigned prio = t->baseprio;
		if_unlikely (t->waiters.next) {
			unsigned waiter_prio = t->waiters.next->prio;
			prio = waiter_prio < prio ? waiter_prio : prio;
		}

		// If the priority is the same we're done
		if_likely (t->prio == prio) {
			break;
		}

		// Requeue the thread
		threadDequeue(t);
		t->prio = prio;
		threadEnqueue(t);

		// If the thread is running we're done
		if_likely (threadIsRunning(t)) {
			break;
		}

		// Reflect the bumped priority too in the queue the thread is blocked on
		ThrListNode* queue = t->queue;
		threadLinkDequeue(queue, t);
		threadLinkEnqueue(queue, t);

		// If the thread is not waiting on a mutex, we're done
		if_likely (!threadIsWaitingOnMutex(t)) {
			break;
		}

		// Update the holder thread's dynamic priority as well
		t = ((Mutex*)t->token)->owner;
	}
}

static Thread* threadRemoveWaiter(Thread* t, u32 token)
{
	Thread* next_owner = NULL;
	Thread* next_waiter;
	for (Thread* cur = t->waiters.next; cur; cur = next_waiter) {
		next_waiter = cur->link.next;
		if_likely (cur->token == token) {
			threadLinkDequeue(&t->waiters, cur);
			if_likely (!next_owner) {
				next_owner = cur;
				cur->status = ThrStatus_Running;
			} else {
				// TODO: Both the source list and the target list are sorted.
				// Should we try to optimize this to take that into account?
				threadLinkEnqueue(&next_owner->waiters, cur);
			}
		}
	}

	// Update dynamic priorities for both threads if needed
	if_unlikely (next_owner) {
		threadUpdateDynamicPrio(t);
		threadUpdateDynamicPrio(next_owner);
	}

	return next_owner;
}

void mutexLock(Mutex* m)
{
	Thread* self = threadGetSelf();
	ArmIrqState st = armIrqLockByPsr();

	if_likely (!m->owner) {
		// Fast path: success
		m->owner = self;
		armIrqUnlockByPsr(st);
	} else {
		// Add current thread to owner thread's list of waiters
		self->status = ThrStatus_WaitingOnMutex;
		self->token = (u32)m;
		threadLinkEnqueue(&m->owner->waiters, self);

		// Bump dynamic priority of owner thread if needed
		Thread* next = NULL;
		if_unlikely (self->prio < m->owner->prio) {
			threadUpdateDynamicPrio(m->owner);

			// Fast path when the owner thread is runnable
			if_likely (threadIsRunning(m->owner)) {
				next = m->owner;
			}
		}

		// Select next thread to run if above code didn't
		if_likely (!next) {
			next = threadFindRunnable(s_firstThread);
		}

		threadSwitchTo(next, st);
	}
}

void mutexUnlock(Mutex* m)
{
	Thread* self = threadGetSelf();
	ArmIrqState st = armIrqLockByPsr();

	if_unlikely (m->owner != self) {
		for (;;); // ERROR
	}

	unsigned old_prio = self->prio;
	m->owner = threadRemoveWaiter(self, (u32)m);

	Thread* next = self;
	if_unlikely (old_prio < self->prio) {
		next = threadFindRunnable(s_firstThread);
	}

	if_unlikely (next != self)
		threadSwitchTo(next, st);
	else
		armIrqUnlockByPsr(st);
}
