#include "common.h"

static bool _ar6kDevReset(Ar6kDev* dev)
{
	// Soft reset the device
	if (!ar6kDevWriteRegDiag(dev, 0x00004000, 0x00000100)) {
		return false;
	}
	threadSleep(50000); // 50ms

	// Check reset cause
	u32 reset_cause = 0;
	if (!ar6kDevReadRegDiag(dev, 0x000040C0, &reset_cause)) {
		return false;
	}
	if ((reset_cause & 7) != 2) {
		return false;
	}

	return true;
}

bool ar6kDevInit(Ar6kDev* dev, SdioCard* sdio)
{
	*dev = (Ar6kDev){0};
	dev->sdio = sdio;

	if (sdio->manfid.code != 0x0271) {
		dietPrint("[AR6K] Bad SDIO manufacturer\n");
		return false;
	}

	// In case we are warmbooting the card and IRQs were enabled previously, disable them
	{
		u32 value = 0;
		if (!sdioCardWriteExtended(dev->sdio, 1, 0x000418, &value, 4)) {
			dietPrint("[AR6K] Unable to disable irqs\n");
			return false;
		}
	}

	// Read chip ID
	if (!ar6kDevReadRegDiag(dev, 0x000040ec, &dev->chip_id)) {
		dietPrint("[AR6K] Cannot read chip ID\n");
		return false;
	}
	dietPrint("[AR6K] Chip ID = %.8lX\n", dev->chip_id);

	// Autodetect address of host interest area. Normally this would be retrieved
	// directly from a parameter in MM_ENV_TWL_WLFIRM_INFO, but this data may not
	// always be available. Instead, we will detect it based on chip identification.
	switch (sdio->manfid.id) {
		case 0x0200: // AR6002
			dev->hia_addr = 0x500400;
			break;
		case 0x0201: // AR6013/AR6014
			dev->hia_addr = 0x520000;
			break;
		default:
			dietPrint("[AR6K] Bad SDIO device ID\n");
			return false;
	}

	// Read board initialized flag from HIA
	u32 hi_board_data_initialized = 0;
	if (!ar6kDevReadRegDiag(dev, dev->hia_addr+0x58, &hi_board_data_initialized)) {
		return false;
	}

	if (!hi_board_data_initialized) {
		// Board is not initialized, we are at BMI and we must upload Atheros firmware
		dietPrint("[AR6K] [!] Firmware not loaded\n");
	} else {
		// Board is initialized, we must soft-reset into BMI (keeping firmware uploaded)
		dietPrint("[AR6K] Soft resetting device\n");

		if (!_ar6kDevReset(dev)) {
			dietPrint("[AR6K] Failed to reset device\n");
			return false;
		}
	}

	// Read target info
	Ar6kBmiTargetInfo tgtinfo;
	if (!ar6kBmiGetTargetInfo(dev, &tgtinfo)) {
		dietPrint("[AR6K] Cannot read target info\n");
		return false;
	}
	dietPrint("[AR6K] BMI %.8lX (type %lu)\n", tgtinfo.target_ver, tgtinfo.target_type);

	// XX: Here the target version is compared against 0x20000188, and if it is lower
	// than said value (and the fw is not loaded yet), a specific sequence of register
	// writes takes place (matches ar6002_REV1_reset_force_host). This does not seem
	// necessary, as the oldest chip used on DSi/3DS (AR6002G) will return 0x20000188.

	// XX: enableuartprint would happen here. This is disabled in official code, and
	// is useless to us anyway (we don't actually have the means to connect UART to
	// the Atheros hardware, nor do we probably have a debugging build of the FW).

	// TODO: more

	// TODO: HTC, WMI bringup

	dietPrint("[AR6K] done\n");

	return true;
}

static bool _ar6kDevSetAddrWinReg(Ar6kDev* dev, u32 reg, u32 addr)
{
	// Write bytes 1,2,3 first
	if (!sdioCardWriteExtended(dev->sdio, 1, reg+1, (u8*)&addr+1, 3)) {
		return false;
	}

	// Write LSB last to initiate the access cycle
	if (!sdioCardWriteExtended(dev->sdio, 1, reg, (u8*)&addr, 1)) {
		return false;
	}

	return true;
}

bool ar6kDevReadRegDiag(Ar6kDev* dev, u32 addr, u32* out)
{
	if (!_ar6kDevSetAddrWinReg(dev, 0x0047C, addr)) { // WINDOW_READ_ADDR
		return false;
	}

	if (!sdioCardReadExtended(dev->sdio, 1, 0x00474, out, 4)) { // WINDOW_DATA
		return false;
	}

	return true;
}

bool ar6kDevWriteRegDiag(Ar6kDev* dev, u32 addr, u32 value)
{
	if (!sdioCardWriteExtended(dev->sdio, 1, 0x00474, &value, 4)) { // WINDOW_DATA
		return false;
	}

	if (!_ar6kDevSetAddrWinReg(dev, 0x00478, addr)) { // WINDOW_WRITE_ADDR
		return false;
	}

	return true;
}
