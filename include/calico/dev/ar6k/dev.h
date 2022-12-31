#pragma once
#include "../../types.h"
#include "../sdio.h"

typedef struct Ar6kEndpoint {
	u16 service_id;
	u16 max_msg_size;
} Ar6kEndpoint;

typedef struct Ar6kDev {
	SdioCard* sdio;
	u32 chip_id;
	u32 hia_addr;

	// HTC
	u32 lookahead;
	u16 credit_size;
	u16 credit_count;
} Ar6kDev;

bool ar6kDevInit(Ar6kDev* dev, SdioCard* sdio);

bool ar6kDevReadRegDiag(Ar6kDev* dev, u32 addr, u32* out);
bool ar6kDevWriteRegDiag(Ar6kDev* dev, u32 addr, u32 value);
