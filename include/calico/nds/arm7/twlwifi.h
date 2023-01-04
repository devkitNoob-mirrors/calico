#pragma once
#if !defined(__NDS__) || !defined(ARM7)
#error "This header file is only for NDS ARM7"
#endif

#include "../../types.h"
#include "../../dev/wlan.h"

typedef void (*TwlWifiScanCompleteFn)(void* user, WlanBssDesc* bss_list, unsigned bss_count);

bool twlwifiInit(void);
bool twlwifiStartScan(WlanBssDesc* out_table, WlanBssScanFilter const* filter, TwlWifiScanCompleteFn cb, void* user);
bool twlwifiIsScanning(void);
