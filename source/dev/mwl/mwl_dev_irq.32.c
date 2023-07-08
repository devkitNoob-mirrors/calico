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

static void _mwlIrqPreTbtt(void)
{
	// Below only applies if we are associated and have configured a threshold
	if (s_mwlState.status != MwlStatus_Class3 || !s_mwlState.beacon_loss_thr) {
		return;
	}

	// Increment the beacon loss counter, raising an error if it exceeds the threshold.
	// Note that it is set back to 0 if we subsequently receive a beacon.
	// XX: This is only supposed to be done during the listen interval.
	// For now (as we don't support powersave mode) do it always.
	if (++s_mwlState.beacon_loss_cnt > s_mwlState.beacon_loss_thr) {
		s_mwlState.has_beacon_sync = 0;
		s_mwlState.beacon_loss_cnt = 0;
		s_mwlState.beacon_loss_thr = 0;
		_mwlPushTask(MwlTask_BeaconLost);
	}
}

static void _mwlIrqTbttGuest(void)
{
	MWL_REG(W_POWER_TX) |= 1;

	// XX: Power saving mode handling for listen interval/DTIM counters goes here

	// Reenable TX queues (HW disables them on TBTT IRQ)
	MWL_REG(W_TXREQ_SET) = 0xd;
}

static void _mwlIrqRxEnd(void)
{
	s_mwlState.rx_wrcsr = MWL_REG(W_RXBUF_WRCSR);
	_mwlPushTask(MwlTask_RxEnd);
}

static void _mwlIrqTxEnd(void)
{
	_mwlPushTask(MwlTask_TxEnd);
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
			_mwlIrqPreTbtt();
		}

		if (cur_irq & W_IRQ_TBTT) {
			if (s_mwlState.mode >= MwlMode_LocalGuest) {
				_mwlIrqTbttGuest();
			}
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
			_mwlIrqTxEnd();
		}

		if (cur_irq & W_IRQ_MP_END) {
			//
		}
	}
}
