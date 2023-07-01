#pragma once
#include <calico/types.h>
#include <calico/system/mutex.h>
#include <calico/system/tick.h>
#include <calico/dev/mwl.h>

#define MWL_DEBUG

#ifdef MWL_DEBUG
#include <calico/system/dietprint.h>
#else
#define dietPrint(...) ((void)0)
#endif

typedef enum MwlTask {
	MwlTask_ExitThread,

	// High priority tasks
	MwlTask_TxEnd,
	MwlTask_RxEnd,

	// Medium priority tasks
	MwlTask_RxMgmtCtrlFrame,

	// Other tasks
	MwlTask_MlmeProcess,
	MwlTask_RxDataFrame,

	MwlTask__Count,
} MwlTask;

typedef enum MwlMlmeState {
	MwlMlmeState_Idle,
	MwlMlmeState_Preparing,

	MwlMlmeState_ScanSetup,
	MwlMlmeState_ScanBusy,

	MwlMlmeState_JoinBusy,
	MwlMlmeState_JoinDone,
} MwlMlmeState;

typedef struct MwlTxQueue {
	NetBufListNode list;
	MwlTxCallback cb;
	void* arg;
} MwlTxQueue;

typedef struct MwlState {
	u32 task_mask;

	u16 mode            : 2;
	u16 status          : 2;
	u16 has_beacon_sync : 1;
	u16 is_power_save   : 1;
	u16 tx_busy         : 3;
	u16 wep_enabled     : 1;
	u16 _pad            : 6;

	u16 bssid[3];

	u16 rx_pos;
	u16 tx_pos[3];
	union {
		u16 tx_reply_pos[2];
		struct {
			u16 tx_beacon_pos;
			u16 tx_cmd_pos;
		};
	};

	u16 rx_wrcsr;

	NetBufListNode rx_mgmt;

	u16 tx_size[3];
	Mutex tx_mutex;
	MwlTxQueue tx_queues[3];

	TickTask timeout_task;
	TickTask periodic_task;

	u8 mlme_state;
	MwlMlmeCallbacks mlme_cb;
	union {
		struct {
			WlanBssScanFilter filter;
			u16 cur_ch;
			u16 ch_dwell_ticks;
			u16 update_period;
			u16 dwell_elapsed;
		} scan;
	} mlme;
} MwlState;

extern MwlState s_mwlState;

unsigned _mwlBbpRead(unsigned reg);
void _mwlBbpWrite(unsigned reg, unsigned value);
void _mwlRfCmd(u32 cmd);

#define _mwlPushTask(...) ({ \
	const MwlTask _task_ids[] = { __VA_ARGS__ }; \
	u32 _task_mask = 0; \
	for (unsigned _task_i = 0; _task_i < sizeof(_task_ids)/sizeof(_task_ids[0]); _task_i ++) { \
		_task_mask |= 1U << _task_ids[_task_i]; \
	} \
	_mwlPushTaskImpl(_task_mask); \
})

void _mwlIrqHandler(void);
MEOW_EXTERN32 void _mwlPushTaskImpl(u32 mask) __asm__("_mwlPushTask");
MEOW_EXTERN32 MwlTask _mwlPopTask(void);
void _mwlRxQueueClear(void);
void _mwlTxQueueClear(unsigned qid);
bool _mwlSetMlmeState(MwlMlmeState state);
void _mwlMlmeOnBssInfo(WlanBssDesc* bssInfo, WlanBssExtra* bssExtra, unsigned rssi);
void _mwlMlmeHandleJoin(WlanBeaconHdr* beaconHdr, WlanBssDesc* bssInfo, WlanBssExtra* bssExtra);

// Utilities for crafting management frames
NetBuf* _mwlMgmtMakeProbeReq(const void* bssid, const char* ssid, unsigned ssid_len);
