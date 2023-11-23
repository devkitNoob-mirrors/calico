#pragma once
#if !defined(__NDS__) || !(defined(ARM9) || defined(ARM7))
#error "This header file is only for NDS"
#endif

#include "../types.h"
#include "../system/sysclock.h"
#include "io.h"

#define SCFG_MCINSREM_CLKDIV 0x200
#define SCFG_MCINSREM_CLOCK  (SYSTEM_CLOCK/SCFG_MCINSREM_CLKDIV)

#define REG_SCFG_ROM MK_REG(u32, IO_SCFG_ROM)
#define REG_SCFG_CLK MK_REG(u16, IO_SCFG_CLK)
#define REG_SCFG_EXT MK_REG(u32, IO_SCFG_EXT)
#define REG_SCFG_MC  MK_REG(u16, IO_SCFG_MC)

#ifdef ARM7
#define REG_SCFG_A9ROM MK_REG(u8,  IO_SCFG_ROM+0)
#define REG_SCFG_A7ROM MK_REG(u8,  IO_SCFG_ROM+1)
#define REG_SCFG_RST   MK_REG(u16, IO_SCFG_RST)
#define REG_SCFG_MCINS MK_REG(u16, IO_SCFG_MCINS)
#define REG_SCFG_MCREM MK_REG(u16, IO_SCFG_MCREM)
#define REG_SCFG_WL    MK_REG(u8,  IO_SCFG_WL)
#define REG_SCFG_OP    MK_REG(u8,  IO_SCFG_OP)

#define REG_OTP_CID    MK_REG(u64, IO_OTP_CID)
#define REG_OTP_ERROR  MK_REG(u8,  IO_OTP_ERROR)
#endif

#ifdef ARM9
#define REG_SCFG_JTAG MK_REG(u16, IO_SCFG_JTAG)
#endif

#define SCFG_ROM_MASK        (3U<<0)
#define SCFG_ROM_BROM_PROT   (1U<<0)
#define SCFG_ROM_MODE_TWL    (0U<<1)
#define SCFG_ROM_MODE_NTR    (1U<<1)

#define SCFG_ROM_A9MASK      SCFG_ROM_MASK
#define SCFG_ROM_A9BROM_PROT SCFG_ROM_BROM_PROT
#define SCFG_ROM_A9MODE_TWL  SCFG_ROM_MODE_TWL
#define SCFG_ROM_A9MODE_NTR  SCFG_ROM_MODE_NTR

#ifdef ARM7
#define SCFG_ROM_A7MASK      (SCFG_ROM_MASK<<8)
#define SCFG_ROM_A7BROM_PROT (SCFG_ROM_BROM_PROT<<8)
#define SCFG_ROM_A7MODE_TWL  (SCFG_ROM_MODE_TWL<<8)
#define SCFG_ROM_A7MODE_NTR  (SCFG_ROM_MODE_NTR<<8)
#define SCFG_ROM_OTP_PROT    (1U<<10)

#define SCFG_CLK_TMIO0       (1U<<0)
#define SCFG_CLK_UNK1        (1U<<1)
#define SCFG_CLK_AES         (1U<<2)
#define SCFG_CLK_NWRAM       (1U<<7)
#define SCFG_CLK_CODEC       (1U<<8)
#endif

#ifdef ARM9
#define SCFG_CLK_CPU_67MHz   (0U<<0)
#define SCFG_CLK_CPU_134MHz  (1U<<0)
#define SCFG_CLK_DSP         (1U<<1)
#define SCFG_CLK_CAM_IFACE   (1U<<2)
#define SCFG_CLK_NWRAM       (1U<<7) // read-only
#define SCFG_CLK_CAM_EXT     (1U<<8)
#endif

#define SCFG_EXT_FIX_DMA   (1U<<0)  // arm9 rw, arm7 rw (per-cpu)
#define SCFG_EXT_FIX_CARD  (1U<<7)  // arm9 rw, arm7 ro
#define SCFG_EXT_TWL_IRQ   (1U<<8)  // arm9 rw, arm7 rw (per-cpu)
#define SCFG_EXT_TWL_LCD   (1U<<12) // arm9 rw, arm7 ro
#define SCFG_EXT_TWL_VRAM  (1U<<13) // arm9 rw, arm7 ro
#define SCFG_EXT_RAM_MASK  (3U<<14) // arm9 rw, arm7 ro
#define SCFG_EXT_RAM_4MB   (0U<<14)
#define SCFG_EXT_RAM_16MB  (2U<<14)
#define SCFG_EXT_RAM_32MB  (3U<<14)
#define SCFG_EXT_HAS_NDMA  (1U<<16) // arm9 rw, arm7 rw (per-cpu)
#define SCFG_EXT_HAS_CARD2 (1U<<24) // arm9 ro, arm7 rw
#define SCFG_EXT_HAS_NWRAM (1U<<25) // arm9 ro, arm7 rw
#define SCFG_EXT_HAS_SCFG  (1U<<31) // arm9 rw, arm7 rw (per-cpu)

#define SCFG_EXT7_FIX_SND_DMA (1U<<1)
#define SCFG_EXT7_FIX_SND     (1U<<2)
#define SCFG_EXT7_TWL_SPI     (1U<<9)
#define SCFG_EXT7_TWL_SND_DMA (1U<<10)
#define SCFG_EXT7_TWL_UNK11   (1U<<11)
#define SCFG_EXT7_HAS_AES     (1U<<17)
#define SCFG_EXT7_HAS_TMIO0   (1U<<18)
#define SCFG_EXT7_HAS_TMIO1   (1U<<19)
#define SCFG_EXT7_HAS_MICEX   (1U<<20)
#define SCFG_EXT7_HAS_SNDEX   (1U<<21)
#define SCFG_EXT7_HAS_I2C     (1U<<22)
#define SCFG_EXT7_HAS_GPIO    (1U<<23)
#define SCFG_EXT7_HAS_UNK28   (1U<<28)

#define SCFG_EXT9_FIX_3D_GEO  (1U<<1)
#define SCFG_EXT9_FIX_3D_REN  (1U<<2)
#define SCFG_EXT9_FIX_2D      (1U<<3)
#define SCFG_EXT9_FIX_DIV     (1U<<4)
#define SCFG_EXT9_HAS_CAM     (1U<<17)
#define SCFG_EXT9_HAS_DSP     (1U<<18)

#define SCFG_MC_IS_EJECTED    (1U<<0)
#define SCFG_MC_POWER_MASK    (3U<<2)
#define SCFG_MC_POWER_OFF     (0U<<2)
#define SCFG_MC_POWER_ON_REQ  (1U<<2)
#define SCFG_MC_POWER_ON      (2U<<2)
#define SCFG_MC_POWER_OFF_REQ (3U<<2)

MK_EXTERN_C_START

MK_CONSTEXPR u16 scfgCalcMcInsRem(unsigned ms)
{
	return (SCFG_MCINSREM_CLOCK * ms + 500U) / 1000U;
}

MK_INLINE bool scfgIsPresent(void)
{
	return (REG_SCFG_EXT & SCFG_EXT_HAS_SCFG) != 0;
}

MK_EXTERN_C_END
