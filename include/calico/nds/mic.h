#pragma once
#if !defined(__NDS__)
#error "This header file is only for NDS"
#endif

#include "../types.h"
#include "../system/sysclock.h"
#include "timer.h"

typedef enum MicFmt {
	MicFmt_Pcm8  = 0,
	MicFmt_Pcm16 = 1,
} MicFmt;

typedef enum MicMode {
	MicMode_OneShot      = 0,
	MicMode_Repeat       = 1,
	MicMode_DoubleBuffer = 2,
} MicMode;

typedef enum MicRate {
	MicRate_Full = 0,
	MicRate_Div2 = 1,
	MicRate_Div3 = 2,
	MicRate_Div4 = 3,
} MicRate;

MK_CONSTEXPR unsigned soundTimerFromMicRate(MicRate rate)
{
	return 512*((unsigned)rate + 1);
}
