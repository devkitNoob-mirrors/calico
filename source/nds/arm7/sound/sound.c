#include "common.h"
#include <calico/system/thread.h>
#include <calico/nds/pm.h>

#define SOUND_NUM_MAIL_SLOTS 4
#define SOUND_MAIL_EXIT   0
#define SOUND_MAIL_WAKEUP 1
#define SOUND_MAIL_TIMER  2

SoundState g_soundState;

static Thread s_soundSrvThread;
alignas(8) static u8 s_soundSrvThreadStack[1024];

static Mailbox s_soundSrvMailbox, s_soundPxiMailbox;
static u32 s_soundPxiMailboxSlots[PXI_SOUND_NUM_CREDITS];

static void _soundTimerTick(TickTask* task)
{
	// Send timer message to sound server thread
	mailboxTrySend(&s_soundSrvMailbox, SOUND_MAIL_TIMER);
}

static void _soundPxiHandler(void* user, u32 data)
{
	// Add message to PXI mailbox
	mailboxTrySend((Mailbox*)user, data);

	// Wake up sound server thread if needed
	if (s_soundSrvMailbox.pending_slots == 0) {
		mailboxTrySend(&s_soundSrvMailbox, SOUND_MAIL_WAKEUP);
	}
}

static int _soundSrvThreadMain(void* arg)
{
	// TODO: Init DSi I2S interface

	// Set up PXI and mailboxes
	u32 slots[SOUND_NUM_MAIL_SLOTS];
	mailboxPrepare(&s_soundSrvMailbox, slots, SOUND_NUM_MAIL_SLOTS);
	mailboxPrepare(&s_soundPxiMailbox, s_soundPxiMailboxSlots, PXI_SOUND_NUM_CREDITS);
	pxiSetHandler(PxiChannel_Sound, _soundPxiHandler, &s_soundPxiMailbox);

	// Enable sound power
	pmPowerOn(POWCNT_SOUND);

	// Basic reg initialization and cleanup
	REG_SOUNDCNT = SOUNDCNT_VOL(0x7f) | SOUNDCNT_ENABLE;
	svcSoundBias(true, 0x400);
	for (unsigned i = 0; i < SOUND_NUM_CHANNELS; i ++) {
		REG_SOUNDxCNT(i) = 0;
	}

	// Set up tick task for periodic sound updates
	// XX: Official code has a rounding error in the value for the timer period,
	//     which we're incorporating too for compatibility.
	TickTask sound_timer;
	tickTaskStart(&sound_timer, _soundTimerTick, 0, ticksFromHz(SOUND_UPDATE_HZ) + 1);

	for (;;) {
		// Get message
		u32 msg = mailboxRecv(&s_soundSrvMailbox);
		if (msg == SOUND_MAIL_EXIT) {
			break;
		}
		bool is_timer = msg == SOUND_MAIL_TIMER;

		// TODO: Commit channel state to registers

		// Process PXI commands
		_soundPxiProcess(&s_soundPxiMailbox, is_timer);

		// TODO: Process sequencer + sequencer voices

		// Update shared state
		_soundUpdateSharedState();
	}

	return 0;
}

void soundStartServer(u8 thread_prio)
{
	threadPrepare(&s_soundSrvThread, _soundSrvThreadMain, NULL, &s_soundSrvThreadStack[sizeof(s_soundSrvThreadStack)], thread_prio);
	threadStart(&s_soundSrvThread);
}

void _soundUpdateSharedState(void)
{
	unsigned ch_mask = 0;
	for (unsigned i = 0; i < SOUND_NUM_CHANNELS; i ++) {
		if (soundChIsActive(i)) {
			ch_mask |= 1U<<i;
		}
	}

	s_transferRegion->sound_active_ch_mask = ch_mask;
}
