#pragma once
#if !defined(__NDS__)
#error "This header file is only for NDS"
#endif

#include "../types.h"
#include "../system/sysclock.h"

#define SOUND_CLOCK         (SYSTEM_CLOCK/2)
#define SOUND_MIXER_FREQ_HZ (SYSTEM_CLOCK/1024)
#define SOUND_UPDATE_HZ     192
#define SOUND_NUM_CHANNELS  16

typedef enum SoundOutSrc {
	SoundOutSrc_Mixer  = 0,
	SoundOutSrc_Ch1    = 1,
	SoundOutSrc_Ch3    = 2,
	SoundOutSrc_Ch1Ch3 = 3,
} SoundOutSrc;

typedef enum SoundVolDiv {
	SoundVolDiv_1  = 0,
	SoundVolDiv_2  = 1,
	SoundVolDiv_4  = 2,
	SoundVolDiv_16 = 3,
} SoundVolDiv;

typedef enum SoundMode {
	SoundMode_Manual  = 0,
	SoundMode_Repeat  = 1,
	SoundMode_OneShot = 2,
} SoundMode;

typedef enum SoundFmt {
	SoundFmt_Pcm8     = 0,
	SoundFmt_Pcm16    = 1,
	SoundFmt_ImaAdpcm = 2,
	SoundFmt_Psg      = 3,
} SoundFmt;

MEOW_CONSTEXPR unsigned soundTimerFromHz(unsigned hz)
{
	return (SOUND_CLOCK + hz/2) / hz;
}
