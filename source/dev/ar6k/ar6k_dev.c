#include "common.h"

bool ar6kDevInit(Ar6kDev* dev, SdioCard* sdio)
{
	*dev = (Ar6kDev){0};
	dev->sdio = sdio;

	if (sdio->manfid.code != 0x0271) {
		dietPrint("[AR6K] Bad SDIO manufacturer\n");
		return false;
	}

	// Autodetect host interest area
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

	u32 hi_board_data_initialized = 0;
	if (!ar6kDevReadRegDiag(dev, dev->hia_addr+0x58, &hi_board_data_initialized)) {
		return false;
	}

	if (!hi_board_data_initialized) {
		dietPrint("[AR6K] [!] Firmware not loaded\n");
		//return false;
	}

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
		dietPrint("[AR6K] Unable to reset device\n");
		return false;
	}

	// TODO: BMI, HTC, WMI bringup

	if (!hi_board_data_initialized) {
		dietPrint("[AR6K] Fail, due to missing fw\n");
		return false;
	}

	dietPrint("[AR6K] Dev init success!\n");
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
