#pragma once
#if !defined(__NDS__) || !defined(ARM9)
#error "This header file is only for NDS ARM9"
#endif

#include "../../types.h"
#include "../io.h"

#define REG_VRAMCNT_A MEOW_REG(u8, IO_VRAMCNT_A)
#define REG_VRAMCNT_B MEOW_REG(u8, IO_VRAMCNT_B)
#define REG_VRAMCNT_C MEOW_REG(u8, IO_VRAMCNT_C)
#define REG_VRAMCNT_D MEOW_REG(u8, IO_VRAMCNT_D)
#define REG_VRAMCNT_E MEOW_REG(u8, IO_VRAMCNT_E)
#define REG_VRAMCNT_F MEOW_REG(u8, IO_VRAMCNT_F)
#define REG_VRAMCNT_G MEOW_REG(u8, IO_VRAMCNT_G)
#define REG_VRAMCNT_H MEOW_REG(u8, IO_VRAMCNT_H)
#define REG_VRAMCNT_I MEOW_REG(u8, IO_VRAMCNT_I)

#define REG_VRAMCNT_ABCD MEOW_REG(u32, IO_VRAMCNT_A)
#define REG_VRAMCNT_AB   MEOW_REG(u16, IO_VRAMCNT_A)
#define REG_VRAMCNT_CD   MEOW_REG(u16, IO_VRAMCNT_C)
#define REG_VRAMCNT_EF   MEOW_REG(u16, IO_VRAMCNT_E)
#define REG_VRAMCNT_HI   MEOW_REG(u16, IO_VRAMCNT_H)

#define VRAM_ENABLE     (1U<<7)
#define VRAM_MST(_x)    ((_x)&7)
#define VRAM_OFFSET(_x) (((_x)&3)<<3)
#define VRAM_CONFIG(_mst, _off) (VRAM_MST(_mst) | VRAM_OFFSET(_off) | VRAM_ENABLE)
