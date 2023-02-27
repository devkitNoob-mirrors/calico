#pragma once
#if !defined(__NDS__) || !defined(ARM9)
#error "This header file is only for NDS ARM9"
#endif

#include "../sound.h"

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
void soundPreparePcm(
	unsigned ch, unsigned vol, unsigned pan, unsigned timer,
	SoundMode mode, SoundFmt fmt, const void* sad, unsigned pnt, unsigned len);
void soundPreparePsg(unsigned ch, unsigned vol, unsigned pan, unsigned timer, SoundDuty duty);
void soundStart(u16 ch_mask);
void soundStop(u16 ch_mask);
void soundChSetVolume(unsigned ch, unsigned vol);
void soundChSetPan(unsigned ch, unsigned pan);
void soundChSetTimer(unsigned ch, unsigned timer);
void soundChSetDuty(unsigned ch, SoundDuty duty);
