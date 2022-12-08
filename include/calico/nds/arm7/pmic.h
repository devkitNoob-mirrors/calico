#pragma once
#if !defined(__NDS__) || !defined(ARM7)
#error "This header file is only for NDS ARM7"
#endif

#include "../../types.h"
#include "spi.h"

typedef enum PmicRegister {
	PmicReg_Control        = 0x00,
	PmicReg_BatteryStatus  = 0x01,
	PmicReg_MicAmpControl  = 0x02,
	PmicReg_MicAmpGain     = 0x03,
	PmicReg_BacklightLevel = 0x04,
} PmicRegister;

void pmicWriteRegister(PmicRegister reg, u8 data);
u8 pmicReadRegister(PmicRegister reg);
