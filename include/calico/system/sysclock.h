#pragma once

#if defined(__GBA__)
#define SYSTEM_CLOCK 0x1000000 // GBA system bus speed is exactly 2^24 Hz ~= 16.78 MHz
#elif defined(__NDS__)
#define SYSTEM_CLOCK 0x1FF61FE // NDS system bus speed is approximately 33.51 MHz
#else
#error "This header file is only for GBA and NDS"
#endif
