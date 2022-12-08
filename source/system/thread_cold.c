#include "thread-priv.h"

#if defined(__GBA__) || (defined(__NDS__) && defined(ARM7))
void svcHalt(void);

//static alignas(8) u32 s_idleThreadStack[6];
// Using the bottommost 6 words of the IRQ stack as the idle thread stack
extern u32 __sp_usr[];
#define s_idleThreadStack __sp_usr
#endif

static Thread s_mainThread, s_idleThread;
static ThrListNode s_joinThreads;

void _threadInit(void)
{
	// Set up main thread (which is also the current one)
	s_firstThread          = &s_mainThread;
	s_curThread            = &s_mainThread;
	s_mainThread.next      = &s_idleThread;
	s_mainThread.status    = ThrStatus_Running;
	s_mainThread.prio      = MAIN_THREAD_PRIO;
	s_mainThread.baseprio  = s_mainThread.prio;

	// Set up idle thread
	s_idleThread.ctx.psr   = ARM_PSR_MODE_SYS;
#if __ARM_ARCH >= 5
	s_idleThread.ctx.r[15] = (u32)armWaitForIrq;
	s_idleThread.ctx.r[14] = s_idleThread.ctx.r[15];
#elif defined(__GBA__) || defined(__NDS__)
	s_idleThread.ctx.r[14] = (u32)svcHalt;
	s_idleThread.ctx.r[15] = s_idleThread.ctx.r[14] - 1;
	s_idleThread.ctx.r[13] = (u32)&s_idleThreadStack[2];
	s_idleThread.ctx.sp_svc = (u32)&s_idleThreadStack[6];
	s_idleThread.ctx.psr  |= ARM_PSR_T;
#else
#error "This ARM7 platform is not yet supported"
#endif
	s_idleThread.status    = ThrStatus_Running;
	s_idleThread.prio      = THREAD_MIN_PRIO+1;
	s_idleThread.baseprio  = s_idleThread.prio;
}

void threadPrepare(Thread* t, ThreadFunc entrypoint, void* arg, void* stack_top, u8 prio)
{
	// Initialize thread state and context
	memset(t, 0, sizeof(Thread));
	t->ctx.r[0]   = (u32)arg;
	t->ctx.sp_svc = (u32)stack_top &~ 7;
	t->ctx.r[13]  = t->ctx.sp_svc - 0x10;
	t->ctx.r[14]  = (u32)threadExit;
	t->ctx.r[15]  = (u32)entrypoint;
	t->ctx.psr    = ARM_PSR_MODE_SYS;
	t->status     = ThrStatus_Running;
	t->prio       = prio & THREAD_MIN_PRIO;
	t->baseprio   = t->prio;

	// Adjust THUMB entrypoints
	if (t->ctx.r[15] & 1) {
		t->ctx.r[15] &= ~1;
		t->ctx.psr   |= ARM_PSR_T;
	}
}

void threadStart(Thread* t)
{
	// Add thread to queue
	ArmIrqState st = armIrqLockByPsr();
	threadEnqueue(t);
	if (t->prio < s_curThread->prio)
		threadSwitchTo(t, st);
	else
		armIrqUnlockByPsr(st);
}

void threadFree(Thread* t)
{
	// Wait for the thread to finish if it's not already finished
	if (!threadIsFinished(t))
		threadJoin(t);

	// TODO: destruct callback goes here
}

void threadJoin(Thread* t)
{
	ArmIrqState st = armIrqLockByPsr();

	// Block on thread if it's not already finished
	if (!threadIsFinished(t))
		threadBlock(&s_joinThreads, (u32)t);

	armIrqUnlockByPsr(st);
}

void threadYield(bool generous)
{
	ArmIrqState st = armIrqLockByPsr();

	Thread* t = threadFindRunnable(s_curThread->next);
	if (t == &s_idleThread || (!generous && t->prio > s_curThread->prio))
		t = threadFindRunnable(s_firstThread);
	if (t != s_curThread)
		threadSwitchTo(t, st);
	else
		armIrqUnlockByPsr(st);
}

MEOW_INLINE u32 _threadIrqWaitImpl(bool next_irq, IrqMask mask, ThrListNode* wait_list, IrqMask* wait_mask, volatile IrqMask* flags)
{
	if (!mask) return 0;
	ArmIrqState st = armIrqLockByPsr();

	IrqMask cur_flags = *flags;
	IrqMask test = cur_flags & mask;
	if (test) {
		*flags = cur_flags ^ test;
		if (!next_irq) {
			armIrqUnlockByPsr(st);
			return test;
		}
	}

	*wait_mask |= mask;
	threadBlock(wait_list, mask);
	armIrqUnlockByPsr(st);
	return s_curThread->token;
}

u32 threadIrqWait(bool next_irq, IrqMask mask)
{
	return _threadIrqWaitImpl(next_irq, mask, &s_irqWaitList, &s_irqWaitMask, &__irq_flags);
}

#if MEOW_IRQ_NUM_HANDLERS > 32

u32 threadIrqWait2(bool next_irq, IrqMask mask)
{
	return _threadIrqWaitImpl(next_irq, mask, &s_irqWaitList2, &s_irqWaitMask2, &__irq_flags2);
}

#endif

void threadExit(int rc)
{
	armIrqLockByPsr();
	threadDequeue(s_curThread);
	s_curThread->status = ThrStatus_Finished;
	s_curThread->rc = rc;
	// TODO: exit callback goes here
	threadUnblock(&s_joinThreads, -1, ThrUnblockMode_ByValue, (u32)s_curThread);
	s_curThread = threadFindRunnable(s_firstThread);
	armContextLoad(&s_curThread->ctx);
}
