// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#include <calico/arm/mpu.h>

#define _MPU_AUTOGEN(_name, _reg) \
extern u32 armMpuGet##_name(void); \
extern void armMpuSet##_name(u32 value);

_MPU_ACCESSORS
