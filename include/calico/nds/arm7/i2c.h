#pragma once
#if !defined(__NDS__) || !defined(ARM7)
#error "This header file is only for NDS ARM7"
#endif

#include "../../types.h"
#include "../../system/mutex.h"
#include "../io.h"

#define REG_I2C_DATA MEOW_REG(u8, IO_I2C_DATA)
#define REG_I2C_CNT  MEOW_REG(u8, IO_I2C_CNT)

MEOW_EXTERN_C_START

typedef enum I2cDevice {
	I2cDev_MCU = 0x4a,
} I2cDevice;

extern Mutex g_i2cMutex;

MEOW_INLINE void i2cLock(void)
{
	mutexLock(&g_i2cMutex);
}

MEOW_INLINE void i2cUnlock(void)
{
	mutexUnlock(&g_i2cMutex);
}

bool i2cWriteRegister8(I2cDevice dev, u8 reg, u8 data);
u8 i2cReadRegister8(I2cDevice dev, u8 reg);

MEOW_EXTERN_C_END
