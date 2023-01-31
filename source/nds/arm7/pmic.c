#include <calico/system/irq.h>
#include <calico/system/thread.h>
#include <calico/nds/arm7/spi.h>
#include <calico/nds/arm7/pmic.h>

static void _pmicReadWriteCommon(u8 reg, u8 data)
{
	spiWaitBusy();
	REG_SPICNT = SpiBaud_1MHz | SPICNT_DEVICE(SpiDev_PMIC) | SPICNT_HOLD | SPICNT_ENABLE;
	REG_SPIDATA = reg;
	spiWaitBusy();
	REG_SPICNT = SpiBaud_1MHz | SPICNT_DEVICE(SpiDev_PMIC) | SPICNT_ENABLE;
	REG_SPIDATA = data;
}

bool pmicWriteRegister(PmicRegister reg, u8 data)
{
	if (!mutexIsLockedByCurrentThread(&g_spiMutex)) {
		return false;
	}

	_pmicReadWriteCommon(reg, data);
	return true;
}

u8 pmicReadRegister(PmicRegister reg)
{
	if (!mutexIsLockedByCurrentThread(&g_spiMutex)) {
		return 0xff;
	}

	_pmicReadWriteCommon(0x80 | reg, 0);

	spiWaitBusy();
	return REG_SPIDATA & 0xff;
}

void pmicIssueShutdown(void)
{
	armIrqLockByPsr();
	spiLock();
	pmicWriteRegister(PmicReg_Control, PMIC_CTRL_SHUTDOWN);
	for (;;); // infinite loop just in case
}
