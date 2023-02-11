#pragma once
#include <calico/types.h>
#include <calico/system/mailbox.h>
#include <calico/nds/bios.h>
#include <calico/nds/sound.h>
#include <calico/nds/arm7/sound.h>
#include "../../pxi/sound.h"

#define SOUND_DEBUG

#ifdef SOUND_DEBUG
#include <calico/system/dietprint.h>
#else
#define dietPrint(...) ((void)0)
#endif

typedef struct SoundState {
	u16 channel_mask;
	u16 pxi_credits;
} SoundState;

extern SoundState g_soundState;

void _soundPxiProcess(Mailbox* mb, bool do_credit_update);
