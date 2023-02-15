#include <calico/types.h>
#include <calico/nds/system.h>
#include <calico/nds/arm7/pmic.h>
#include <calico/nds/arm7/codec.h>
#include <calico/nds/arm7/i2c.h>
#include <calico/nds/arm7/mcu.h>
#include <calico/nds/pm.h>

unsigned pmGetBatteryState(void)
{
	unsigned ret;

	if (systemIsTwlMode()) {
		// DSi mode: Read state from MCU
		// XX: Official code converts 0-15 into 0-5 with the following formula:
		//     (level&1) + ((level&2)>>1) + ((level&0xc)>>2)
		i2cLock();
		ret = i2cReadRegister8(I2cDev_MCU, McuReg_BatteryState);
		i2cUnlock();
	} else {
		// DS mode: Read state from PMIC
		// TODO: detect DS Lite, only read PmicReg_BacklightLevel on DS Lite
		spiLock();
		ret = (pmicReadRegister(PmicReg_BatteryStatus) & PMIC_BATT_STAT_LOW) ? 3 : 15;
		ret |= (pmicReadRegister(PmicReg_BacklightLevel) & PMIC_BL_CHARGER_DETECTED) ? PM_BATT_CHARGING : 0;
		spiUnlock();
	}

	return ret;
}

void pmSoundSetAmpPower(bool enable)
{
	spiLock();
	if (cdcIsTwlMode()) {
		// TODO: implement this with CODEC
	} else {
		// Use PMIC
		unsigned reg = pmicReadRegister(PmicReg_Control);
		if (enable) {
			reg = (reg &~ PMIC_CTRL_SOUND_MUTE) | PMIC_CTRL_SOUND_ENABLE;
		} else {
			reg = (reg &~ PMIC_CTRL_SOUND_ENABLE) | PMIC_CTRL_SOUND_MUTE;
		}
		pmicWriteRegister(PmicReg_Control, reg);
	}
	spiUnlock();
}
