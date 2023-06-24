#pragma once
#include "types.h"

MEOW_EXTERN_C_START

typedef struct MwlMlmeCallbacks {
	void (* onBssInfo)(WlanBssDesc* bssInfo, WlanBssExtra* bssExtra, unsigned rssi);
	u32 (* onScanEnd)(void);
} MwlMlmeCallbacks;

MwlMlmeCallbacks* mwlMlmeGetCallbacks(void);
bool mwlMlmeScan(WlanBssScanFilter const* filter, unsigned ch_dwell_time);

MEOW_EXTERN_C_END

