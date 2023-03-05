#pragma once
#if !defined(__NDS__)
#error "This header file is only for NDS"
#endif

#include "../types.h"
#include "../dev/netbuf.h"
#include "../dev/wlan.h"

typedef enum WlMgrMode {
	WlMgrMode_Infrastructure = 0,
	WlMgrMode_LocalComms     = 1,
} WlMgrMode;

typedef enum WlMgrState {
	WlMgrState_Stopped       = 0,
	WlMgrState_Starting      = 1,
	WlMgrState_Idle          = 2,
	WlMgrState_Scanning      = 3,
	WlMgrState_Associating   = 4,
	WlMgrState_Associated    = 5,
	WlMgrState_Deassociating = 6,
} WlMgrState;

typedef enum WlMgrEvent {
	WlMgrEvent_NewState     = 0,
	WlMgrEvent_CmdFailed    = 1,
	WlMgrEvent_ScanComplete = 2,
	WlMgrEvent_Disconnected = 3,
} WlMgrEvent;

#if defined(ARM9)

typedef void (*WlMgrEventFn)(void* user, WlMgrEvent event, uptr arg0, uptr arg1);
typedef void (*WlMgrRawRxFn)(void* user, NetBuf* pPacket);

void wlmgrInit(void* work_mem, size_t work_mem_sz, u8 thread_prio);
void wlmgrSetEventHandler(WlMgrEventFn cb, void* user);
WlMgrState wlmgrGetState(void);

void wlmgrStart(WlMgrMode mode);
void wlmgrStop(void);
void wlmgrStartScan(WlanBssDesc* out_table, WlanBssScanFilter const* filter);
void wlmgrAssociate(WlanBssDesc const* bss, WlanAuthData const* auth);
void wlmgrDeassociate(void);

void wlmgrSetRawRxHandler(WlMgrRawRxFn cb, void* user);
void wlmgrRawTx(NetBuf* pPacket);

#elif defined(ARM7)

void wlmgrStartServer(u8 thread_prio);

#endif
