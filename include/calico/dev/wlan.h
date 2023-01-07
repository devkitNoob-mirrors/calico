#pragma once
#include "netbuf.h"

#define WLAN_MAX_BSS_ENTRIES 32
#define WLAN_MAX_SSID_LEN    32
#define WLAN_WEP_40_LEN      5
#define WLAN_WEP_104_LEN     13
#define WLAN_WEP_128_LEN     16
#define WLAN_WPA_PSK_LEN     32

typedef enum WlanBeaconEid {
	WlanBeaconEid_SSID             = 0,
	WlanBeaconEid_SupportedRates   = 1,
	WlanBeaconEid_RSN              = 48,
	WlanBeaconEid_SupportedRatesEx = 50,
	WlanBeaconEid_Vendor           = 221, // also used for NN-specific data
} WlanBeaconEid;

typedef struct WlanBeaconHdr {
	u32 timestamp[2];
	u16 interval;
	u16 capabilities;
	// Information elements follow: WlanBeaconIeHdr[]
} WlanBeaconHdr;

typedef struct WlanBeaconIeHdr {
	u8 id;
	u8 len;
	// Data follows
} WlanBeaconIeHdr;

typedef enum WlanBssAuthType {
	WlanBssAuthType_Open          = 0,
	WlanBssAuthType_WEP_40        = 1,
	WlanBssAuthType_WEP_104       = 2,
	WlanBssAuthType_WEP_128       = 3,
	WlanBssAuthType_WPA_PSK_TKIP  = 4,
	WlanBssAuthType_WPA2_PSK_TKIP = 5,
	WlanBssAuthType_WPA_PSK_AES   = 6,
	WlanBssAuthType_WPA2_PSK_AES  = 7,
} WlanBssAuthType;

typedef struct WlanBssDesc {
	u8 bssid[6];
	u16 ssid_len;
	char ssid[WLAN_MAX_SSID_LEN];
	u16 ieee_caps;
	u16 ieee_basic_rates;
	u16 ieee_all_rates;
	WlanBssAuthType auth_type;
	s8 rssi;
	u8 channel;
} WlanBssDesc;

typedef struct WlanBssScanFilter {
	u32 channel_mask;
	u8 target_bssid[6];
	u16 target_ssid_len;
	char target_ssid[WLAN_MAX_SSID_LEN];
} WlanBssScanFilter;

typedef union WlanAuthData {
	u8 wpa_psk[WLAN_WPA_PSK_LEN];
	u8 wep_key[WLAN_WEP_128_LEN];
} WlanAuthData;

MEOW_CONSTEXPR unsigned wlanFreqToChannel(unsigned freq_mhz)
{
	if (freq_mhz == 2484) {
		return 14;
	} else if (freq_mhz < 2484) {
		return (freq_mhz - 2407) / 5;
	} else if (freq_mhz < 5000) {
		return 15 + ((freq_mhz - 2512) / 20);
	} else {
		return (freq_mhz - 5000) / 5;
	}
}

MEOW_CONSTEXPR unsigned wlanChannelToFreq(unsigned ch)
{
	if (ch == 14) {
		return 2484;
	} else if (ch < 14) {
		return 2407 + 5*ch;
	} else if (ch < 27) {
		return 2512 + 20*(ch-15);
	} else {
		return 5000 + 5*ch;
	}
}

WlanBssDesc* wlanFindOrAddBss(WlanBssDesc* desc_table, unsigned* num_entries, void* bssid, int rssi);
void wlanParseBeacon(WlanBssDesc* desc, NetBuf* pPacket);
