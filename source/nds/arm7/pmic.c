#include <calico/system/irq.h>
#include <calico/system/thread.h>
#include <calico/nds/arm7/spi.h>
#include <calico/nds/arm7/pmic.h>

void pmicWriteRegister(PmicRegister reg, u8 data)
{
	spiWaitBusy();
	REG_SPICNT = SpiBaud_1MHz | SPICNT_DEVICE(SpiDev_PMIC) | SPICNT_HOLD | SPICNT_ENABLE;
	REG_SPIDATA = reg;
	spiWaitBusy();
	REG_SPICNT = SpiBaud_1MHz | SPICNT_DEVICE(SpiDev_PMIC) | SPICNT_ENABLE;
	REG_SPIDATA = data;
	//spiWaitBusy();
	//return REG_SPIDATA & 0xff;
}

u8 pmicReadRegister(PmicRegister reg)
{
	pmicWriteRegister(0x80 | reg, 0);
	spiWaitBusy();
	return REG_SPIDATA & 0xff;
}
