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
#define SOUND_NUM_CAPTURES  2

MEOW_EXTERN_C_START

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

typedef enum SoundDuty {
	SoundDuty_12_5 = 0,
	SoundDuty_25   = 1,
	SoundDuty_37_5 = 2,
	SoundDuty_50   = 3,
	SoundDuty_62_5 = 4,
	SoundDuty_75   = 5,
	SoundDuty_87_5 = 6,
	SoundDuty_0    = 7,
} SoundDuty;

typedef enum SoundCapDst {
	// Generic
	SoundCapDst_ChBNormal = 0,
	SoundCapDst_ChBAddToA = 1,

	// For capture 0
	SoundCapDst_Ch1Normal = SoundCapDst_ChBNormal,
	SoundCapDst_Ch1AddTo0 = SoundCapDst_ChBAddToA,

	// For capture 1
	SoundCapDst_Ch3Normal = SoundCapDst_ChBNormal,
	SoundCapDst_Ch3AddTo2 = SoundCapDst_ChBAddToA,
} SoundCapDst;

typedef enum SoundCapSrc {
	// Generic
	SoundCapSrc_Mixer      = 0,
	SoundCapSrc_ChA        = 1,

	// For capture 0
	SoundCapSrc_LeftMixer  = SoundCapSrc_Mixer,
	SoundCapSrc_Ch0        = SoundCapSrc_ChA,

	// For capture 1
	SoundCapSrc_RightMixer = SoundCapSrc_Mixer,
	SoundCapSrc_Ch2        = SoundCapSrc_ChA,
} SoundCapSrc;

typedef enum SoundCapFmt {
	SoundCapFmt_Pcm16 = 0,
	SoundCapFmt_Pcm8  = 1,
} SoundCapFmt;

MEOW_CONSTEXPR unsigned soundTimerFromHz(unsigned hz)
{
	return (SOUND_CLOCK + hz/2) / hz;
}

MEOW_CONSTEXPR unsigned soundMakeMixerConfig(SoundOutSrc src_l, SoundOutSrc src_r, bool mute_ch1, bool mute_ch3)
{
	return (src_l&3) | ((src_r&3)<<2) | (mute_ch1<<4) | (mute_ch3<<5);
}

MEOW_CONSTEXPR unsigned soundMakeCapConfig(SoundCapDst dst, SoundCapSrc src, bool loop, SoundCapFmt fmt)
{
	return (dst&1) | ((src&1)<<1) | ((!loop)<<2) | ((fmt&1)<<3);
}

MEOW_EXTERN_C_END
