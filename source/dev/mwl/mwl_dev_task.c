#include "common.h"
#include <calico/system/thread.h>

MwlState s_mwlState;

static Thread s_mwlThread;
alignas(8) static u8 s_mwlThreadStack[0x600];

typedef void (*MwlTaskHandler)(void);

static void _mwlExitTask(void);
void _mwlTxEndTask(void);
void _mwlRxEndTask(void);
void _mwlRxMgmtCtrlTask(void);
void _mwlMlmeTask(void);

static const MwlTaskHandler s_mwlTaskHandlers[MwlTask__Count] = {
	[MwlTask_ExitThread]      = _mwlExitTask,
	[MwlTask_TxEnd]           = _mwlTxEndTask,
	[MwlTask_RxEnd]           = _mwlRxEndTask,
	[MwlTask_RxMgmtCtrlFrame] = _mwlRxMgmtCtrlTask,
	[MwlTask_MlmeProcess]     = _mwlMlmeTask,
};

static int _mwlTaskHandler(void* arg)
{
	dietPrint("[MWL] Thread start\n");

	MwlTask id;
	do {
		id = _mwlPopTask();

		MwlTaskHandler h = s_mwlTaskHandlers[id];
		if (h) {
			h();
		}
	} while (id != MwlTask_ExitThread);

	dietPrint("[MWL] Thread end\n");
	return 0;
}

void mwlDevStart(void)
{
	mwlDevStop();

	unsigned mode = s_mwlState.mode;
	if (mode == MwlMode_Test) {
		return;
	}

	// Install ISR
	irqSet(IRQ_WIFI, _mwlIrqHandler);
	irqEnable(IRQ_WIFI);

	// Start task handler thread
	s_mwlState.task_mask = 0;
	threadPrepare(&s_mwlThread, _mwlTaskHandler, NULL, &s_mwlThreadStack[sizeof(s_mwlThreadStack)], 0x11);
	threadStart(&s_mwlThread);

	// Set mode, disable powersave
	MWL_REG(W_MODE_WEP) = (MWL_REG(W_MODE_WEP) &~ 0x47) | mode;

	MWL_REG(W_WEP_CNT) = 0x8000;
	MWL_REG(W_POST_BEACON) = 0xffff;
	MWL_REG(W_AID_FULL) = 0;
	MWL_REG(W_AID_LOW) = 0;
	MWL_REG(W_POWER_TX) = 0xf;

	// Enable TX LOC1/LOC2/LOC3 if needed
	// XX: how on earth does LocalHost send mgmt frames then?
	if (mode >= MwlMode_LocalGuest) {
		MWL_REG(W_TXREQ_SET) = 0xd;
	}

	// Configure RX
	unsigned rxbuf = s_mwlState.rx_pos;
	MWL_REG(W_RXCNT) = 0x8000;
	MWL_REG(W_RXBUF_BEGIN) = MWL_MAC_RAM + rxbuf;
	MWL_REG(W_RXBUF_WR_ADDR) = rxbuf/2;
	MWL_REG(W_RXBUF_END) = MWL_MAC_RAM + MWL_MAC_RAM_SZ;
	MWL_REG(W_RXBUF_READCSR) = rxbuf/2;
	MWL_REG(W_RXBUF_GAP) = MWL_MAC_RAM + MWL_MAC_RAM_SZ - 2;
	MWL_REG(W_RXCNT) = 0x8001;
	MWL_REG(0x24c) = 0xffff;
	MWL_REG(0x24e) = 0xffff;
	g_mwlMacVars->unk_0x10 = 0xffff; // ??
	g_mwlMacVars->unk_0x12 = 0xffff; // ??
	g_mwlMacVars->unk_0x1e = 0xffff; // ??
	g_mwlMacVars->unk_0x16 = 0xffff; // ??

	//MWL_REG(W_RXCNT) = 0x8000; // redundant
	MWL_REG(W_IF) = 0xffff;

	// Set up statistics. XX: We decide against using statistics
	MWL_REG(W_RXSTAT_OVF_IE) = 0;
	MWL_REG(W_RXSTAT_INC_IE) = 0;
	MWL_REG(W_TXSTATCNT) = 0;
	MWL_REG(0x00a) = 0;

	unsigned ie = W_IRQ_RX_END | W_IRQ_TX_END | W_IRQ_RX_CNT_INC | W_IRQ_TX_ERR | W_IRQ_TBTT;
	unsigned rxfilter = (1U<<0);
	unsigned rxfilter2 = (1U<<0) | (1U<<3);

	switch (mode) {
		default: break;

		case MwlMode_LocalHost:
			ie |= W_IRQ_MP_END | W_IRQ_POST_TBTT;
			rxfilter |= (1U<<8) | (1U<<9);
			rxfilter2 |= (1U<<2);
			break;

		case MwlMode_LocalGuest:
			ie |= W_IRQ_RX_START | W_IRQ_TX_START | W_IRQ_POST_TBTT | W_IRQ_PRE_TBTT;
			rxfilter |= (1U<<7) | (1U<<8);
			rxfilter2 |= (1U<<1);
			break;

		case MwlMode_Infra:
			ie |= W_IRQ_PRE_TBTT;
			rxfilter2 |= (1U<<1);
			break;
	}

	// If the current BSSID is multicast, enable RXFILTER.bit10.
	// XX: It is currently unknown what this does. Regardless of this bit, all beacon
	// frames from all BSSes are received (and bit15 of the RX status field is set when
	// the frame matches the current BSSID). Maybe bit10 only affects data packets?
	// I.e. bit10 = promiscuous mode.
	if (mode >= MwlMode_LocalGuest && (s_mwlState.bssid[0] & 1)) {
		rxfilter |= 0x400;
	}

	MWL_REG(W_IE) = ie;
	// XX: LocalGuest mode would do something with RXSTAT irqs here for the workaround involving rx_start irq
	MWL_REG(W_RXFILTER) = rxfilter;
	MWL_REG(W_RXFILTER2) = rxfilter2;
	MWL_REG(W_MODE_RST) = 1;

	// XX: I like neat things, and as a result I clear the MHz counter
	MWL_REG(W_US_COUNT0) = 0;
	MWL_REG(W_US_COUNT1) = 0;
	MWL_REG(W_US_COUNT2) = 0;
	MWL_REG(W_US_COUNT3) = 0;

	// XX: LocalHost mode would set up beacon transfer and set W_US_COMPARE* (with bit0) here

	// Enable counters
	MWL_REG(W_US_COUNTCNT) = 1;
	MWL_REG(W_US_COMPARECNT) = 1;

	// Set current STA status
	s_mwlState.status = MwlStatus_Class1; // XX: LocalHost goes straight to Class3 (when beacons are being transferred)

	// Do stuff I guess
	MWL_REG(0x048) = 0;
	MWL_REG(W_POWER_TX) &= ~2; // disable multipoll powersave
	MWL_REG(0x048) = 0;
	MWL_REG(W_TXREQ_SET) = 2; // XX: Enable TX CMD... even for LocalGuest/Infra?!

	// Power on the hardware
	MWL_REG(W_POWERFORCE) = 0x8000;
	MWL_REG(W_POWERSTATE) = 2;
	while (!(MWL_REG(W_RF_PINS) & 0x80));
}

void mwlDevStop(void)
{
	// Tear down task handler thread if it is active
	if (s_mwlThread.status >= ThrStatus_Running) {
		_mwlPushTask(MwlTask_ExitThread);
		if (&s_mwlThread != threadGetSelf()) {
			// Wait for the thread to exit
			threadJoin(&s_mwlThread);
		}
	}

	irqDisable(IRQ_WIFI);

	MWL_REG(W_IE) = 0;
	MWL_REG(W_MODE_RST) = 0;
	MWL_REG(W_US_COMPARECNT) = 0;
	MWL_REG(W_US_COUNTCNT) = 0;
	MWL_REG(W_TXSTATCNT) = 0;
	MWL_REG(0x00a) = 0;
	MWL_REG(W_TXBUF_BEACON) = 0;
	MWL_REG(W_TXREQ_RESET) = 0xffff;
	MWL_REG(W_TXBUF_RESET) = 0xffff;
}

void mwlDevGracefulStop(void)
{
	s_mwlState.mlme_cb.onStateLost = (void*)mwlDevStop;
	if (mwlMlmeDeauthenticate()) {
		threadJoin(&s_mwlThread);
	} else {
		mwlDevStop();
	}
}

void _mwlExitTask(void)
{
	// Stop ticktasks
	tickTaskStop(&s_mwlState.timeout_task);
	tickTaskStop(&s_mwlState.periodic_task);

	// Free any unsent authentication packets
	if (s_mwlState.mlme_state == MwlMlmeState_AuthBusy || s_mwlState.mlme_state == MwlMlmeState_AuthDone) {
		_mwlMlmeAuthFreeReply();
	}

	s_mwlState.status = MwlStatus_Idle;
	s_mwlState.has_beacon_sync = 0;
	s_mwlState.is_power_save = 0;
	s_mwlState.mlme_state = MwlMlmeState_Idle;

	_mwlRxQueueClear();
	for (unsigned i = 0; i < 3; i ++) {
		_mwlTxQueueClear(i);
	}
}
