#pragma once
#include "netbuf.h"

/*! @addtogroup wlan
	@{
*/

#define WLAN_OUI_NINTENDO  0x0009bf
#define WLAN_OUI_IEEE      0x000fac
#define WLAN_OUI_MICROSOFT 0x0050f2

#define WLAN_MAX_BSS_ENTRIES 32
#define WLAN_MAX_SSID_LEN    32
#define WLAN_WEP_40_LEN      5
#define WLAN_WEP_104_LEN     13
#define WLAN_WEP_128_LEN     16
#define WLAN_WPA_PSK_LEN     32

#define WLAN_RSSI_STRENGTH_1 10
#define WLAN_RSSI_STRENGTH_2 16
#define WLAN_RSSI_STRENGTH_3 21

MK_EXTERN_C_START

// Potentially misaligned integer datatypes
typedef u8 Wlan16[2];
typedef u8 Wlan24[3];
typedef u8 Wlan32[4];

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
	WlanEid_DSParamSet       = 3,
	WlanEid_CFParamSet       = 4,
	WlanEid_TIM              = 5,
	WlanEid_ChallengeText    = 16,
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

typedef struct WlanAuthHdr {
	u16 algorithm_id;
	u16 sequence_num;
	u16 status;
	// Information elements follow: WlanIeHdr[]
} WlanAuthHdr;

typedef struct WlanAssocReqHdr {
	u16 capabilities;
	u16 interval;
	// Information elements follow: WlanIeHdr[]
} WlanAssocReqHdr;

typedef struct WlanAssocRespHdr {
	u16 capabilities;
	u16 status;
	u16 aid;
	// Information elements follow: WlanIeHdr[]
} WlanAssocRespHdr;

typedef struct WlanIeHdr {
	u8 id;
	u8 len;
	u8 data[];
} WlanIeHdr;

typedef struct WlanIeTim {
	u8 dtim_count;
	u8 dtim_period;
	u8 bitmap_ctrl;
	u8 bitmap[];
} WlanIeTim;

typedef struct WlanIeCfp {
	u8 count;
	u8 period;
	Wlan16 max_duration;
	Wlan16 dur_remaining;
} WlanIeCfp;

typedef struct WlanIeRsn {
	Wlan16 version;
	u8 group_cipher[4];
	Wlan16 num_pairwise_ciphers;
	u8 pairwise_ciphers[];
} WlanIeRsn;

typedef struct WlanIeVendor {
	Wlan24 oui;
	u8 type;
	u8 data[];
} WlanIeVendor;

typedef struct WlanIeNin {
	Wlan16 active_zone;
	Wlan16 vtsf;

	Wlan16 magic;
	u8 version;
	u8 platform;
	Wlan32 game_id;
	Wlan16 stream_code;
	u8 data_sz;
	u8 attrib;
	Wlan16 cmd_sz;
	Wlan16 reply_sz;

	u8 data[];
} WlanIeNin;

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
	union {
		u8 auth_mask;
		WlanBssAuthType auth_type;
	};
	u8 rssi;
	u8 channel;
} WlanBssDesc;

typedef struct WlanBssExtra {
	WlanIeTim* tim;
	WlanIeCfp* cfp;
	WlanIeNin* nin;
	u8 tim_bitmap_sz;
} WlanBssExtra;

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

MK_CONSTEXPR unsigned wlanCalcSignalStrength(unsigned rssi)
{
	if (rssi >= WLAN_RSSI_STRENGTH_3) return 3;
	if (rssi >= WLAN_RSSI_STRENGTH_2) return 2;
	if (rssi >= WLAN_RSSI_STRENGTH_1) return 1;
	return 0;
}

MK_CONSTEXPR unsigned wlanFreqToChannel(unsigned freq_mhz)
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

MK_CONSTEXPR unsigned wlanChannelToFreq(unsigned ch)
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

MK_INLINE unsigned wlanDecode16(const u8* data)
{
	return data[0] | (data[1]<<8);
}

MK_INLINE unsigned wlanDecode24(const u8* data)
{
	return data[0] | (data[1]<<8) | (data[2]<<16);
}

MK_INLINE unsigned wlanDecodeOui(const u8* data)
{
	return data[2] | (data[1]<<8) | (data[0]<<16);
}

MK_INLINE unsigned wlanDecode32(const u8* data)
{
	return data[0] | (data[1]<<8) | (data[2]<<16) | (data[3]<<24);
}

unsigned wlanGetRateBit(unsigned rate);
WlanBssDesc* wlanFindOrAddBss(WlanBssDesc* desc_table, unsigned* num_entries, void* bssid, unsigned rssi);
WlanIeHdr* wlanFindRsnOrWpaIe(void* rawdata, unsigned rawdata_len);
void wlanParseBeacon(WlanBssDesc* desc, WlanBssExtra* extra, NetBuf* pPacket);

MK_EXTERN_C_END

//! @}
