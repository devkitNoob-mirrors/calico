#pragma once
#if !defined(__NDS__)
#error "This header file is only for NDS"
#endif

#include "../types.h"
#include "../arm/common.h"
#include "io.h"

#define REG_IME MK_REG(u32, IO_IME)
#define REG_IE  MK_REG(u32, IO_IE)
#define REG_IF  MK_REG(u32, IO_IF)
#ifdef ARM7
#define REG_IE2 MK_REG(u32, IO_IE2)
#define REG_IF2 MK_REG(u32, IO_IF2)
#endif

#define IRQ_VBLANK       (1U << 0)
#define IRQ_HBLANK       (1U << 1)
#define IRQ_VCOUNT       (1U << 2)
#define IRQ_TIMER0       (1U << 3)
#define IRQ_TIMER1       (1U << 4)
#define IRQ_TIMER2       (1U << 5)
#define IRQ_TIMER3       (1U << 6)
#define IRQ_DMA0         (1U << 8)
#define IRQ_DMA1         (1U << 9)
#define IRQ_DMA2         (1U << 10)
#define IRQ_DMA3         (1U << 11)
#define IRQ_KEYPAD       (1U << 12)
#define IRQ_SLOT2        (1U << 13)
#define IRQ_PXI_SYNC     (1U << 16)
#define IRQ_PXI_SEND     (1U << 17)
#define IRQ_PXI_RECV     (1U << 18)
#define IRQ_SLOT1_TX     (1U << 19)
#define IRQ_SLOT1_IREQ   (1U << 20)
#define IRQ_NDMA0        (1U << 28)
#define IRQ_NDMA1        (1U << 29)
#define IRQ_NDMA2        (1U << 30)
#define IRQ_NDMA3        (1U << 31)

#define IRQ_TIMER(_x)    (1U << ( 3+(_x)))
#define IRQ_DMA(_x)      (1U << ( 8+(_x)))
#define IRQ_NDMA(_x)     (1U << (28+(_x)))

#ifdef ARM7
#define MK_IRQ_NUM_HANDLERS 64

#define IRQ_RTC          (1U << 7)
#define IRQ_HINGE        (1U << 22)
#define IRQ_SPI          (1U << 23)
#define IRQ_WIFI         (1U << 24)

#define IRQ2_HEADPHONE   (1U << 5)
#define IRQ2_MCU         (1U << 6)
#define IRQ2_TMIO0       (1U << 8)
#define IRQ2_TMIO0_SDIO  (1U << 9)
#define IRQ2_TMIO1       (1U << 10)
#define IRQ2_TMIO1_SDIO  (1U << 11)
#define IRQ2_AES         (1U << 12)
#define IRQ2_I2C         (1U << 13)
#define IRQ2_MICEX       (1U << 14)
#endif

#ifdef ARM9
#define MK_IRQ_NUM_HANDLERS 32

#define IRQ_SLOT1_SWAP   (1U << 14)
#define IRQ_3DFIFO       (1U << 21)
#define IRQ_DSP          (1U << 24)
#define IRQ_CAM          (1U << 25)
#endif

typedef unsigned IrqState;
typedef u32 IrqMask;

MK_INLINE IrqState irqLock(void)
{
	armCompilerBarrier();
	IrqState saved = REG_IME;
	REG_IME = 0;
	armCompilerBarrier();
	return saved;
}

MK_INLINE void irqUnlock(IrqState state)
{
	armCompilerBarrier();
	REG_IME = state;
	armCompilerBarrier();
}
