#include <calico/nds/system.h>
#include <calico/nds/bios.h>
#include <calico/nds/arm7/gpio.h>
#include <calico/nds/arm7/rtc.h>

#define RTC_CMD(_reg,_isread) (0x60 | (((_reg)&0xf)<<1) | ((_isread)&1))

MEOW_INLINE void _rtcSleepUsec(int usec)
{
	// This is probably eyeballed (not by me) and a lot slower than what's necessary.
	// TODO: replace with a more accurate (and inline-asm) cycle count busyloop.
	svcWaitByLoop(usec<<3);
}

MEOW_INLINE void _rtcBegin(void)
{
	REG_RCNT_RTC = RCNT_RTC_CS_0 | RCNT_RTC_SCK_1;
	_rtcSleepUsec(1);
	REG_RCNT_RTC = RCNT_RTC_CS_1 | RCNT_RTC_SCK_1;
	_rtcSleepUsec(1);
}

MEOW_INLINE void _rtcEnd(void)
{
	REG_RCNT_RTC = RCNT_RTC_CS_0 | RCNT_RTC_SCK_1;
}

MEOW_INLINE void _rtcOutputBit(u8 bit)
{
	bit &= 1;
	REG_RCNT_RTC = RCNT_RTC_CS_1 | RCNT_RTC_SCK_0 | RCNT_RTC_SIO_OUT | bit;
	_rtcSleepUsec(5);
	REG_RCNT_RTC = RCNT_RTC_CS_1 | RCNT_RTC_SCK_1 | RCNT_RTC_SIO_OUT | bit;
	_rtcSleepUsec(5);
}

MEOW_INLINE u8 _rtcInputBit(void)
{
	REG_RCNT_RTC = RCNT_RTC_CS_1 | RCNT_RTC_SCK_0;
	_rtcSleepUsec(5);
	REG_RCNT_RTC = RCNT_RTC_CS_1 | RCNT_RTC_SCK_1;
	_rtcSleepUsec(5);
	return REG_RCNT_RTC & RCNT_RTC_SIO;
}

MEOW_INLINE void _rtcOutputCmdByte(u8 byte)
{
	// MSB to LSB
	for (int i = 7; i >= 0; i --) {
		_rtcOutputBit(byte>>i);
	}
}

MEOW_INLINE void _rtcOutputDataByte(u8 byte)
{
	// LSB to MSB
	for (int i = 0; i < 8; i ++) {
		_rtcOutputBit(byte>>i);
	}
}

MEOW_INLINE u8 _rtcInputDataByte(void)
{
	// LSB to MSB
	u8 byte = 0;
	for (int i = 0; i < 8; i ++) {
		byte |= _rtcInputBit()<<i;
	}
	return byte;
}

void rtcInit(void)
{
	// Read current RTC status
	u8 status1 = rtcReadRegister8(RtcReg_Status1);
	u8 status2 = rtcReadRegister8(RtcReg_Status2);

	// Reset the RTC on first use/voltage drop/test mode
	if ((status1 & (RTC_STATUS1_BLD|RTC_STATUS1_POC)) || (status2 & RTC_STATUS2_TEST)) {
		rtcWriteRegister8(RtcReg_Status1, status1 | RTC_STATUS1_RESET);
		status1 = rtcReadRegister8(RtcReg_Status1);

		if (systemIsTwlMode()) {
			// Behold! Nintendo's amazing bodge, used to feed the Atheros hardware
			// (DSi Wifi) a steady 32768Hz clock through the RTC chip's FOUT signal.
			// If Atheros doesn't receive this clock, it reportedly fails to process
			// WMI messages (according to gbatek).
			rtcWriteRegister8(RtcReg_FoutHi, 0x80);
			rtcWriteRegister8(RtcReg_FoutLo, 0x00);
		}
	}

	// Ensure 24-hour mode is selected
	if (!(status1 & RTC_STATUS1_24HRS)) {
		rtcWriteRegister8(RtcReg_Status1, status1 | RTC_STATUS1_24HRS);
	}

	// Disable the RTC interrupts
	rtcWriteRegister8(RtcReg_Status2, 0);
}

void rtcReadRegister(RtcRegister reg, void* data, size_t size)
{
	u8* data8 = (u8*)data;

	_rtcBegin();
	_rtcOutputCmdByte(RTC_CMD(reg,1));

	for (size_t i = 0; i < size; i ++) {
		data8[i] = _rtcInputDataByte();
	}

	_rtcEnd();
}

void rtcWriteRegister(RtcRegister reg, const void* data, size_t size)
{
	const u8* data8 = (const u8*)data;

	_rtcBegin();
	_rtcOutputCmdByte(RTC_CMD(reg,0));

	for (size_t i = 0; i < size; i ++) {
		_rtcOutputDataByte(data8[i]);
	}

	_rtcEnd();
}