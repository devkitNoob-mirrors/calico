#pragma once
#if !defined(__NDS__)
#error "This header file is only for NDS"
#endif

#include "../types.h"
#include "io.h"

#define NTRCARD_SECTOR_SZ 0x200

#define REG_NTRCARD_CNT       MK_REG(u16, IO_NTRCARD_CNT)
#define REG_NTRCARD_SPIDATA   MK_REG(u16, IO_NTRCARD_SPIDATA)

#define REG_NTRCARD_ROMCNT    MK_REG(u32, IO_NTRCARD_ROMCNT)
#define REG_NTRCARD_ROMCMD    MK_REG(u64, IO_NTRCARD_ROMCMD) // big endian!
#define REG_NTRCARD_ROMCMD_HI MK_REG(u32, IO_NTRCARD_ROMCMD+0)
#define REG_NTRCARD_ROMCMD_LO MK_REG(u32, IO_NTRCARD_ROMCMD+4)
#define REG_NTRCARD_SEED0_LO  MK_REG(u32, IO_NTRCARD_SEED0_LO)
#define REG_NTRCARD_SEED1_LO  MK_REG(u32, IO_NTRCARD_SEED1_LO)
#define REG_NTRCARD_SEED0_HI  MK_REG(u32, IO_NTRCARD_SEED0_HI)
#define REG_NTRCARD_SEED1_HI  MK_REG(u32, IO_NTRCARD_SEED1_HI)
#define REG_NTRCARD_FIFO      MK_REG(u32, IO_NTRCARD_FIFO)

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

MK_EXTERN_C_START

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

typedef enum NtrCardMode {
	NtrCardMode_None   = 0,
	NtrCardMode_Init   = 1,
	NtrCardMode_Secure = 2,
	NtrCardMode_Main   = 3,
} NtrCardMode;

typedef enum NtrCardCmd {
	// Initialization mode commands (unencrypted)
	NtrCardCmd_Init               = 0x9f,
	NtrCardCmd_InitGetChipId      = 0x90,
	NtrCardCmd_InitRomRead        = 0x00,
	NtrCardCmd_InitEnterSecureNtr = 0x3c,
	NtrCardCmd_InitEnterSecureTwl = 0x3d,

	// Secure mode commands (encrypted with Blowfish aka "KEY1")
	NtrCardCmd_SecurePngOn        = 0x40,
	NtrCardCmd_SecurePngOff       = 0x60,
	NtrCardCmd_SecureGetChipId    = 0x10,
	NtrCardCmd_SecureReadBlock    = 0x20,
	NtrCardCmd_SecureEnterMain    = 0xa0,

	// Main mode commands (encrypted with PNG aka "KEY2")
	NtrCardCmd_MainGetChipId      = 0xb8,
	NtrCardCmd_MainRomRead        = 0xb7,
	NtrCardCmd_MainGetStatus      = 0xd6,
	NtrCardCmd_MainRomRefresh     = 0xb5,
} NtrCardCmd;

typedef union NtrChipId {
	u32 raw;
	struct {
		u32 manuf       : 8;

		u32 chip_size   : 8;

		u32 has_ir      : 1;
		u32 unk17       : 1;
		u32 _pad18      : 5;
		u32 unk23       : 1;

		u32 _pad24      : 3;
		u32 is_nand     : 1;
		u32 is_ctr      : 1;
		u32 has_refresh : 1;
		u32 is_twl      : 1;
		u32 is_1trom    : 1;
	};
} NtrChipId;

MK_CONSTEXPR u32 ntrcardCalcChipSize(NtrChipId id)
{
	unsigned val = id.chip_size;
	if (val < 0xf0) {
		return 0x100000 * (val+1);
	} else {
		return 0x10000000 * (0x100-val);
	}
}

bool ntrcardOpen(void);
void ntrcardClose(void);

NtrCardMode ntrcardGetMode(void);

bool ntrcardGetChipId(NtrChipId* out);

bool ntrcardRomReadSector(int dma_ch, u32 offset, void* buf);
bool ntrcardRomRead(int dma_ch, u32 offset, void* buf, u32 size);

MK_EXTERN_C_END
