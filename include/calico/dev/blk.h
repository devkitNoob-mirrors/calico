#pragma once
#include "../types.h"

#define BLK_SECTOR_SZ 512

typedef enum BlkDevice {

#if defined(__NDS__)

	BlkDevice_Dldi       = 0,
	BlkDevice_TwlSdCard  = 1,
	BlkDevice_TwlNand    = 2,
	BlkDevice_TwlNandAes = 3,

#else
#error "Unsupported platform"
#endif

} BlkDevice;

typedef void (*BlkDevCallbackFn)(BlkDevice dev, bool insert);

void blkInit(void);
void blkSetDevCallback(BlkDevCallbackFn fn);

bool blkDevIsPresent(BlkDevice dev);
bool blkDevInit(BlkDevice dev);
u32  blkDevGetSectorCount(BlkDevice dev);
bool blkDevReadSectors(BlkDevice dev, void* buffer, u32 first_sector, u32 num_sectors);
bool blkDevWriteSectors(BlkDevice dev, const void* buffer, u32 first_sector, u32 num_sectors);
