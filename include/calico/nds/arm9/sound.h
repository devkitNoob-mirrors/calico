#pragma once
#if !defined(__NDS__) || !defined(ARM9)
#error "This header file is only for NDS ARM9"
#endif

#include "../sound.h"

void soundInit(void);
void soundSynchronize(void);
