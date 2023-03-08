#include <calico/types.h>
#include <calico/system/thread.h>
#include <calico/system/mailbox.h>
#include <calico/nds/system.h>
#include <calico/nds/pm.h>
#include <calico/nds/pxi.h>
#include <calico/nds/wlmgr.h>
#include <calico/nds/arm7/twlwifi.h>
#include "../transfer.h"
#include "../pxi/wlmgr.h"

#include <calico/system/dietprint.h>

#define WLMGR_NUM_MAIL_SLOTS 4
#define WLMGR_MAIL_EXIT      0
#define WLMGR_MAIL_PXI       1
#define WLMGR_MAIL_PM        2
#define WLMGR_MAIL_EVENT     3

static Thread s_wlmgrThread;
alignas(8) static u8 s_wlmgrThreadStack[2048];

static struct {
	NetBufListNode tx_queue;
	WlMgrState state;
	Mailbox mbox;
	Mailbox pxi_mbox;
	bool using_twlwifi;
} s_wlmgrState;

MEOW_NOINLINE static void _wlmgrPxiProcess(Mailbox* mb);
MEOW_NOINLINE static void _wlmgrTxProcess(void);
MEOW_NOINLINE static void _wlmgrStop(void);
MEOW_NOINLINE MEOW_CODE32 static void _wlmgrPxiProcessCmd(PxiWlMgrCmd cmd, unsigned imm, const void* body, unsigned num_words);

MEOW_INLINE void _wlmgrSendEvent(WlMgrEvent evt, unsigned imm)
{
	pxiSend(PxiChannel_WlMgr, pxiWlMgrMakeEvent(evt, imm));
}

void _wlmgrSetState(WlMgrState state)
{
	if (state != s_wlmgrState.state) {
		s_wlmgrState.state = state;
		_wlmgrSendEvent(WlMgrEvent_NewState, state);
	}
}

static void _wlmgrCmdPxiHandler(void* user, u32 data)
{
	// Send data to mailbox
	mailboxTrySend(&s_wlmgrState.pxi_mbox, data);

	// Signal thread if needed
	Mailbox* mbox = (Mailbox*)user;
	if (mbox->pending_slots == 0) {
		mailboxTrySend(mbox, WLMGR_MAIL_PXI);
	}
}

static void _wlmgrTxPxiHandler(void* user, u32 data)
{
	// Calculate packet address
	NetBuf* buf = (NetBuf*)(MM_MAINRAM + (data << 5));

	// Append packet to tx queue
	buf->link.next = NULL;
	if (s_wlmgrState.tx_queue.next) {
		s_wlmgrState.tx_queue.prev->link.next = buf;
	} else {
		s_wlmgrState.tx_queue.next = buf;
	}
	s_wlmgrState.tx_queue.prev = buf;

	// Signal thread if needed
	Mailbox* mbox = (Mailbox*)user;
	if (mbox->pending_slots == 0) {
		mailboxTrySend(mbox, WLMGR_MAIL_PXI);
	}
}

static void _wlmgrPmEventHandler(void* user, PmEvent event)
{
	switch (event) {
		default: break;

		case PmEvent_OnSleep:
		case PmEvent_OnReset: {
			// Signal thread
			Mailbox* mbox = (Mailbox*)user;
			mailboxTrySend(mbox, WLMGR_MAIL_PM);

			while (s_wlmgrState.state != WlMgrState_Stopped) {
				threadSleep(1000);
			}
			break;
		}
	}
}

static void _wlmgrScanComplete(void* user, WlanBssDesc* bss_list, unsigned bss_count)
{
	u32 pkt = pxiWlMgrMakeEvent(WlMgrEvent_ScanComplete, bss_count);
	mailboxTrySend(&s_wlmgrState.mbox, WLMGR_MAIL_EVENT | (pkt << 2));
}

static void _wlmgrOnAssoc(void* user, bool success, unsigned reason)
{
	u32 pkt;
	if (success) {
		pkt = pxiWlMgrMakeEvent(WlMgrEvent_NewState, WlMgrState_Associated);
	} else {
		pkt = pxiWlMgrMakeEvent(WlMgrEvent_Disconnected, reason);
	}
	mailboxTrySend(&s_wlmgrState.mbox, WLMGR_MAIL_EVENT | (pkt << 2));
}

void _netbufRx(NetBuf* pPacket, int rssi)
{
	if (s_wlmgrState.state == WlMgrState_Associated) {
		uptr addr = (uptr)pPacket - MM_MAINRAM;
		pxiSend(PxiChannel_NetBuf, addr >> 5);
	} else {
		netbufFree(pPacket);
	}
}

static int _wlmgrThreadMain(void* arg)
{
	u32 mbox_slots[WLMGR_NUM_MAIL_SLOTS];
	u32 pxi_mbox_slots[PXI_WLMGR_NUM_CREDITS];
	mailboxPrepare(&s_wlmgrState.mbox, mbox_slots, WLMGR_NUM_MAIL_SLOTS);
	mailboxPrepare(&s_wlmgrState.pxi_mbox, pxi_mbox_slots, PXI_WLMGR_NUM_CREDITS);

	pxiSetHandler(PxiChannel_NetBuf, _wlmgrTxPxiHandler, &s_wlmgrState.mbox);
	pxiSetHandler(PxiChannel_WlMgr, _wlmgrCmdPxiHandler, &s_wlmgrState.mbox);

	PmEventCookie pm_cookie;
	pmAddEventHandler(&pm_cookie, _wlmgrPmEventHandler, &s_wlmgrState.mbox);

	for (;;) {
		u32 mail = mailboxRecv(&s_wlmgrState.mbox);
		unsigned mail_type = mail & 3;
		if (mail_type == WLMGR_MAIL_PM || mail_type == WLMGR_MAIL_EXIT) {
			// Wind down and deactivate wireless hardware
			_wlmgrStop();
		}
		if (mail_type == WLMGR_MAIL_EXIT) {
			break;
		}

		// Process wireless events
		if (mail_type == WLMGR_MAIL_EVENT && s_wlmgrState.state >= WlMgrState_Idle) {
			u32 evt_packet = mail >> 2;
			WlMgrEvent evt = pxiWlMgrEventGetType(evt_packet);

			if (evt == WlMgrEvent_NewState) {
				// Process simple state changes
				_wlmgrSetState((WlMgrState)pxiWlMgrEventGetImm(evt_packet));
			} else {
				// Relay other events
				pxiSend(PxiChannel_WlMgr, evt_packet);

				// Perform state change if needed
				switch (evt) {
					default: break;

					case WlMgrEvent_ScanComplete:
					case WlMgrEvent_Disconnected:
						_wlmgrSetState(WlMgrState_Idle);
						break;
				}
			}
		}

		// Process PXI commands
		_wlmgrPxiProcess(&s_wlmgrState.pxi_mbox);

		// Process outgoing packets
		_wlmgrTxProcess();
	}

	return 0;
}

void wlmgrStartServer(u8 thread_prio)
{
	threadPrepare(&s_wlmgrThread, _wlmgrThreadMain, NULL, &s_wlmgrThreadStack[sizeof(s_wlmgrThreadStack)], thread_prio);
	threadStart(&s_wlmgrThread);
}

void _wlmgrPxiProcessCmd(PxiWlMgrCmd cmd, unsigned imm, const void* body, unsigned num_words)
{
	bool rc = false;

	switch (cmd) {
		default: {
			dietPrint("[WLMGR] cmd %u\n", cmd);
			break;
		}

		case PxiWlMgrCmd_Start: {
			if (s_wlmgrState.state == WlMgrState_Stopped) {
				s_wlmgrState.using_twlwifi = systemIsTwlMode() && imm == WlMgrMode_Infrastructure;

				pmSetSleepAllowed(false);
				_wlmgrSetState(WlMgrState_Starting);
				if (s_wlmgrState.using_twlwifi) {
					rc = twlwifiInit();
				} else {
					// XX: mitsumi
				}

				if (rc) {
					_wlmgrSetState(WlMgrState_Idle);
				} else {
					_wlmgrSetState(WlMgrState_Stopped);
					pmSetSleepAllowed(true);
				}
			}
			break;
		}

		case PxiWlMgrCmd_Stop: {
			_wlmgrStop();
			rc = true;
			break;
		}

		case PxiWlMgrCmd_StartScan: {
			if (s_wlmgrState.state == WlMgrState_Idle) {
				uptr buf_addr = *(uptr*)body;
				WlanBssScanFilter filter = *(WlanBssScanFilter*)buf_addr;
				WlanBssDesc* out_table = (WlanBssDesc*)buf_addr;
				if (s_wlmgrState.using_twlwifi) {
					rc = twlwifiStartScan(out_table, &filter, _wlmgrScanComplete, NULL);
				} else {
					// XX: Mitsumi
				}
				if (rc) {
					_wlmgrSetState(WlMgrState_Scanning);
				}
			}
			break;
		}

		case PxiWlMgrCmd_Associate: {
			if (s_wlmgrState.state == WlMgrState_Idle) {
				PxiWlMgrArgAssociate* arg = (PxiWlMgrArgAssociate*)body;
				if (s_wlmgrState.using_twlwifi) {
					rc = twlwifiAssociate(arg->bss, arg->auth, _wlmgrOnAssoc, NULL);
				} else {
					// XX: Mitsumi
				}
				if (rc) {
					_wlmgrSetState(WlMgrState_Associating);
				}
			}
			break;
		}

		case PxiWlMgrCmd_Deassociate: {
			if (s_wlmgrState.state == WlMgrState_Associating || s_wlmgrState.state == WlMgrState_Associated) {
				if (s_wlmgrState.using_twlwifi) {
					rc = twlwifiDeassociate();
				} else {
					// XX: Mitsumi
				}
				if (rc) {
					_wlmgrSetState(WlMgrState_Deassociating);
				}
			}
			break;
		}
	}

	if (!rc) {
		_wlmgrSendEvent(WlMgrEvent_CmdFailed, cmd);
	}
}

void _wlmgrPxiProcess(Mailbox* mb)
{
	u32 cmdheader;
	while (mailboxTryRecv(mb, &cmdheader)) {
		unsigned num_words = cmdheader >> 26;
		if (num_words) {
			cmdheader &= (1U<<26)-1;
		}

		PxiWlMgrCmd cmd = pxiWlMgrCmdGetType(cmdheader);
		unsigned imm = pxiWlMgrCmdGetImm(cmdheader);

		if_likely (num_words == 0) {
			// Process command directly
			_wlmgrPxiProcessCmd(cmd, imm, NULL, 0);
		} else {
			// Receive message body
			u32 body[num_words];
			for (unsigned i = 0; i < num_words; i ++) {
				body[i] = mailboxRecv(mb);
			}

			// Process command
			_wlmgrPxiProcessCmd(cmd, imm, body, num_words);
		}
	}
}

void _wlmgrTxProcess(void)
{
	// Atomically borrow the packet list
	IrqState st = irqLock();
	NetBuf* pPacket = s_wlmgrState.tx_queue.next;
	s_wlmgrState.tx_queue.next = NULL;
	s_wlmgrState.tx_queue.prev = NULL;
	irqUnlock(st);

	NetBuf* pNext;
	for (; pPacket; pPacket = pNext) {
		pNext = pPacket->link.next;

		if (s_wlmgrState.state == WlMgrState_Associated) {
			if (s_wlmgrState.using_twlwifi) {
				twlwifiTx(pPacket);
			} else {
				// XX: Mitsumi
			}
		}

		netbufFree(pPacket);
	}
}

void _wlmgrStop(void)
{
	if (s_wlmgrState.state >= WlMgrState_Idle) {
		_wlmgrSetState(WlMgrState_Stopping);
		if (s_wlmgrState.using_twlwifi) {
			twlwifiExit();
		} else {
			// XX: mitsumi
		}
		_wlmgrSetState(WlMgrState_Stopped);
		pmSetSleepAllowed(true);
	}
}