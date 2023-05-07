#pragma once
#if !defined(__NDS__) || !defined(ARM7)
#error "This header file is only for NDS ARM7"
#endif

#include "../../types.h"

MEOW_EXTERN_C_START

bool twlblkInit(void);

bool twlSdInit(void);
bool twlSdIsInserted(void);
bool twlSdReadSectors(void* buffer, u32 first_sector, u32 num_sectors);
bool twlSdWriteSectors(const void* buffer, u32 first_sector, u32 num_sectors);

bool twlNandInit(void);
bool twlNandReadSectors(void* buffer, u32 first_sector, u32 num_sectors);
bool twlNandReadSectorsAes(void* buffer, u32 first_sector, u32 num_sectors);

MEOW_EXTERN_C_END
