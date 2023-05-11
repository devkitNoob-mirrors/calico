#pragma once
#include <calico/nds/pxi.h>

typedef enum PxiMicCmd {
	PxiMicCmd_SetCpuTimer = 0,
	PxiMicCmd_SetDmaRate  = 1,
	PxiMicCmd_Start       = 2,
	PxiMicCmd_Stop        = 3,
} PxiMicCmd;

typedef union PxiMicImmDivTimer {
	unsigned imm;
	struct {
		unsigned div   : 2;
		unsigned timer : 16;
	};
} PxiMicImmDivTimer;

typedef union PxiMicImmStart {
	unsigned imm;
	struct {
		unsigned is_16bit : 1;
		unsigned mode     : 2;
	};
} PxiMicImmStart;

typedef struct PxiMicArgStart {
	u32 dest_addr;
	u32 dest_sz;
} PxiMicArgStart;

MEOW_CONSTEXPR u32 pxiMicMakeCmdMsg(PxiMicCmd cmd, unsigned imm)
{
	return (cmd & 7) | (imm << 3);
}

MEOW_CONSTEXPR PxiMicCmd pxiMicCmdGetType(u32 msg)
{
	return (PxiMicCmd)(msg & 7);
}

MEOW_CONSTEXPR unsigned pxiMicCmdGetImm(u32 msg)
{
	return msg >> 3;
}
