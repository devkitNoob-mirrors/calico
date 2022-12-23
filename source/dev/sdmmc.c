#include <calico/types.h>
#include <calico/dev/tmio.h>
#include <calico/dev/sdmmc.h>

#define SDMMC_DEBUG

#ifdef SDMMC_DEBUG
#include <calico/system/dietprint.h>
#else
#define dietPrint(...) ((void)0)
#endif

MEOW_CONSTEXPR u32 _sdmmcGetMaxSpeed(u8 tran_speed)
{
	// https://android.googlesource.com/device/google/accessory/adk2012/+/e0f114ab1d645caeac2c30273d0b693d72063f54/MakefileBasedBuild/Atmel/sam3x/sam3x-ek/libraries/memories/sdmmc/sdmmc.c#212
	static const u16 units[] = { 10, 100, 1000, 10000 }; // in 10Kb/s units
	static const u8 multipliers[] = { 0, 10, 12, 13, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80 }; // in 1/10 units
	u8 unit = tran_speed & 7;
	u8 multiplier = (tran_speed >> 3) & 0xF;
	if (unit < 4) {
		return 1000*units[unit]*multipliers[multiplier];
	}

	return 0;
}

MEOW_CONSTEXPR u32 _sdmmcCalcNumSectors(TmioResp csd, bool ismmc)
{
	u32 c_size, c_blk;
	if (!ismmc && (csd.value[3]>>30) == 1) {
		c_size = (csd.value[1]>>16) | ((csd.value[2]&0x3f)<<16);
		c_blk  = 1024*512/SDMMC_SECTOR_SZ;
	} else {
		int block_len = (csd.value[2]>>16) & 0xF;
		int mult      = (csd.value[1]>>15) & 7;
		c_size = (csd.value[1]>>30) | ((csd.value[2]&0x3ff)<<2);
		c_blk  = (1U<<(block_len+mult+2))/SDMMC_SECTOR_SZ;
	}

	return (c_size+1) * c_blk;
}

MEOW_CONSTEXPR bool _sdmmcCheckSectorRange(u32 total_sectors, u32 sector_id, u32 num_sectors)
{
	u32 end_sector = sector_id+num_sectors-1;
	return sector_id<=end_sector && end_sector<total_sectors;
}

static bool _sdmmcTransact(SdmmcCard* card, TmioTx* tx, u16 type, u32 arg)
{
	tx->port = card->port;
	if (type & TMIO_CMD_TYPE_ACMD) {
		tx->type = SDMMC_CMD_APP_CMD;
		tx->arg = card->rca << 16;
		if (!tmioTransact(card->ctl, tx)) {
			return false;
		}
	}

	tx->type = type;
	tx->arg = arg;
	return tmioTransact(card->ctl, tx);
}

bool sdmmcCardInit(SdmmcCard* card, TmioCtl* ctl, unsigned port, bool ismmc)
{
	*card = (SdmmcCard){0};
	card->ctl = ctl;
	card->port.clock = tmioSelectClock(400000); // 400kHz is the standard identification clock
	card->port.num = port;
	card->port.width = 1;

	bool isv2 = false;
	u32 ocr_arg = (1U<<20);

	TmioTx tx;
	tx.callback = NULL;
	tx.xfer_isr = tmioXferRecvByCpu;

	if (!_sdmmcTransact(card, &tx, SDMMC_CMD_GO_IDLE, 0)) {
		goto _error;
	}

	if (!ismmc && _sdmmcTransact(card, &tx, SDMMC_CMD_SET_IF_COND, 0x1aa)) {
		if ((tx.resp.value[0] & 0xfff) != 0x1aa) {
			dietPrint("SET_IF_COND fail: %#.3lx\n", tx.resp.value[0] & 0xfff);
			goto _error;
		} else {
			isv2 = true;
		}
	}

	if (isv2) {
		ocr_arg |= (1U<<30);
	}

	do {
		if (ismmc) {
			if (!_sdmmcTransact(card, &tx, SDMMC_CMD_SEND_OP_COND, ocr_arg)) {
				goto _error;
			}
		} else if (!_sdmmcTransact(card, &tx, SDMMC_ACMD_SEND_OP_COND, ocr_arg)) {
			if (isv2) {
				goto _error;
			}

			ismmc = true;
			tx.resp.value[0] = 0;
		}
	} while (!(tx.resp.value[0] & (1U<<31)));

	dietPrint("Card OCR = %#.8lx\n", tx.resp.value[0]);

	if (ismmc) {
		card->type = SdmmcType_MMC;
	} else if (!isv2) {
		card->type = SdmmcType_SDv1;
	} else if (!(tx.resp.value[0] & (1U<<30))) {
		card->type = SdmmcType_SDv2_SDSC;
	} else {
		card->type = SdmmcType_SDv2_SDHC;
	}

#ifdef SDMMC_DEBUG
	static const char* const cardtypestr[] = {
		"Invalid",
		"MMC",
		"SDv1",
		"SDv2 (SDSC)",
		"SDv2 (SDHC)",
	};
	dietPrint("Card is %s\n", cardtypestr[card->type]);
#endif

	if (!_sdmmcTransact(card, &tx, SDMMC_CMD_ALL_GET_CID, 0)) {
		goto _error;
	}

	card->cid = tx.resp;
	dietPrint("CID: %.8lX %.8lX\n     %.8lX %.8lX\n", card->cid.value[0], card->cid.value[1], card->cid.value[2], card->cid.value[3]);

	if (ismmc) {
		card->rca = 1;
		if (!_sdmmcTransact(card, &tx, SDMMC_CMD_MMC_SET_RELATIVE_ADDR, card->rca << 16)) {
			goto _error;
		}
	} else {
		if (!_sdmmcTransact(card, &tx, SDMMC_CMD_SD_GET_RELATIVE_ADDR, 0)) {
			goto _error;
		}
		card->rca = tx.resp.value[0] >> 16;
	}

	dietPrint("RCA = 0x%.4x\n", card->rca);

	if (!_sdmmcTransact(card, &tx, SDMMC_CMD_GET_CSD, card->rca << 16)) {
		goto _error;
	}

	card->csd = tx.resp;
	card->num_sectors = _sdmmcCalcNumSectors(card->csd, ismmc);
	dietPrint("CSD: %.8lX %.8lX\n     %.8lX %.8lX\n", card->csd.value[0], card->csd.value[1], card->csd.value[2], card->csd.value[3]);
	dietPrint("Num sectors: %#lx\n", card->num_sectors);

	u32 max_speed = _sdmmcGetMaxSpeed(card->csd.value[3] & 0xff);
	card->port.clock = TMIO_CLKCTL_AUTO | tmioSelectClock(max_speed);
	dietPrint("Speed: %lu b/sec (0x%.2x)\n", max_speed, card->port.clock & 0xff);

	if (!_sdmmcTransact(card, &tx, SDMMC_CMD_SELECT_CARD, card->rca << 16)) {
		goto _error;
	}

	if (!_sdmmcTransact(card, &tx, SDMMC_CMD_SET_BLOCKLEN, SDMMC_SECTOR_SZ)) {
		goto _error;
	}

	if (ismmc) {
		if (((card->csd.value[3]>>26)&0xf) >= 4) {
			if (!_sdmmcTransact(card, &tx, SDMMC_CMD_MMC_SWITCH, SDMMC_CMD_MMC_SWITCH_ARG(3,183,1))) {
				goto _error;
			}

			card->port.width = 4;
		}
	} else {
		if (!_sdmmcTransact(card, &tx, SDMMC_ACMD_SET_CLR_CARD_DETECT, 0)) {
			goto _error;
		}

		if (!_sdmmcTransact(card, &tx, SDMMC_ACMD_SET_BUS_WIDTH, 0)) {
			goto _error;
		}

		u32 scr[2];
		tx.user = scr;
		tx.block_size = sizeof(scr);
		tx.num_blocks = 1;
		if (!_sdmmcTransact(card, &tx, SDMMC_ACMD_GET_SCR, 0)) {
			goto _error;
		}

		card->scr_hi = __builtin_bswap32(scr[0]);
		dietPrint("SCR: %.8lX\n", card->scr_hi);

		if (card->scr_hi & (1U<<(48-32+2))) {
			if (!_sdmmcTransact(card, &tx, SDMMC_ACMD_SET_BUS_WIDTH, 2)) {
				goto _error;
			}

			card->port.width = 4;
		}
	}

	if (card->port.width == 4) {
		dietPrint("Switched to 4-bit width\n");
	}

	return true;

_error:
	dietPrint("SDMC init error %.8lX\n", tx.status);
	card->type = SdmmcType_Invalid;
	return false;
}

static bool _sdmmcCardReadWriteSectors(SdmmcCard* card, TmioTx* tx, u32 sector_id, u32 num_sectors, u16 type)
{
	if (card->type == SdmmcType_Invalid || !num_sectors || !_sdmmcCheckSectorRange(card->num_sectors, sector_id, num_sectors)) {
		tx->status = TMIO_STAT_ILL_ACCESS;
		return false;
	}

	if (card->type < SdmmcType_SDv2_SDHC) {
		sector_id *= SDMMC_SECTOR_SZ;
	}

	tx->block_size = SDMMC_SECTOR_SZ;
	tx->num_blocks = num_sectors;
	return _sdmmcTransact(card, tx, type, sector_id);
}

bool sdmmcCardReadSectors(SdmmcCard* card, TmioTx* tx, u32 sector_id, u32 num_sectors)
{
	return _sdmmcCardReadWriteSectors(card, tx, sector_id, num_sectors, SDMMC_CMD_READ_MULTIPLE_BLOCK);
}

bool sdmmcCardWriteSectors(SdmmcCard* card, TmioTx* tx, u32 sector_id, u32 num_sectors)
{
	return _sdmmcCardReadWriteSectors(card, tx, sector_id, num_sectors, SDMMC_CMD_WRITE_MULTIPLE_BLOCK);
}
