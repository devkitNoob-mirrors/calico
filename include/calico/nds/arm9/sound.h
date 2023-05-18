#pragma once
#if !defined(__NDS__) || !defined(ARM9)
#error "This header file is only for NDS ARM9"
#endif

#include "../sound.h"

#define SOUND_START (1U<<4)

MEOW_EXTERN_C_START

void soundInit(void);
void soundSynchronize(void);

void soundSetPower(bool enable);
void soundSetAutoUpdate(bool enable);

MEOW_INLINE void soundPowerOn(void)
{
	soundSetPower(true);
}

MEOW_INLINE void soundPowerOff(void)
{
	soundSetPower(false);
}

unsigned soundGetActiveChannels(void);
void soundSetMixerVolume(unsigned vol);
void soundSetMixerConfigDirect(unsigned config);
void soundSetMixerSleep(bool enable);
void soundPreparePcm(
	unsigned ch, unsigned vol, unsigned pan, unsigned timer,
	SoundMode mode, SoundFmt fmt, const void* sad, unsigned pnt, unsigned len);
void soundPreparePsg(unsigned ch, unsigned vol, unsigned pan, unsigned timer, SoundDuty duty);
void soundPrepareCapDirect(unsigned cap, unsigned config, void* dad, unsigned len);
void soundStart(u32 mask);
void soundStop(u32 mask);
void soundChSetVolume(unsigned ch, unsigned vol);
void soundChSetPan(unsigned ch, unsigned pan);
void soundChSetTimer(unsigned ch, unsigned timer);
void soundChSetDuty(unsigned ch, SoundDuty duty);

MEOW_INLINE void soundSetMixerConfig(SoundOutSrc src_l, SoundOutSrc src_r, bool mute_ch1, bool mute_ch3)
{
	unsigned config = soundMakeMixerConfig(src_l, src_r, mute_ch1, mute_ch3);
	soundSetMixerConfigDirect(config);
}

MEOW_INLINE void soundPrepareCap(
	unsigned cap, SoundCapDst dst, SoundCapSrc src, bool loop, SoundCapFmt fmt,
	void* dad, unsigned len)
{
	unsigned config = soundMakeCapConfig(dst, src, loop, fmt);
	soundPrepareCapDirect(cap, config, dad, len);
}

MEOW_EXTERN_C_END
