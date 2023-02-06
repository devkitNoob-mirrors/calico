#pragma once
#if !defined(__NDS__) || !defined(ARM7)
#error "This header file is only for NDS ARM7"
#endif

#include "../../types.h"
#include "spi.h"

typedef enum TscResult {
	TscResult_None  = 0,
	TscResult_Noisy = 1,
	TscResult_Valid = 2,
} TscResult;

typedef struct TscTouchData {
	u16 x, y;
} TscTouchData;

MEOW_CONSTEXPR unsigned tscAbs(signed x)
{
	return x >= 0 ? x : (-x);
}

void tscInit(void);
TscResult tscReadTouch(TscTouchData* out, unsigned diff_threshold, u16* out_max_diff);
