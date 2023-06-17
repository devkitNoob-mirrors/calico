#include "common.h"
#include <calico/system/thread.h>

static ThrListNode s_mwlTaskQueue;

void _mwlPushTaskImpl(u32 mask)
{
	ArmIrqState st = armIrqLockByPsr();

	u32 cur_mask = s_mwlState.task_mask;
	u32 new_mask = mask | cur_mask;

	if_likely (new_mask != cur_mask) {
		s_mwlState.task_mask = new_mask;
		if_likely (s_mwlTaskQueue.next != NULL) {
			threadUnblockOneByValue(&s_mwlTaskQueue, 0);
		}
	}

	armIrqUnlockByPsr(st);
}

MwlTask _mwlPopTask(void)
{
	ArmIrqState st = armIrqLockByPsr();

	u32 mask;
	while (!(mask = s_mwlState.task_mask)) {
		threadBlock(&s_mwlTaskQueue, 0);
	}

	unsigned i;
	for (i = 0; i < 32; i ++) {
		unsigned bit = 1U<<i;
		if (mask & bit) {
			s_mwlState.task_mask = mask &~ bit;
			break;
		}
	}

	armIrqUnlockByPsr(st);
	return (MwlTask)i;
}

//-----------------------------------------------------------------------------

MEOW_NOINLINE static void _mwlIrqRxEnd(void)
{
	s_mwlState.rx_wrcsr = MWL_REG(W_RXBUF_WRCSR);
	_mwlPushTask(MwlTask_RxEnd);
}

void _mwlIrqHandler(void)
{
	for (;;) {
		unsigned cur_irq = MWL_REG(W_IE) & MWL_REG(W_IF);
		if (!cur_irq) {
			break;
		}
		MWL_REG(W_IF) = cur_irq;

		if (cur_irq & W_IRQ_TX_START) {
			//
		}

		if (cur_irq & W_IRQ_RX_START) {
			//
		}

		if (cur_irq & W_IRQ_PRE_TBTT) {
			//
		}

		if (cur_irq & W_IRQ_TBTT) {
			//
		}

		if (cur_irq & W_IRQ_POST_TBTT) {
			//
		}

		if (cur_irq & W_IRQ_TX_ERR) {
			//
		}

		if (cur_irq & W_IRQ_RX_CNT_INC) {
			//
		}

		if (cur_irq & W_IRQ_RX_END) {
			_mwlIrqRxEnd();
		}

		if (cur_irq & W_IRQ_RX_CNT_OVF) {
			//
		}

		if (cur_irq & W_IRQ_TX_END) {
			//
		}

		if (cur_irq & W_IRQ_MP_END) {
			//
		}
	}
}
