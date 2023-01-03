#pragma once
#include "base.h"
#include "htc.h"

#define AR6K_WMI_PROTOCOL_VER 2

typedef enum Ar6kWmiCmdId {
	Ar6kWmiCmdId_StartScan                = 0x0007,
	Ar6kWmiCmdId_SetScanParams            = 0x0008,
	Ar6kWmiCmdId_SetBssFilter             = 0x0009,
	Ar6kWmiCmdId_SetProbedSsid            = 0x000a,
	Ar6kWmiCmdId_GetChannelList           = 0x000e,
	Ar6kWmiCmdId_SetChannelParams         = 0x0011,
	Ar6kWmiCmdId_TargetErrorReportBitmask = 0x0022,
} Ar6kWmiCmdId;

typedef enum Ar6kWmiEventId {
	Ar6kWmiEventId_GetChannelListReply    = 0x000e,
	Ar6kWmiEventId_Ready                  = 0x1001,
	Ar6kWmiEventId_BssInfo                = 0x1004,
	Ar6kWmiEventId_ScanComplete           = 0x100a,
	Ar6kWmiEventId_Extension              = 0x1010,
} Ar6kWmiEventId;

typedef enum Ar6kWmixEventId {
	Ar6kWmixEventId_DbgLog                = 0x3008,
} Ar6kWmixEventId;

typedef struct Ar6kWmiCtrlHdr {
	u16 id; // Ar6kWmiCmdId or Ar6kWmiEventId
} Ar6kWmiCtrlHdr;

typedef struct Ar6kWmiDataHdr {
	s8 rssi;
	u8 msg_type  : 2;
	u8 user_prio : 3;
	u8 _pad      : 3;
} Ar6kWmiDataHdr;

typedef struct Ar6kWmiGeneric8 {
	u8 value;
} Ar6kWmiGeneric8;

typedef struct Ar6kWmiGeneric16 {
	u16 value;
} Ar6kWmiGeneric16;

typedef struct Ar6kWmiGeneric32 {
	u32 value;
} Ar6kWmiGeneric32;

// Error report bits
#define AR6K_WMI_ERPT_TARGET_PM_ERR_FAIL    (1U<<0)
#define AR6K_WMI_ERPT_TARGET_KEY_NOT_FOUND  (1U<<1)
#define AR6K_WMI_ERPT_TARGET_DECRYPTION_ERR (1U<<2)
#define AR6K_WMI_ERPT_TARGET_BMISS          (1U<<3)
#define AR6K_WMI_ERPT_PSDISABLE_NODE_JOIN   (1U<<4)
#define AR6K_WMI_ERPT_TARGET_COM_ERR        (1U<<5)
#define AR6K_WMI_ERPT_TARGET_FATAL_ERR      (1U<<6)
#define AR6K_WMI_ERPT_ALL                   0x7f

typedef enum Ar6kWmiPhyMode {
	Ar6kWmiPhyMode_11A     = 1,
	Ar6kWmiPhyMode_11G     = 2,
	Ar6kWmiPhyMode_11AG    = 3,
	Ar6kWmiPhyMode_11B     = 4,
	Ar6kWmiPhyMode_11Gonly = 5,
} Ar6kWmiPhyMode;

//-----------------------------------------------------------------------------
// WMI commands
//-----------------------------------------------------------------------------

typedef enum Ar6kWmiScanType {
	Ar6kWmiScanType_Long  = 0,
	Ar6kWmiScanType_Short = 1,
} Ar6kWmiScanType;

typedef struct Ar6kWmiCmdStartScan {
	u8 force_bg_scan;           // bool
	u8 is_legacy;               // bool - "for legacy Cisco AP compatibility"
	u32 home_dwell_time_ms;     // "Maximum duration in the home channel"
	u32 force_scan_interval_ms; // "Time interval between scans"
	u8 scan_type;               // Ar6kWmiScanType
	u8 num_channels;
	u16 channel_mhz[];
} Ar6kWmiCmdStartScan;

#define AR6K_WMI_SCAN_CONNECT        (1U<<0) // "set if can scan in the Connect cmd" (sic)
#define AR6K_WMI_SCAN_CONNECTED      (1U<<1) // "set if scan for the SSID it is already connected to"
#define AR6K_WMI_SCAN_ACTIVE         (1U<<2) // active scan (as opposed to passive scan)
#define AR6K_WMI_SCAN_ROAM           (1U<<3) // "enable roam scan when bmiss and lowrssi"
#define AR6K_WMI_SCAN_REPORT_BSSINFO (1U<<4) // presumably enables bssinfo events
#define AR6K_WMI_SCAN_ENABLE_AUTO    (1U<<5) // "if disabled, target doesn't scan after a disconnect event"
#define AR6K_WMI_SCAN_ENABLE_ABORT   (1U<<6) // "Scan complete event with canceled status will be generated when a scan is prempted before it gets completed"

typedef struct Ar6kWmiScanParams {
	u16 fg_start_period_secs;
	u16 fg_end_period_secs;
	u16 bg_period_secs;
	u16 maxact_chdwell_time_ms;
	u16 pas_chdwell_time_ms;
	u8  short_scan_ratio;
	u8  scan_ctrl_flags;
	u16 minact_chdwell_time_ms;
	u16 _pad;
	u32 max_dfsch_act_time_ms;
} Ar6kWmiScanParams;

typedef enum Ar6kWmiBssFilter {
	Ar6kWmiBssFilter_None          = 0, // "no beacons forwarded"
	Ar6kWmiBssFilter_All           = 1, // "all beacons forwarded"
	Ar6kWmiBssFilter_Profile       = 2, // "only beacons matching profile"
	Ar6kWmiBssFilter_AllButProfile = 3, // "all but beacons matching profile"
	Ar6kWmiBssFilter_CurrentBss    = 4, // "only beacons matching current BSS"
	Ar6kWmiBssFilter_AllButBss     = 5, // "all but beacons matching BSS"
	Ar6kWmiBssFilter_ProbedSsid    = 6, // "beacons matching probed ssid"
} Ar6kWmiBssFilter;

typedef struct Ar6kWmiCmdBssFilter {
	u8 bss_filter; // Ar6kWmiBssFilter
	u8 _pad[3];
	u32 ie_mask;
} Ar6kWmiCmdBssFilter;

typedef enum Ar6kWmiSsidProbeMode {
	Ar6kWmiSsidProbeMode_Disable  = 0,
	Ar6kWmiSsidProbeMode_Specific = 1,
	Ar6kWmiSsidProbeMode_Any      = 2,
} Ar6kWmiSsidProbeMode;

typedef struct Ar6kWmiProbedSsid {
	u8 entry_idx;
	u8 mode; // Ar6kWmiSsidProbeMode
	u8 ssid_len;
	char ssid[32];
} Ar6kWmiProbedSsid;

typedef struct Ar6kWmiChannelParams {
	u8  reserved;
	u8  scan_param;   // ???
	u8  phy_mode;     // Ar6kWmiPhyMode
	u8  num_channels;
	u16 channel_mhz[];
} Ar6kWmiChannelParams;

//-----------------------------------------------------------------------------
// WMI events
//-----------------------------------------------------------------------------

typedef struct Ar6kWmiEvtReady {
	u8 macaddr[6];
	u8 phy_capability;
} Ar6kWmiEvtReady;

typedef struct Ar6kWmiChannelList {
	u8  reserved;
	u8  num_channels;
	u16 channel_mhz[];
} Ar6kWmiChannelList;

typedef enum Ar6kWmiBIFrameType {
	Ar6kWmiBIFrameType_Beacon     = 1,
	Ar6kWmiBIFrameType_ProbeResp  = 2,
	Ar6kWmiBIFrameType_ActionMgmt = 3,
	Ar6kWmiBIFrameType_ProbeReq   = 4,
	Ar6kWmiBIFrameType_AssocReq   = 5,
	Ar6kWmiBIFrameType_AssocResp  = 6,
} Ar6kWmiBIFrameType;

typedef struct Ar6kWmiBssInfoHdr {
	u16 channel_mhz;
	u8  frame_type; // Ar6kWmiBIFrameType
	s8  snr;
	s16 rssi;
	u8  bssid[6];
	u32 ie_mask;
	// beacon or probe-response frame body follows (sans 802.11 frame header)
} Ar6kWmiBssInfoHdr;

//-----------------------------------------------------------------------------
// WMI APIs
//-----------------------------------------------------------------------------

bool ar6kWmiStartup(Ar6kDev* dev);

bool ar6kWmiSimpleCmd(Ar6kDev* dev, Ar6kWmiCmdId cmdid);
bool ar6kWmiSimpleCmdWithParam32(Ar6kDev* dev, Ar6kWmiCmdId cmdid, u32 param);

bool ar6kWmiStartScan(Ar6kDev* dev, Ar6kWmiScanType type, u32 home_dwell_time_ms);
bool ar6kWmiSetScanParams(Ar6kDev* dev, Ar6kWmiScanParams const* params);
bool ar6kWmiSetBssFilter(Ar6kDev* dev, Ar6kWmiBssFilter filter, u32 ie_mask);
bool ar6kWmiSetProbedSsid(Ar6kDev* dev, Ar6kWmiProbedSsid const* probed_ssid);
bool ar6kWmiSetChannelParams(Ar6kDev* dev, u8 scan_param, u32 chan_mask);

MEOW_INLINE bool ar6kWmiGetChannelList(Ar6kDev* dev)
{
	return ar6kWmiSimpleCmd(dev, Ar6kWmiCmdId_GetChannelList);
}

MEOW_INLINE bool ar6kWmiSetErrorReportBitmask(Ar6kDev* dev, u32 bitmask)
{
	return ar6kWmiSimpleCmdWithParam32(dev, Ar6kWmiCmdId_TargetErrorReportBitmask, bitmask);
}
