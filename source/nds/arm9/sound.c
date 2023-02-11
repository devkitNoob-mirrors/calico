#include <calico/types.h>
#include <calico/system/thread.h>
#include <calico/nds/arm9/sound.h>
#include "../pxi/sound.h"

static ThrListNode s_soundPxiCreditWaitList;
static u16 s_soundPxiCredits;

static void _soundPxiHandler(void* user, u32 data)
{
	PxiSoundEvent evt = pxiSoundEventGetType(data);
	unsigned imm = pxiSoundEventGetImm(data);

	switch (evt) {
		default: break;

		case PxiSoundEvent_UpdateCredits: {
			s_soundPxiCredits += imm;
			if (s_soundPxiCreditWaitList.next) {
				threadUnblockOneByValue(&s_soundPxiCreditWaitList, (u32)user);
			}
			break;
		}
	}
}

static bool _soundPxiCheckCredits(size_t body_len)
{
	unsigned needed_credits = 1 + body_len/4;

	// If we don't have enough credits, wait until we do
	ArmIrqState st = armIrqLockByPsr();
	unsigned avail;
	while ((avail = s_soundPxiCredits) < needed_credits) {
		threadBlock(&s_soundPxiCreditWaitList, 0); // izQKJJ9tyZc
	}
	avail -= needed_credits;
	s_soundPxiCredits = avail;
	armIrqUnlockByPsr(st);

	return avail < PXI_SOUND_CREDIT_UPDATE_THRESHOLD;
}

void soundInit(void)
{
	s_soundPxiCredits = PXI_SOUND_NUM_CREDITS;
	pxiSetHandler(PxiChannel_Sound, _soundPxiHandler, NULL);
	pxiWaitRemote(PxiChannel_Sound);
}

void soundSynchronize(void)
{
	bool update_credits = _soundPxiCheckCredits(0);
	pxiSendAndReceive(PxiChannel_Sound, pxiSoundMakeCmdMsg(PxiSoundCmd_Synchronize, update_credits, 0));
}
