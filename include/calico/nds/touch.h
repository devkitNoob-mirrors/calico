#pragma once
#if !defined(__NDS__)
#error "This header file is only for NDS"
#endif

#include "../types.h"

typedef struct MEOW_STRUCT_ALIGN(4) TouchData {
	u16 px, py;
	u16 rawx, rawy;
} TouchData;

#if defined(ARM7)
void touchInit(void);
void touchStartServer(unsigned lyc, u8 thread_prio);
void touchLoadCalibration(void);
#endif

bool touchRead(TouchData* out);
