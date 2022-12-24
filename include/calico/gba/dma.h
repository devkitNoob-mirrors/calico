#pragma once
#include "../types.h"

#if defined(__GBA__)
#include "io.h"
#elif defined(__NDS__)
#include "../nds/io.h"
#else
#error "This header file is only for GBA and NDS"
#endif

#define REG_DMAxSAD(_x)   MEOW_REG(u32, IO_DMAxSAD(_x))
#define REG_DMAxDAD(_x)   MEOW_REG(u32, IO_DMAxDAD(_x))
#define REG_DMAxCNT(_x)   MEOW_REG(u32, IO_DMAxCNT(_x))
#define REG_DMAxCNT_L(_x) MEOW_REG(u16, IO_DMAxCNT(_x)+0)
#define REG_DMAxCNT_H(_x) MEOW_REG(u16, IO_DMAxCNT(_x)+2)
#if defined(ARM9)
#define REG_DMAxFIL(_x)   MEOW_REG(u32, IO_DMAxFIL(_x))
#endif

typedef enum DmaMode {
	DmaMode_Increment  = 0,
	DmaMode_Decrement  = 1,
	DmaMode_Fixed      = 2,
	DmaMode_IncrReload = 3,
} DmaMode;

typedef enum DmaTiming {
	DmaTiming_Immediate = 0,
	DmaTiming_VBlank    = 1,

#if defined(__GBA__)
	DmaTiming_HBlank    = 2,
	DmaTiming_Special   = 3,

	DmaTiming_Sound     = DmaTiming_Special, // DMA1 & DMA2
	DmaTiming_DispStart = DmaTiming_Special, // DMA3
#elif defined(__NDS__) && defined(ARM7)
	DmaTiming_Slot1     = 2,
	DmaTiming_Special   = 3,

	DmaTiming_Mitsumi   = DmaTiming_Special, // DMA0 & DMA2
	DmaTiming_Slot2     = DmaTiming_Special, // DMA1 & DMA3
#elif defined(__NDS__) && defined(ARM9)
	DmaTiming_HBlank    = 2,
	DmaTiming_DispStart = 3,
	DmaTiming_MemDisp   = 4,
	DmaTiming_Slot1     = 5,
	DmaTiming_Slot2     = 6,
	DmaTiming_3dFifo    = 7,
#endif

} DmaTiming;

#define DMA_MODE_DST(_x) (((_x)&3)<<5)
#define DMA_MODE_SRC(_x) (((_x)&3)<<7)
#define DMA_REPEAT       (1<<9)
#define DMA_UNIT_16      (0<<10)
#define DMA_UNIT_32      (1<<10)
#if defined(ARM7)
#if defined(__GBA__)
#define DMA_CART_DREQ    (1<<11)
#endif
#define DMA_TIMING(_x)   (((_x)&3)<<12)
#elif defined(ARM9)
#define DMA_TIMING(_x)   (((_x)&7)<<11)
#endif
#define DMA_IRQ_ENABLE   (1<<14)
#define DMA_ENABLE       (1<<15)

MEOW_INLINE bool dmaIsBusy(unsigned id)
{
	return REG_DMAxCNT_H(id) & DMA_ENABLE;
}

MEOW_INLINE void dmaBusyWait(unsigned id)
{
	while (dmaIsBusy(id));
}

MEOW_INLINE void dmaStartCopy32(unsigned id, void* dst, const void* src, size_t size)
{
	REG_DMAxSAD(id) = (u32)src;
	REG_DMAxDAD(id) = (u32)dst;
	REG_DMAxCNT_L(id) = size/4;
	REG_DMAxCNT_H(id) =
		DMA_MODE_DST(DmaMode_Increment) |
		DMA_MODE_SRC(DmaMode_Increment) |
		DMA_UNIT_32 |
		DMA_TIMING(DmaTiming_Immediate) |
		DMA_ENABLE;
}

MEOW_INLINE void dmaStartFill32(unsigned id, void* dst, u32 value, size_t size)
{
#if defined(ARM9)
	REG_DMAxFIL(id) = value;
	REG_DMAxSAD(id) = (u32)&REG_DMAxFIL(id);
#else
	REG_DMAxSAD(id) = (u32)&value;
#endif
	REG_DMAxDAD(id) = (u32)dst;
	REG_DMAxCNT_L(id) = size/4;
	REG_DMAxCNT_H(id) =
		DMA_MODE_DST(DmaMode_Increment) |
		DMA_MODE_SRC(DmaMode_Fixed) |
		DMA_UNIT_32 |
		DMA_TIMING(DmaTiming_Immediate) |
		DMA_ENABLE;
}
