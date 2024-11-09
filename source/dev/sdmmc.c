// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#include <calico/types.h>
#include <calico/dev/tmio.h>
#include <calico/dev/sdmmc.h>

//#define SDMMC_DEBUG

#ifdef SDMMC_DEBUG
#include <calico/system/dietprint.h>
#else
#define dietPrint(...) ((void)0)
#endif

MK_CONSTEXPR u32 _sdmmcCalcNumSectors(TmioResp csd, bool ismmc)
{
	u32 c_size;
	int shift;

	if (!ismmc && (csd.value[3]>>30) == 1) {
		c_size = (csd.value[1]>>16) | ((csd.value[2]&0x3f)<<16);
		shift  = 10;
	} else {
		int block_len = (csd.value[2]>>16) & 0xF;
		int mult      = (csd.value[1]>>15) & 7;
		c_size = (csd.value[1]>>30) | ((csd.value[2]&0x3ff)<<2);
		shift  = block_len + (2+mult) - 9;
	}

	if_likely (shift >= 0) {
		return (c_size+1) << shift;
	} else {
		return (c_size+1) >> (-shift);
	}
}

MK_CONSTEXPR bool _sdmmcCheckSectorRange(u32 total_sectors, u32 sector_id, u32 num_sectors)
{
	return sector_id < total_sectors && num_sectors <= (total_sectors - sector_id);
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

static bool _sdmmcCardReadCsr(SdmmcCard* card, u32* out_csr)
{
	TmioTx tx;
	tx.callback = NULL;
	tx.xfer_isr = NULL;

	if (!_sdmmcTransact(card, &tx, SDMMC_CMD_GET_STATUS, card->rca << 16)) {
		*out_csr = 0;
		return false;
	}

	*out_csr = tx.resp.value[0];
	return true;
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

	card->ocr = tx.resp.value[0];
	dietPrint("Card OCR = %#.8lx\n", card->ocr);

	if (ismmc) {
		card->type = SdmmcType_MMC;
	} else if (!isv2) {
		card->type = SdmmcType_SDv1;
	} else if (!(card->ocr & (1U<<30))) {
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

	unsigned max_speed = tmioDecodeTranSpeed(card->csd.value[3] & 0xff);
	card->port.clock = TMIO_CLKCTL_AUTO | tmioSelectClock(max_speed);
	dietPrint("Speed: %u b/sec (0x%.2x)\n", max_speed, card->port.clock & 0xff);

	if (!_sdmmcTransact(card, &tx, SDMMC_CMD_SELECT_CARD, card->rca << 16)) {
		goto _error;
	}

	if (!_sdmmcTransact(card, &tx, SDMMC_CMD_SET_BLOCKLEN, SDMMC_SECTOR_SZ)) {
		goto _error;
	}

	const u32 scr_has_4bit = 1U<<(48-32+2);
	if (ismmc) {
		card->scr_hi = 0;

		if (((card->csd.value[3]>>26)&0xf) >= 4) {
			if (!_sdmmcTransact(card, &tx, SDMMC_CMD_MMC_SWITCH, SDMMC_CMD_MMC_SWITCH_ARG(3,183,1))) {
				goto _error;
			}

			card->port.width = 4;
			card->scr_hi |= scr_has_4bit;
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

		if (card->scr_hi & scr_has_4bit) {
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

bool sdmmcCardInitFromState(SdmmcCard* card, TmioCtl* ctl, SdmmcFrozenState const* state)
{
	*card = (SdmmcCard){0};

	// Fail early if the CID looks invalid
	if (!state->cid.value[0]) {
		dietPrint("Invalid sdmc state struct\n");
		goto _error;
	}

	// Retrieve card type from the state struct
	if_likely (state->is_mmc) {
		card->type = SdmmcType_MMC;
	} else if (state->is_sdhc) {
		card->type = SdmmcType_SDv2_SDHC;
	} else if (state->is_sd) {
		// XX: The state struct offers no way of telling apart SDv1 from SDv2_SDSC.
		// Assume non-SDHC cards are SDv1
		card->type = SdmmcType_SDv1;
	} else {
		dietPrint("Unknown sdmc card type\n");
		goto _error;
	}

	// Copy TMIO settings
	card->ctl = ctl;
	card->port.clock = TMIO_CLKCTL_AUTO | (state->tmio_clkctl & TMIO_CLKCTL_DIV(0xff));
	card->port.num = state->tmio_port & 1;
	card->port.width = (state->tmio_option & TMIO_OPTION_BUS_WIDTH1) ? 1 : 4;
	card->rca = state->rca;

	// Attempt to read the card's status register (CSR)
	u32 csr;
	if (!_sdmmcCardReadCsr(card, &csr)) {
		dietPrint("Could not read CSR\n");
		goto _error;
	}

	unsigned csr_state = (csr >> 9) & 0xf;
	dietPrint("CSR = %.8lX (state=%u)\n", csr, csr_state);

	// Ensure the card is in transfer state
	if (csr_state != 4) {
		dietPrint("Card not in transfer state\n");
		goto _error;
	}

	// Ensure the card doesn't report any errors
	if (csr & 0xf9ff0008) {
		dietPrint("CSR error flags are set\n");
		goto _error;
	}

	// XX: Official software deselects the card, reads CID, validates it against the struct,
	// and reselects the card. We decide instead to trust the information in the struct.

	card->cid = state->cid;
	card->csd.value[0] = (state->csd.value[0]<<8);
	card->csd.value[1] = (state->csd.value[1]<<8) | (state->csd.value[0]>>24);
	card->csd.value[2] = (state->csd.value[2]<<8) | (state->csd.value[1]>>24);
	card->csd.value[3] = (state->csd.value[3]<<8) | (state->csd.value[2]>>24);
	card->ocr = state->ocr;
	card->scr_hi = __builtin_bswap32(state->scr_hi_be);
	card->num_sectors = _sdmmcCalcNumSectors(card->csd, state->is_mmc);
	return true;

_error:
	card->type = SdmmcType_Invalid;
	return false;
}

void sdmmcCardDumpState(SdmmcCard* card, SdmmcFrozenState* state)
{
	state->cid = card->cid;
	state->csd.value[0] = (card->csd.value[0]>>8) | (card->csd.value[1]<<24);
	state->csd.value[1] = (card->csd.value[1]>>8) | (card->csd.value[2]<<24);
	state->csd.value[2] = (card->csd.value[2]>>8) | (card->csd.value[3]<<24);
	state->csd.value[3] = (card->csd.value[3]>>8);
	state->ocr = card->ocr;
	state->scr_hi_be = __builtin_bswap32(card->scr_hi);
	state->scr_lo_be = 0; // This is always zero/reserved
	state->rca = card->rca;
	state->is_mmc = card->type == SdmmcType_MMC;
	state->is_sdhc = card->type == SdmmcType_SDv2_SDHC;
	state->is_sd = card->type >= SdmmcType_SDv1;
	state->unknown = 0;
	_sdmmcCardReadCsr(card, &state->csr);
	state->tmio_clkctl = TMIO_CLKCTL_ENABLE | (card->port.clock & TMIO_CLKCTL_DIV(0xff));
	state->tmio_option = TMIO_OPTION_TIMEOUT(14) | TMIO_OPTION_NO_C2 |
		((card->port.width == 1) ? TMIO_OPTION_BUS_WIDTH1 : TMIO_OPTION_BUS_WIDTH4);
	state->is_ejected = 0;
	state->tmio_port = card->port.num;
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
