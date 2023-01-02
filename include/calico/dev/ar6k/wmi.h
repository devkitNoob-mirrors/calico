#pragma once
#include "base.h"
#include "htc.h"

#define AR6K_WMI_PROTOCOL_VER 2

typedef enum Ar6kWmiCmdId {
	Ar6kWmiCmdId_GetChannelList           = 0x000e,
	Ar6kWmiCmdId_TargetErrorReportBitmask = 0x0022,
} Ar6kWmiCmdId;

typedef enum Ar6kWmiEventId {
	Ar6kWmiEventId_GetChannelListReply    = 0x000e,
	Ar6kWmiEventId_Ready                  = 0x1001,
} Ar6kWmiEventId;

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

//-----------------------------------------------------------------------------
// WMI commands
//-----------------------------------------------------------------------------

//...

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

//-----------------------------------------------------------------------------
// WMI APIs
//-----------------------------------------------------------------------------

bool ar6kWmiStartup(Ar6kDev* dev);

bool ar6kWmiSimpleCmd(Ar6kDev* dev, Ar6kWmiCmdId cmdid);
bool ar6kWmiSimpleCmdWithParam32(Ar6kDev* dev, Ar6kWmiCmdId cmdid, u32 param);

MEOW_INLINE bool ar6kWmiGetChannelList(Ar6kDev* dev)
{
	return ar6kWmiSimpleCmd(dev, Ar6kWmiCmdId_GetChannelList);
}

MEOW_INLINE bool ar6kWmiSetErrorReportBitmask(Ar6kDev* dev, u32 bitmask)
{
	return ar6kWmiSimpleCmdWithParam32(dev, Ar6kWmiCmdId_TargetErrorReportBitmask, bitmask);
}
