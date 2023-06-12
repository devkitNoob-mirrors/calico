#pragma once
#include "types.h"

MEOW_EXTERN_C_START

void mwlDevWakeUp(void);
void mwlDevReset(void);
void mwlDevSetChannel(unsigned ch);
void mwlDevShutdown(void);

MEOW_EXTERN_C_END
