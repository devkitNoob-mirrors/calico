#pragma once
#if !defined(__GBA__)
#error "This header file is only for GBA"
#endif

#include "../types.h"
#include "io.h"

#define REG_DMAxSAD(_x)   MEOW_REG(u32, IO_DMAxSAD(_x))
#define REG_DMAxDAD(_x)   MEOW_REG(u32, IO_DMAxDAD(_x))
#define REG_DMAxCNT(_x)   MEOW_REG(u32, IO_DMAxCNT(_x))
#define REG_DMAxCNT_L(_x) MEOW_REG(u16, IO_DMAxCNT(_x)+0)
#define REG_DMAxCNT_H(_x) MEOW_REG(u16, IO_DMAxCNT(_x)+2)

typedef enum DmaMode {
	DmaMode_Increment  = 0,
	DmaMode_Decrement  = 1,
	DmaMode_Fixed      = 2,
	DmaMode_IncrReload = 3,
} DmaMode;

typedef enum DmaTiming {
	DmaTiming_Immediate = 0,
	DmaTiming_VBlank    = 1,
	DmaTiming_HBlank    = 2,
	DmaTiming_Device    = 3,

	DmaTiming_Sound     = DmaTiming_Device,
	DmaTiming_Capture   = DmaTiming_Device,
} DmaTiming;

#define DMA_MODE_DST(_x) (((_x)&3)<<5)
#define DMA_MODE_SRC(_x) (((_x)&3)<<7)
#define DMA_REPEAT       (1<<9)
#define DMA_UNIT_HWORD   (0<<10)
#define DMA_UNIT_WORD    (1<<10)
#define DMA_CART_DREQ    (1<<11)
#define DMA_TIMING(_x)   (((_x)&3)<<12)
#define DMA_ENABLE_IRQ   (1<<14)
#define DMA_ENABLE       (1<<15)

MEOW_INLINE void dmaCopyWords(unsigned id, void* dst, const void* src, size_t size)
{
	REG_DMAxSAD(id) = (u32)src;
	REG_DMAxDAD(id) = (u32)dst;
	REG_DMAxCNT_L(id) = size/4;
	REG_DMAxCNT_H(id) =
		DMA_MODE_DST(DmaMode_Increment) |
		DMA_MODE_SRC(DmaMode_Increment) |
		DMA_UNIT_WORD |
		DMA_TIMING(DmaTiming_Immediate) |
		DMA_ENABLE;
}
