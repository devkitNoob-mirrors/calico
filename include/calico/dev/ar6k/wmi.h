#pragma once
#include "base.h"
#include "htc.h"

typedef enum Ar6kWmiEventId {
	Ar6kWmiEventId_Ready = 0x1001,
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

//-----------------------------------------------------------------------------
// WMI APIs
//-----------------------------------------------------------------------------

void ar6kWmiWaitReady(Ar6kDev* dev);
