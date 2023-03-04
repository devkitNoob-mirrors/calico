#pragma once
#include <calico/nds/pxi.h>
#include <calico/nds/wlmgr.h>

#define PXI_WLMGR_NUM_CREDITS 32

typedef enum PxiWlMgrCmd {
	PxiWlMgrCmd_Start       = 0,
	PxiWlMgrCmd_Stop        = 1,
	PxiWlMgrCmd_StartScan   = 2,
	PxiWlMgrCmd_Associate   = 3,
	PxiWlMgrCmd_Deassociate = 4,
} PxiWlMgrCmd;

MEOW_CONSTEXPR u32 pxiWlMgrMakeCmd(PxiWlMgrCmd type, unsigned imm)
{
	return (type & 0x1f) | (imm << 5);
}

MEOW_CONSTEXPR PxiWlMgrCmd pxiWlMgrCmdGetType(u32 msg)
{
	return (PxiWlMgrCmd)(msg & 0x1f);
}

MEOW_CONSTEXPR unsigned pxiWlMgrCmdGetImm(u32 msg)
{
	return msg >> 5;
}

MEOW_CONSTEXPR u32 pxiWlMgrMakeEvent(WlMgrEvent type, unsigned imm)
{
	return (type & 0xf) | (imm << 4);
}

MEOW_CONSTEXPR WlMgrEvent pxiWlMgrEventGetType(u32 msg)
{
	return (WlMgrEvent)(msg & 0xf);
}

MEOW_CONSTEXPR unsigned pxiWlMgrEventGetImm(u32 msg)
{
	return msg >> 4;
}
