#include <calico/types.h>
#include <calico/system/thread.h>
#include <calico/nds/arm9/sound.h>
#include "../transfer.h"
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

MEOW_NOINLINE static bool _soundPxiCheckCredits(unsigned needed_credits)
{
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

MEOW_INLINE void _soundIssueCmdAsync(PxiSoundCmd cmd, unsigned imm, const void* arg, size_t arg_size)
{
	unsigned arg_size_words = (arg_size + 3) / 4;
	bool update_credits = _soundPxiCheckCredits(1 + arg_size_words);
	unsigned msg = pxiSoundMakeCmdMsg(cmd, update_credits, imm);
	if (arg_size) {
		pxiSendWithData(PxiChannel_Sound, msg, (const u32*)arg, arg_size_words);
	} else {
		pxiSend(PxiChannel_Sound, msg);
	}
}

void soundInit(void)
{
	s_soundPxiCredits = PXI_SOUND_NUM_CREDITS;
	pxiSetHandler(PxiChannel_Sound, _soundPxiHandler, NULL);
	pxiWaitRemote(PxiChannel_Sound);
}

void soundSynchronize(void)
{
	bool update_credits = _soundPxiCheckCredits(1);
	pxiSendAndReceive(PxiChannel_Sound, pxiSoundMakeCmdMsg(PxiSoundCmd_Synchronize, update_credits, 0));
}

unsigned soundGetActiveChannels(void)
{
	return s_transferRegion->sound_active_ch_mask;
}

MEOW_INLINE SoundVolDiv _soundCalcVolDiv(unsigned* vol)
{
	if (*vol < 0x80) {
		return SoundVolDiv_16;
	} else if (*vol < 0x200) {
		*vol /= 4;
		return SoundVolDiv_4;
	} else if (*vol < 0x400) {
		*vol /= 8;
		return SoundVolDiv_2;
	} else if (*vol < 0x800) {
		*vol /= 16;
		return SoundVolDiv_1;
	} else {
		*vol = 0x7f;
		return SoundVolDiv_1;
	}
}

void soundPreparePcm(
	unsigned ch, unsigned vol, unsigned pan, unsigned timer,
	SoundMode mode, SoundFmt fmt, const void* sad, unsigned pnt, unsigned len)
{
	SoundVolDiv voldiv = _soundCalcVolDiv(&vol);
	PxiSoundArgPreparePcm arg = {
		.sad    = ((u32)sad - MM_MAINRAM) / 4,
		.mode   = mode,
		.vol    = vol,
		.start  = 0,

		.timer  = timer,
		.pnt    = pnt,

		.len    = len,
		.voldiv = voldiv,
		.fmt    = fmt,
		._pad   = 0,
		.ch     = ch,
	};

	_soundIssueCmdAsync(PxiSoundCmd_PreparePcm, pan, &arg, sizeof(arg));
}

void soundPreparePsg(unsigned ch, unsigned vol, unsigned pan, unsigned timer, SoundDuty duty)
{
	SoundVolDiv voldiv = _soundCalcVolDiv(&vol);
	PxiSoundArgPreparePsg arg = {
		.timer  = timer,
		.vol    = vol,
		.voldiv = voldiv,
		.duty   = duty,
		.ch     = ch-8,
		.start  = 0,
	};

	_soundIssueCmdAsync(PxiSoundCmd_PreparePsg, pan, &arg, sizeof(arg));
}

void soundStart(u16 ch_mask)
{
	_soundIssueCmdAsync(PxiSoundCmd_Start, ch_mask, NULL, 0);
}

void soundStop(u16 ch_mask)
{
	_soundIssueCmdAsync(PxiSoundCmd_Stop, ch_mask, NULL, 0);
}
