#pragma once
#if !defined(__NDS__)
#error "This header file is only for NDS"
#endif

#include "../types.h"
#include "io.h"

#define NTRCARD_SECTOR_SZ 0x200

#define REG_NTRCARD_CNT       MEOW_REG(u16, IO_NTRCARD_CNT)
#define REG_NTRCARD_SPIDATA   MEOW_REG(u16, IO_NTRCARD_SPIDATA)

#define REG_NTRCARD_ROMCNT    MEOW_REG(u32, IO_NTRCARD_ROMCNT)
#define REG_NTRCARD_ROMCMD    MEOW_REG(u64, IO_NTRCARD_ROMCMD) // big endian!
#define REG_NTRCARD_ROMCMD_HI MEOW_REG(u32, IO_NTRCARD_ROMCMD+0)
#define REG_NTRCARD_ROMCMD_LO MEOW_REG(u32, IO_NTRCARD_ROMCMD+4)
#define REG_NTRCARD_SEED0_LO  MEOW_REG(u32, IO_NTRCARD_SEED0_LO)
#define REG_NTRCARD_SEED1_LO  MEOW_REG(u32, IO_NTRCARD_SEED1_LO)
#define REG_NTRCARD_SEED0_HI  MEOW_REG(u32, IO_NTRCARD_SEED0_HI)
#define REG_NTRCARD_SEED1_HI  MEOW_REG(u32, IO_NTRCARD_SEED1_HI)
#define REG_NTRCARD_FIFO      MEOW_REG(u32, IO_NTRCARD_FIFO)

#define NTRCARD_CNT_SPI_BAUD(_n) ((_n)&3)
#define NTRCARD_CNT_SPI_HOLD     (1U<<6)
#define NTRCARD_CNT_SPI_BUSY     (1U<<7)
#define NTRCARD_CNT_MODE_ROM     (0U<<13)
#define NTRCARD_CNT_MODE_SPI     (1U<<13)
#define NTRCARD_CNT_TX_IE        (1U<<14)
#define NTRCARD_CNT_ENABLE       (1U<<15)

#define NTRCARD_ROMCNT_GAP1_LEN(_n) ((_n)&0x1fff)
#define NTRCARD_ROMCNT_ENCR_DATA    (1U<<13)
#define NTRCARD_ROMCNT_ENCR_ENABLE  (1U<<14)
#define NTRCARD_ROMCNT_SEED_APPLY   (1U<<15)
#define NTRCARD_ROMCNT_GAP2_LEN(_n) (((_n)&0x3f)<<16)
#define NTRCARD_ROMCNT_ENCR_CMD     (1U<<22)
#define NTRCARD_ROMCNT_DATA_READY   (1U<<23)
#define NTRCARD_ROMCNT_BLK_SIZE(n)  (((n)&7)<<24)
#define NTRCARD_ROMCNT_CLK_DIV_5    (0U<<27)
#define NTRCARD_ROMCNT_CLK_DIV_8    (1U<<27)
#define NTRCARD_ROMCNT_CLK_GAP_ON   (1U<<28)
#define NTRCARD_ROMCNT_NO_RESET     (1U<<29)
#define NTRCARD_ROMCNT_WRITE        (1U<<30)
#define NTRCARD_ROMCNT_START        (1U<<31)
#define NTRCARD_ROMCNT_BUSY         NTRCARD_ROMCNT_START

MEOW_EXTERN_C_START

typedef enum NtrCardSpiBaud {
	NtrCardSpiBaud_4MHz = 0,
	NtrCardSpiBaud_2MHz,
	NtrCardSpiBaud_1MHz,
	NtrCardSpiBaud_512KHz,
} NtrCardSpiBaud;

typedef enum NtrCardBlkSize {
	NtrCardBlkSize_0      = 0,
	NtrCardBlkSize_0x200  = 1,
	NtrCardBlkSize_0x400  = 2,
	NtrCardBlkSize_0x800  = 3,
	NtrCardBlkSize_0x1000 = 4,
	NtrCardBlkSize_0x2000 = 5,
	NtrCardBlkSize_0x4000 = 6,
	NtrCardBlkSize_4      = 7,

	NtrCardBlkSize_Sector = NtrCardBlkSize_0x200,
} NtrCardBlkSize;

bool ntrcardOpen(void);
void ntrcardClose(void);

bool ntrcardRomReadSector(int dma_ch, u32 offset, void* buf);
bool ntrcardRomRead(int dma_ch, u32 offset, void* buf, u32 size);

MEOW_EXTERN_C_END
