#include <string.h>
#include <calico/types.h>
#include <calico/arm/cache.h>
#include <calico/system/thread.h>
#include <calico/system/mailbox.h>
#include <calico/nds/pm.h>
#include <calico/nds/pxi.h>
#include <calico/nds/wlmgr.h>
#include "../transfer.h"
#include "../pxi/wlmgr.h"

#define WLMGR_NUM_MAIL_SLOTS 4

static Thread s_wlmgrThread;
alignas(8) static u8 s_wlmgrThreadStack[2048];

static struct {
	WlMgrState state;
	NetBufListNode rx_queue;

	WlMgrEventFn event_cb;
	void* event_user;

	WlMgrRawRxFn rx_cb;
	void* rx_user;

	WlanBssDesc* scan_buf;
} s_wlmgrState;

static void _wlmgrRxPxiHandler(void* user, u32 data)
{
	// Calculate packet address
	NetBuf* buf = (NetBuf*)(MM_MAINRAM + (data << 5));

	// Append packet to rx queue
	buf->link.next = NULL;
	if (s_wlmgrState.rx_queue.next) {
		s_wlmgrState.rx_queue.prev->link.next = buf;
	} else {
		s_wlmgrState.rx_queue.next = buf;
	}
	s_wlmgrState.rx_queue.prev = buf;

	// Signal thread if needed
	Mailbox* mbox = (Mailbox*)user;
	if (mbox->pending_slots == 0) {
		mailboxTrySend(mbox, UINT32_MAX);
	}
}

static int _wlmgrThreadMain(void* arg)
{
	// Set up main mailbox
	Mailbox mbox;
	u32 mbox_slots[WLMGR_NUM_MAIL_SLOTS];
	mailboxPrepare(&mbox, mbox_slots, WLMGR_NUM_MAIL_SLOTS);

	// Set up PXI channels
	pxiSetHandler(PxiChannel_NetBuf, _wlmgrRxPxiHandler, &mbox);
	pxiSetMailbox(PxiChannel_WlMgr, &mbox);

	for (;;) {
		u32 msg = mailboxRecv(&mbox);
		if (msg != UINT32_MAX) {
			// Parse message
			WlMgrEvent evt = pxiWlMgrEventGetType(msg);
			unsigned imm = pxiWlMgrEventGetImm(msg);

			// Process event
			uptr arg0 = 0, arg1 = 0;
			switch (evt) {
				default: break;
				case WlMgrEvent_NewState: {
					arg0 = imm;
					arg1 = s_wlmgrState.state;
					s_wlmgrState.state = (WlMgrState)imm;
					break;
				}

				case WlMgrEvent_ScanComplete: {
					arg0 = (uptr)s_wlmgrState.scan_buf;
					arg1 = imm;
					s_wlmgrState.scan_buf = NULL;
					break;
				}

				case WlMgrEvent_Disconnected: {
					arg0 = imm;
					break;
				}
			}

			// Call user event callback
			if (s_wlmgrState.event_cb) {
				s_wlmgrState.event_cb(s_wlmgrState.event_user, evt, arg0, arg1);
			}
		}

		// Atomically borrow the packet list
		IrqState st = irqLock();
		NetBuf* pPacket = s_wlmgrState.rx_queue.next;
		s_wlmgrState.rx_queue.next = NULL;
		s_wlmgrState.rx_queue.prev = NULL;
		irqUnlock(st);

		// Process incoming packets
		NetBuf* pNext;
		for (; pPacket; pPacket = pNext) {
			pNext = pPacket->link.next;
			if (s_wlmgrState.rx_cb) {
				s_wlmgrState.rx_cb(s_wlmgrState.rx_user, pPacket);
			} else {
				netbufFree(pPacket);
			}
		}
	}

	return 0;
}

void wlmgrInit(void* work_mem, size_t work_mem_sz, u8 thread_prio)
{
	// TODO: do something with the work mem (_netbufPrvInitPools)

	// Bring up event/rx thread
	threadPrepare(&s_wlmgrThread, _wlmgrThreadMain, NULL, &s_wlmgrThreadStack[sizeof(s_wlmgrThreadStack)], thread_prio);
	threadStart(&s_wlmgrThread);

	// Wait for ARM7 to be available
	pxiWaitRemote(PxiChannel_WlMgr);
}

void wlmgrSetEventHandler(WlMgrEventFn cb, void* user)
{
	IrqState st = irqLock();
	s_wlmgrState.event_cb = cb;
	s_wlmgrState.event_user = user;
	irqUnlock(st);
}

WlMgrState wlmgrGetState(void)
{
	return s_wlmgrState.state;
}

void wlmgrStart(WlMgrMode mode)
{
	pxiSend(PxiChannel_WlMgr, pxiWlMgrMakeCmd(PxiWlMgrCmd_Start, mode));
}

void wlmgrStop(void)
{
	pxiSend(PxiChannel_WlMgr, pxiWlMgrMakeCmd(PxiWlMgrCmd_Stop, 0));
}

void wlmgrStartScan(WlanBssDesc* out_table, WlanBssScanFilter const* filter)
{
	// Check output buffer alignment
	u32 buf_addr = (u32)out_table;
	if (buf_addr & (ARM_CACHE_LINE_SZ-1)) {
		return;
	}

	// Copy filter to output buffer
	memcpy((void*)buf_addr, filter, sizeof(*filter));
	armDCacheFlush((void*)buf_addr, WLAN_MAX_BSS_ENTRIES*sizeof(WlanBssDesc));

	// Send command
	s_wlmgrState.scan_buf = out_table;
	pxiSendWithData(PxiChannel_WlMgr, pxiWlMgrMakeCmd(PxiWlMgrCmd_StartScan, 0), &buf_addr, 1);
}

void wlmgrAssociate(WlanBssDesc const* bss, WlanAuthData const* auth)
{
	armDCacheFlush((void*)bss, sizeof(*bss));
	armDCacheFlush((void*)auth, sizeof(*auth));

	PxiWlMgrArgAssociate arg = {
		.bss  = bss,
		.auth = auth,
	};

	pxiSendWithData(PxiChannel_WlMgr, pxiWlMgrMakeCmd(PxiWlMgrCmd_Associate, 0), (const u32*)&arg, sizeof(arg)/sizeof(u32));
}

void wlmgrDeassociate(void)
{
	pxiSend(PxiChannel_WlMgr, pxiWlMgrMakeCmd(PxiWlMgrCmd_Deassociate, 0));
}

void wlmgrSetRawRxHandler(WlMgrRawRxFn cb, void* user)
{
	IrqState st = irqLock();
	s_wlmgrState.rx_cb = cb;
	s_wlmgrState.rx_user = user;
	irqUnlock(st);
}

void wlmgrRawTx(NetBuf* pPacket)
{
	uptr addr = (uptr)pPacket - MM_MAINRAM;
	netbufFlush(pPacket);
	pxiSend(PxiChannel_NetBuf, addr >> 5);
}
