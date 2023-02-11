#pragma once
#include <calico/nds/pxi.h>

#define PXI_SOUND_NUM_CREDITS 32
#define PXI_SOUND_CREDIT_UPDATE_THRESHOLD 4

typedef enum PxiSoundCmd {
	PxiSoundCmd_Synchronize = 0,
	// etc
} PxiSoundCmd;

typedef enum PxiSoundEvent {
	PxiSoundEvent_UpdateCredits = 0,
} PxiSoundEvent;

MEOW_CONSTEXPR u32 pxiSoundMakeCmdMsg(PxiSoundCmd cmd, bool update_credits, unsigned imm)
{
	return (cmd & 0x7f) | (update_credits ? (1U<<7) : 0) | (imm << 8);
}

MEOW_CONSTEXPR PxiSoundCmd pxiSoundCmdGetType(u32 msg)
{
	return (PxiSoundCmd)(msg & 0x7f);
}

MEOW_CONSTEXPR bool pxiSoundCmdIsUpdateCredits(u32 msg)
{
	return (msg >> 7) & 1;
}

MEOW_CONSTEXPR unsigned pxiSoundCmdGetImm(u32 msg)
{
	return msg >> 8;
}

MEOW_CONSTEXPR u32 pxiSoundMakeEventMsg(PxiSoundEvent evt, unsigned imm)
{
	return (evt & 0xf) | (imm << 4);
}

MEOW_CONSTEXPR PxiSoundEvent pxiSoundEventGetType(u32 msg)
{
	return (PxiSoundEvent)(msg & 0xf);
}

MEOW_CONSTEXPR unsigned pxiSoundEventGetImm(u32 msg)
{
	return msg >> 4;
}
