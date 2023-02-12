#pragma once
#if !defined(__NDS__) || !defined(ARM9)
#error "This header file is only for NDS ARM9"
#endif

#include "../sound.h"

void soundInit(void);
void soundSynchronize(void);

void soundPreparePcm(
	unsigned ch, unsigned vol, unsigned pan, unsigned timer,
	SoundMode mode, SoundFmt fmt, const void* sad, unsigned pnt, unsigned len);
void soundPreparePsg(unsigned ch, unsigned vol, unsigned pan, unsigned timer, SoundDuty duty);
void soundStart(u16 ch_mask);
void soundStop(u16 ch_mask);
