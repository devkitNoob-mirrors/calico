#include <calico/system/thread.h>
#include <calico/nds/arm7/spi.h>
#include <calico/nds/arm7/codec.h>

static CdcPage s_cdcCurPage = CdcPage_Dummy;

MEOW_INLINE bool _cdcCanUse(void)
{
	return cdcIsTwlMode() && mutexIsLockedByCurrentThread(&g_spiMutex);
}

MEOW_INLINE void _cdcSpiBegin(u8 cmd)
{
	spiRawStartHold(SpiDev_TSC, SpiBaud_4MHz);
	spiRawWriteByte(cmd);
}

MEOW_INLINE void _cdcSpiPreEnd(void)
{
	spiRawEndHold(SpiDev_TSC, SpiBaud_4MHz);
}

static u8 _cdcRawReadByte(unsigned reg)
{
	_cdcSpiBegin(1 | (reg<<1));
	_cdcSpiPreEnd();
	return spiRawReadByte();
}

static void _cdcRawWriteByte(unsigned reg, u8 data)
{
	_cdcSpiBegin(reg<<1);
	_cdcSpiPreEnd();
	spiRawWriteByteAsync(data);
}

static void _cdcSelectPage(CdcPage page)
{
	if (s_cdcCurPage != page) {
		_cdcRawWriteByte(s_cdcCurPage != CdcPage_DsMode ? 0 : 0x7f, page);
		s_cdcCurPage = page;
	}
}

u8 cdcReadReg(CdcPage page, unsigned reg)
{
	if (!_cdcCanUse()) {
		return 0xff;
	}

	_cdcSelectPage(page);
	return _cdcRawReadByte(reg);
}

bool cdcWriteReg(CdcPage page, unsigned reg, u8 value)
{
	if (!_cdcCanUse()) {
		return false;
	}

	_cdcSelectPage(page);
	_cdcRawWriteByte(reg, value);
	return true;
}

bool cdcWriteRegMask(CdcPage page, unsigned reg, u8 mask, u8 value)
{
	if (!_cdcCanUse()) {
		return false;
	}

	_cdcSelectPage(page);
	u8 old_value = _cdcRawReadByte(reg);
	_cdcRawWriteByte(reg, (old_value &~ mask) | (value & mask));
	return true;
}

bool cdcReadRegArray(CdcPage page, unsigned reg, void* data, unsigned len)
{
	if (!_cdcCanUse()) {
		return false;
	}

	if (!len) {
		return true;
	}

	_cdcSelectPage(page);
	_cdcSpiBegin(1 | (reg<<1));

	u8* data8 = (u8*)data;
	for (u32 i = 0; i < len - 1; i ++) {
		*data8++ = spiRawReadByte();
	}

	_cdcSpiPreEnd();
	*data8++ = spiRawReadByte();

	return true;
}

bool cdcWriteRegArray(CdcPage page, unsigned reg, const void* data, unsigned len)
{
	if (!_cdcCanUse()) {
		return false;
	}

	if (!len) {
		return true;
	}

	_cdcSelectPage(page);
	_cdcSpiBegin(reg<<1);

	const u8* data8 = (const u8*)data;
	for (u32 i = 0; i < len - 1; i ++) {
		spiRawWriteByte(*data8++);
	}

	_cdcSpiPreEnd();
	spiRawWriteByteAsync(*data8++);

	return true;
}

void cdcTscInit(void)
{
	// Disable buffer
	cdcWriteRegMask(CdcPage_TscControl, CdcTscCtrlReg_BufferMode,     0x80, 0U<<7);

	// Set resolution
	cdcWriteRegMask(CdcPage_TscControl, CdcTscCtrlReg_SarAdcCtrl,     0x18, 3U<<3); // SAR ADC CLK/8 ("recommended" for 12-bit resolution)

	// Set touch scan interval
	cdcWriteReg    (CdcPage_TscControl, CdcTscCtrlReg_ScanModeTimer,  0xa0);        // 2ms delay (+ bit7=autoscan)

	// Set touch panel data length (5)
	cdcWriteRegMask(CdcPage_TscControl, CdcTscCtrlReg_BufferMode,     0x38, 5U<<3); // XX: datasheet says this means 48 points

	// Set buffer mode
	cdcWriteRegMask(CdcPage_TscControl, CdcTscCtrlReg_BufferMode,     0x40, 0U<<6); // continuous conversion

	// Set conversion mode
	cdcWriteReg    (CdcPage_TscControl, CdcTscCtrlReg_SarAdcConvMode, 0x87);        // irq mode = ?? (3), conv mode = XY (1), bit7=selfconv

	// Set stabilization time
	cdcWriteRegMask(CdcPage_TscControl, CdcTscCtrlReg_PanelVoltStblz, 0x07, 4U<<0); // 30us

	// Set sense time
	cdcWriteRegMask(CdcPage_TscControl, CdcTscCtrlReg_PrechargeSense, 0x07, 6U<<0); // 300us

	// Set precharge time
	cdcWriteRegMask(CdcPage_TscControl, CdcTscCtrlReg_PrechargeSense, 0x70, 4U<<4); // 30us

	// Set debounce time
	cdcWriteRegMask(CdcPage_TscControl, CdcTscCtrlReg_DebounceTimer,  0x07, 0U<<0); // 0us

	// Enable buffer
	cdcWriteRegMask(CdcPage_TscControl, CdcTscCtrlReg_BufferMode,     0x80, 1U<<7);
}

bool cdcTscReadBuffer(CdcTscBuffer* out)
{
	// Fail early if pen touch is not detected
	if ((cdcReadReg(CdcPage_TscControl, CdcTscCtrlReg_Status0) & 0xc0) == 0x40) {
		return false;
	}

	// Fail early if the buffer is empty (uh, datasheet says bit1 means full???)
	if (cdcReadReg(CdcPage_TscControl, CdcTscCtrlReg_BufferMode) & 0x02) {
		return false;
	}

	// Read raw incoming data from TSC
	u8 raw[2*2*5];
	cdcReadRegArray(CdcPage_TscData, 0x01, raw, sizeof(raw));

	// Decode coordinates of all 5 touch points
	u16 arrayX[5], arrayY[5];
	for (unsigned i = 0; i < 5; i ++) {
		arrayX[i] = (raw[i*2+ 0]<<8) | raw[i*2+ 1];
		arrayY[i] = (raw[i*2+10]<<8) | raw[i*2+11];
		if ((arrayX[i] & 0xF000) || (arrayY[i] & 0xF000)) {
			// Pen-up was detected - return failure
			return false;
		}
	}

	// TODO: For now we just average all values without removing inaccurate values
	u32 sumX = 0, sumY = 0;
	for (unsigned i = 0; i < 5; i ++) {
		sumX += arrayX[i];
		sumY += arrayY[i];
	}

	// Output final data
	out->x = sumX / 5;
	out->y = sumY / 5;

	return true;
}
