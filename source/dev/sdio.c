#include <calico/types.h>
#include <calico/dev/tmio.h>
#include <calico/dev/sdio.h>

//#define SDIO_DEBUG

#ifdef SDIO_DEBUG
#include <calico/system/dietprint.h>
#else
#define dietPrint(...) ((void)0)
#endif

static bool _sdioTransact(SdioCard* card, TmioTx* tx, u16 type, u32 arg)
{
	tx->port = card->port;
	tx->type = type;
	tx->arg = arg;
	return tmioTransact(card->ctl, tx);
}

static bool _sdioCardBootup(SdioCard* card)
{
	TmioTx tx;
	tx.callback = NULL;
	tx.xfer_isr = NULL;

	// XX: GO_IDLE is not intended to be necessary for SDIO cards operating in
	// SD mode (as opposed to SPI mode). Yet, the card does not reply to commands
	// (incl. SEND_OP_COND) if this is not done first after a "reset".
	// TODO: investigate
	if (!_sdioTransact(card, &tx, SDIO_CMD_GO_IDLE, 0)) {
		return false;
	}

	// Read the OCR
	if (!_sdioTransact(card, &tx, SDIO_CMD_SEND_OP_COND, 0)) {
		return false;
	}
	dietPrint("SDIO OCR = %.8lX\n", tx.resp.value[0]);

	// Send our supported voltage, and wait for the card to be ready
	do {
		if (!_sdioTransact(card, &tx, SDIO_CMD_SEND_OP_COND, 1U<<20)) {
			return false;
		}
		threadSleep(10000); // 10ms
	} while (!(tx.resp.value[0] & (1U<<31)));

	// Retrieve card RCA
	if (!_sdioTransact(card, &tx, SDIO_CMD_GET_RELATIVE_ADDR, 0)) {
		return false;
	}
	card->rca = tx.resp.value[0] >> 16;
	dietPrint("SDIO RCA = %.4X\n", card->rca);

	// Select the card
	if (!_sdioTransact(card, &tx, SDIO_CMD_SELECT_CARD, card->rca << 16)) {
		return false;
	}

	return true;
}

bool sdioCardInit(SdioCard* card, TmioCtl* ctl, unsigned port)
{
	*card = (SdioCard){0};
	card->ctl = ctl;
	card->port.clock = tmioSelectClock(400000); // 400kHz is the standard identification clock
	// XX: Official code uses HCLK/256 aka 131kHz here. Figure out which clock should we use
	card->port.num = port;
	card->port.width = 1;

	// Try to read a SDIO register (Int Enable), and if that fails, try to perform initial bootup.
	u8 dummy = 0;
	if (!sdioCardReadDirect(card, 0, 0x00004, &dummy, 1) && !_sdioCardBootup(card)) {
		return false;
	}

	// Retrieve CCCR/SDIO revision
	if (!sdioCardReadDirect(card, 0, 0x00000, &card->revision, 1)) {
		return false;
	}
	dietPrint("CCCR/SDIO rev: 0x%.2X\n", card->revision);

	// Enable moar power on SDIO 1.1+
	if ((card->revision & 0xf0) >= 0x10) do {
		u8 power_ctl = 0;
		if (!sdioCardReadDirect(card, 0, 0x00012, &power_ctl, 1)) {
			break;
		}
		if (!(power_ctl & 0x01)) {
			break;
		}
		power_ctl = 0x02;
		if (!sdioCardWriteDirect(card, 0, 0x00012, &power_ctl, 1)) {
			return false;
		}
	} while (0);

	// Retrieve capabilities
	if (!sdioCardReadDirect(card, 0, 0x00008, &card->caps, 1)) {
		return false;
	}
	dietPrint("SDIO caps: 0x%.2X\n", card->caps);

	// Bus interface control
	{
		// Disable card detect pull-up resistor
		u8 bus_iface_ctl = 0x80;

		// Switch to 4-bit mode if supported (i.e. not a slow card, or a slow card
		// that supports 4-bit mode)
		bool can_do_4bit = (card->caps & 0xc0) != 0x40;
		if (can_do_4bit) {
			bus_iface_ctl |= 2;
		}

		if (!sdioCardWriteDirect(card, 0, 0x00007, &bus_iface_ctl, 1)) {
			return false;
		}

		if (can_do_4bit) {
			card->port.width = 4;
		}
	}

	// Retrieve the pointer to CIS0
	u32 cis0_ptr = 0;
	if (!sdioCardReadDirect(card, 0, 0x00009, &cis0_ptr, 3)) {
		return false;
	}
	cis0_ptr &= 0xffffff;
	dietPrint("CIS0 at 0x%.6lX\n", cis0_ptr);

	// Process CIS0 tuples
	u32 cis0_tuple_mask = 0;
	while (cis0_tuple_mask != 3) {
		struct { u8 id, len; } cis_tuple;
		if (!sdioCardReadDirect(card, 0, cis0_ptr, &cis_tuple, sizeof(cis_tuple))) {
			return false;
		}

		cis0_ptr += sizeof(cis_tuple);

		if (cis_tuple.id == 0xff) {
			break;
		}

		switch (cis_tuple.id) {
			default: break;
			case 0x20: { // MANFID
				if (!sdioCardReadDirect(card, 0, cis0_ptr, &card->manfid, sizeof(card->manfid))) {
					return false;
				}
				dietPrint(" MANFID code=%.4X id=%.4X\n", card->manfid.code, card->manfid.id);
				cis0_tuple_mask |= 1U<<0;
				break;
			}
			case 0x22: { // FUNCE
				struct { u8 type; u8 blk_size[2]; u8 max_tran_speed; } funce;
				if (!sdioCardReadDirect(card, 0, cis0_ptr, &funce, sizeof(funce))) {
					return false;
				}
				if (funce.type != 0x00) { // We are only interested in Function 0
					break;
				}

				unsigned max_speed = tmioDecodeTranSpeed(funce.max_tran_speed);
				card->port.clock = tmioSelectClock(max_speed);
				dietPrint(" Speed: %u b/sec (0x%.2x)\n", max_speed, card->port.clock & 0xff);
				cis0_tuple_mask |= 1U<<1;
				break;
			}
		}

		cis0_ptr += cis_tuple.len;
	}

	if (cis0_tuple_mask != 3) {
		dietPrint("SDIO missing CIS0 tuples\n");
		return false;
	}

	// Set block size for function0
	u16 blk_size = SDIO_BLOCK_SZ;
	if (!sdioCardWriteDirect(card, 0, 0x00010, &blk_size, 2)) {
		return false;
	}

	//-------------------------------------------------------------------------
	// Below code assumes we want to initialize function1. Technically speaking
	// this would be handled elsewhere (enumerating/probing SDIO functions),
	// however for simplicity's sake, and taking into account the intended use
	// case (i.e. Atheros wireless), we will do this here anyway.

	// Set block size for function1
	if (!sdioCardWriteDirect(card, 0, 0x00110, &blk_size, 2)) {
		return false;
	}

	// Enable function1
	u8 func_mask = 1U << 1;
	if (!sdioCardWriteDirect(card, 0, 0x00002, &func_mask, 1)) {
		return false;
	}

	// Wait for function1 to be ready
	bool func_enabled = false;
	do {
		u8 ready = 0;
		if (!sdioCardReadDirect(card, 0, 0x00003, &ready, 1)) {
			return false;
		}
		func_enabled = (ready & func_mask) == func_mask;
		if (!func_enabled) {
			threadSleep(1000);
		}
	} while (!func_enabled);

	dietPrint("SDIO enabled func1!\n");

	return true;
}

bool sdioCardReadDirect(SdioCard* card, unsigned func, unsigned addr, void* out, size_t size)
{
	TmioTx tx;
	tx.callback = NULL;
	tx.xfer_isr = NULL;

	u8* buf = (u8*)out;
	for (size_t i = 0; i < size; i ++) {
		u32 arg = SDIO_RW_DIRECT_ADDR(addr+i) | SDIO_RW_DIRECT_FUNC(func);
		if (!_sdioTransact(card, &tx, SDIO_CMD_RW_DIRECT, arg)) {
			return false;
		}
		u32 ret = tx.resp.value[0];
		//dietPrint(" sdio ret %.8lX\n", ret);
		if ((ret >> 8) & 0xcf) {
			return false;
		}
		buf[i] = ret & 0xff;
	}
	return true;
}

bool sdioCardWriteDirect(SdioCard* card, unsigned func, unsigned addr, const void* in, size_t size)
{
	TmioTx tx;
	tx.callback = NULL;
	tx.xfer_isr = NULL;

	const u8* buf = (const u8*)in;
	for (size_t i = 0; i < size; i ++) {
		u32 arg = SDIO_RW_DIRECT_DATA(buf[i]) | SDIO_RW_DIRECT_ADDR(addr+i) | SDIO_RW_DIRECT_FUNC(func) | SDIO_RW_DIRECT_WRITE;
		if (!_sdioTransact(card, &tx, SDIO_CMD_RW_DIRECT, arg)) {
			return false;
		}
		u32 ret = tx.resp.value[0];
		if ((ret >> 8) & 0xcf) {
			return false;
		}
	}
	return true;
}
