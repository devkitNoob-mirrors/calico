#include <calico/arm/mpu.h>

#define _MPU_AUTOGEN(_name, _reg) \
\
u32 armMpuGet##_name##FromThumb(void) __asm__("armMpuGet" #_name); \
void armMpuSet##_name##FromThumb(u32 value) __asm__("armMpuSet" #_name); \
\
u32 armMpuGet##_name##FromThumb(void) \
{ \
	return armMpuGet##_name(); \
} \
\
void armMpuSet##_name##FromThumb(u32 value) \
{ \
	armMpuSet##_name(value); \
}

_MPU_ACCESSORS
