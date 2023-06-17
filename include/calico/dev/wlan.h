#pragma once
#include "netbuf.h"

#define WLAN_MAX_BSS_ENTRIES 32
#define WLAN_MAX_SSID_LEN    32
#define WLAN_WEP_40_LEN      5
#define WLAN_WEP_104_LEN     13
#define WLAN_WEP_128_LEN     16
#define WLAN_WPA_PSK_LEN     32

MEOW_EXTERN_C_START

typedef enum WlanFrameType {
	WlanFrameType_Management = 0,
	WlanFrameType_Control    = 1,
	WlanFrameType_Data       = 2,
} WlanFrameType;

typedef enum WlanMgmtType {
	WlanMgmtType_AssocReq    = 0,
	WlanMgmtType_AssocResp   = 1,
	WlanMgmtType_ReassocReq  = 2,
	WlanMgmtType_ReassocResp = 3,
	WlanMgmtType_ProbeReq    = 4,
	WlanMgmtType_ProbeResp   = 5,
	WlanMgmtType_Beacon      = 8,
	WlanMgmtType_ATIM        = 9,
	WlanMgmtType_Disassoc    = 10,
	WlanMgmtType_Auth        = 11,
	WlanMgmtType_Deauth      = 12,
	WlanMgmtType_Action      = 13,
} WlanMgmtType;

typedef enum WlanCtrlType {
	WlanCtrlType_BlockAckReq = 8,
	WlanCtrlType_BlockAck    = 9,
	WlanCtrlType_PSPoll      = 10,
	WlanCtrlType_RTS         = 11,
	WlanCtrlType_CTS         = 12,
	WlanCtrlType_ACK         = 13,
	WlanCtrlType_CFEnd       = 14,
	WlanCtrlType_CFEnd_CFAck = 15,
} WlanCtrlType;

// "WlanDataType" is just a collection of flags
#define WLAN_DATA_CF_ACK  (1U<<0)
#define WLAN_DATA_CF_POLL (1U<<1)
#define WLAN_DATA_IS_NULL (1U<<2)
#define WLAN_DATA_IS_QOS  (1U<<3)

typedef enum WlanEid {
	WlanEid_SSID             = 0,
	WlanEid_SupportedRates   = 1,
	WlanEid_RSN              = 48,
	WlanEid_SupportedRatesEx = 50,
	WlanEid_Vendor           = 221, // also used for NN-specific data
} WlanEid;

typedef union WlanFrameCtrl {
	u16 value;
	struct {
		u16 version    : 2;
		u16 type       : 2; // WlanFrameType
		u16 subtype    : 4; // WlanMgmtType, WlanCtrlType, WLAN_DATA_* flags
		u16 to_ds      : 1;
		u16 from_ds    : 1;
		u16 more_frag  : 1;
		u16 retry      : 1;
		u16 power_mgmt : 1;
		u16 more_data  : 1;
		u16 wep        : 1;
		u16 order      : 1;
	};
} WlanFrameCtrl;

typedef union WlanSeqCtrl {
	u16 value;
	struct {
		u16 frag_num : 4;
		u16 seq_num  : 12;
	};
} WlanSeqCtrl;

typedef struct WlanMacHdr {
	WlanFrameCtrl fc;
	u16 duration;
	u8  rx_addr[6];
	u8  tx_addr[6];
	u8  xtra_addr[6];
	WlanSeqCtrl sc;
	// WDS/Mesh routing has a 4th addr here (Data frame with ToDS=1 FromDS=1)
} WlanMacHdr;

typedef struct WlanBeaconHdr {
	u32 timestamp[2];
	u16 interval;
	u16 capabilities;
	// Information elements follow: WlanIeHdr[]
} WlanBeaconHdr;

typedef struct WlanAssocReqHdr {
	u16 capabilities;
	u16 interval;
	// Information elements follow: WlanIeHdr[]
} WlanAssocReqHdr;

typedef struct WlanAssocRespHdr {
	u16 capabilities;
	u16 interval;
	u16 aid;
	// Information elements follow: WlanIeHdr[]
} WlanAssocRespHdr;

typedef struct WlanIeHdr {
	u8 id;
	u8 len;
	u8 data[];
} WlanIeHdr;

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
WlanIeHdr* wlanFindRsnOrWpaIe(void* rawdata, unsigned rawdata_len);
void wlanParseBeacon(WlanBssDesc* desc, NetBuf* pPacket);

MEOW_EXTERN_C_END
