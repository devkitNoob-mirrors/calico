#include <calico/system/mutex.h>
#include <calico/nds/bios.h>
#include <calico/nds/arm7/i2c.h>

Mutex g_i2cMutex;
static s32 s_i2cDelay, s_i2cMcuDelay;

MEOW_INLINE void _i2cWaitBusy(void)
{
	while (REG_I2C_CNT & 0x80);
}

MEOW_INLINE void _i2cSetDelay(I2cDevice dev)
{
	if (dev == I2cDev_MCU) {
		s_i2cDelay = s_i2cMcuDelay;
	} else {
		s_i2cDelay = 0;
	}
}

void _i2cSetMcuDelay(s32 delay)
{
	s_i2cMcuDelay = delay;
}

MEOW_INLINE void _i2cDelay()
{
	_i2cWaitBusy();
	if (s_i2cDelay > 0) {
		svcWaitByLoop(s_i2cDelay);
		// !!! WARNING !!! Calling a BIOS function from within IRQ mode
		// will use up 4 words of SVC and 2 words of SYS stack from the **CURRENT** thread,
		// which may be the idle thread; which does NOT have enough space for nested syscall
		// frames!
		// For now, and because I am lazy, let's do this in shittily written C.
		//for (volatile s32 i = s_i2cDelay; i; i --);
	}
}

static void _i2cStop(u8 arg0)
{
	if (s_i2cDelay) {
		REG_I2C_CNT = (arg0 << 5) | 0xC0;
		_i2cDelay();
		REG_I2C_CNT = 0xC5;
	} else {
		REG_I2C_CNT = (arg0 << 5) | 0xC1;
	}
}

MEOW_INLINE bool _i2cGetResult()
{
	_i2cWaitBusy();
	return (REG_I2C_CNT >> 4) & 0x01;
}

MEOW_INLINE u8 _i2cGetData()
{
	_i2cWaitBusy();
	return REG_I2C_DATA;
}

static bool _i2cSelectDevice(I2cDevice dev)
{
	_i2cWaitBusy();
	REG_I2C_DATA = dev;
	REG_I2C_CNT = 0xC2;
	return _i2cGetResult();
}

static bool _i2cSelectRegister(u8 reg)
{
	_i2cDelay();
	REG_I2C_DATA = reg;
	REG_I2C_CNT = 0xC0;
	return _i2cGetResult();
}

bool i2cWriteRegister8(I2cDevice dev, u8 reg, u8 data)
{
	if_unlikely (!mutexIsLockedByCurrentThread(&g_i2cMutex)) {
		return false;
	}

	_i2cSetDelay(dev);
	for (unsigned i = 0; i < 8; i ++) {
		if (_i2cSelectDevice(dev) && _i2cSelectRegister(reg)) {
			_i2cDelay();
			REG_I2C_DATA = data;
			_i2cStop(0);

			if (_i2cGetResult()) {
				return true;
			}
        }
		REG_I2C_CNT = 0xC5;
    }

    return false;
}

u8 i2cReadRegister8(I2cDevice dev, u8 reg)
{
	if_unlikely (!mutexIsLockedByCurrentThread(&g_i2cMutex)) {
		return 0xff;
	}

	_i2cSetDelay(dev);
	for (unsigned i = 0; i < 8; i++) {
		if (_i2cSelectDevice(dev) && _i2cSelectRegister(reg)) {
			_i2cDelay();
			if (_i2cSelectDevice(dev | 1)) {
				_i2cDelay();
				_i2cStop(1);
				return _i2cGetData();
			}
		}

		REG_I2C_CNT = 0xC5;
	}

	return 0xff;
}
