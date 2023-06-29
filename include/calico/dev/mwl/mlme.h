#pragma once
#include "types.h"

MEOW_EXTERN_C_START

typedef struct MwlMlmeCallbacks {
	void (* onBssInfo)(WlanBssDesc* bssInfo, WlanBssExtra* bssExtra, unsigned rssi);
	u32 (* onScanEnd)(void);
	void (* onJoinEnd)(bool ok);
} MwlMlmeCallbacks;

MwlMlmeCallbacks* mwlMlmeGetCallbacks(void);
bool mwlMlmeScan(WlanBssScanFilter const* filter, unsigned ch_dwell_time);
bool mwlMlmeJoin(WlanBssDesc const* bssInfo, unsigned timeout);

MEOW_EXTERN_C_END

