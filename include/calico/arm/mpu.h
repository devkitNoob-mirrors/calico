#pragma once
#if __ARM_ARCH < 5
#error "This header is for ARMv5+ only"
#endif

#include "../types.h"
#include "cp15.h"

#define _MPU_ACCESSORS \
	_MPU_AUTOGEN(DCacheConfig, "c2, c0, 0") \
	_MPU_AUTOGEN(ICacheConfig, "c2, c0, 1") \
	_MPU_AUTOGEN(WrBufConfig,  "c3, c0, 0") \
	_MPU_AUTOGEN(DataPerm,     "c5, c0, 2") \
	_MPU_AUTOGEN(CodePerm,     "c5, c0, 3") \
	_MPU_AUTOGEN(Region0,      "c6, c0, 0") \
	_MPU_AUTOGEN(Region1,      "c6, c1, 0") \
	_MPU_AUTOGEN(Region2,      "c6, c2, 0") \
	_MPU_AUTOGEN(Region3,      "c6, c3, 0") \
	_MPU_AUTOGEN(Region4,      "c6, c4, 0") \
	_MPU_AUTOGEN(Region5,      "c6, c5, 0") \
	_MPU_AUTOGEN(Region6,      "c6, c6, 0") \
	_MPU_AUTOGEN(Region7,      "c6, c7, 0")

MK_EXTERN_C_START

#if !__thumb__

#define _MPU_AUTOGEN(_name, _reg) \
\
MK_EXTINLINE u32 armMpuGet##_name(void) \
{ \
	u32 value; \
	__asm__ __volatile__("mrc p15, 0, %0, " _reg : "=r" (value)); \
	return value; \
} \
\
MK_EXTINLINE void armMpuSet##_name(u32 value) \
{ \
	__asm__ __volatile__ ("mcr p15, 0, %0, " _reg :: "r" (value)); \
}

#else

#define _MPU_AUTOGEN(_name, _reg) \
MK_EXTERN32 u32 armMpuGet##_name(void); \
MK_EXTERN32 void armMpuSet##_name(u32 value);

#endif

_MPU_ACCESSORS

#undef _MPU_AUTOGEN

MK_CONSTEXPR u32 armMpuDefineRegion(uptr addr, unsigned sz)
{
	return (addr &~ 0xfff) | (sz & 0x3e) | CP15_PU_ENABLE;
}

MK_INLINE void armMpuSetRegion(unsigned id, u32 config)
{
	switch (id & 7) {
		default:
		case 0: armMpuSetRegion0(config); break;
		case 1: armMpuSetRegion1(config); break;
		case 2: armMpuSetRegion2(config); break;
		case 3: armMpuSetRegion3(config); break;
		case 4: armMpuSetRegion4(config); break;
		case 5: armMpuSetRegion5(config); break;
		case 6: armMpuSetRegion6(config); break;
		case 7: armMpuSetRegion7(config); break;
	}
}

MK_INLINE u32 armMpuGetRegion(unsigned id)
{
	switch (id & 7) {
		default:
		case 0: return armMpuGetRegion0();
		case 1: return armMpuGetRegion1();
		case 2: return armMpuGetRegion2();
		case 3: return armMpuGetRegion3();
		case 4: return armMpuGetRegion4();
		case 5: return armMpuGetRegion5();
		case 6: return armMpuGetRegion6();
		case 7: return armMpuGetRegion7();
	}
}

MK_INLINE void armMpuSetRegionDCacheEnable(unsigned id, bool enable)
{
	u32 bit = 1U << (id&7);
	u32 config = armMpuGetDCacheConfig() &~ bit;
	armMpuSetDCacheConfig(enable ? (config | bit) : config);
}

MK_INLINE void armMpuSetRegionICacheEnable(unsigned id, bool enable)
{
	u32 bit = 1U << (id&7);
	u32 config = armMpuGetICacheConfig() &~ bit;
	armMpuSetICacheConfig(enable ? (config | bit) : config);
}

MK_INLINE void armMpuSetRegionWrBufEnable(unsigned id, bool enable)
{
	u32 bit = 1U << (id&7);
	u32 config = armMpuGetWrBufConfig() &~ bit;
	armMpuSetWrBufConfig(enable ? (config | bit) : config);
}

MK_INLINE void armMpuSetRegionDataPerm(unsigned id, unsigned perm)
{
	unsigned pos = 4*(id&7);
	u32 config = armMpuGetDataPerm() &~ (0xf << pos);
	armMpuSetDataPerm(config | ((perm&0xf) << pos));
}

MK_INLINE void armMpuSetRegionCodePerm(unsigned id, unsigned perm)
{
	unsigned pos = 4*(id&7);
	u32 config = armMpuGetCodePerm() &~ (0xf << pos);
	armMpuSetCodePerm(config | ((perm&0xf) << pos));
}

MK_INLINE void armMpuSetRegionAddrSize(unsigned id, uptr addr, unsigned sz)
{
	armMpuSetRegion(id, armMpuDefineRegion(addr, sz));
}

MK_INLINE void armMpuClearRegion(unsigned id)
{
	armMpuSetRegion(id, 0);
}

MK_EXTERN_C_END
