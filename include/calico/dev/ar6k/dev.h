#pragma once
#include "../../types.h"
#include "../sdio.h"

typedef struct Ar6kDev {
	SdioCard* sdio;
	u32 hia_addr;
} Ar6kDev;

bool ar6kDevInit(Ar6kDev* dev, SdioCard* sdio);

bool ar6kDevReadRegDiag(Ar6kDev* dev, u32 addr, u32* out);
bool ar6kDevWriteRegDiag(Ar6kDev* dev, u32 addr, u32 value);
