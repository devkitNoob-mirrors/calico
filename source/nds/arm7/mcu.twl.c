#include <calico/system/irq.h>
#include <calico/system/thread.h>
#include <calico/nds/arm7/pmic.h>
#include <calico/nds/arm7/i2c.h>
#include <calico/nds/arm7/mcu.h>

void _i2cSetMcuDelay(s32 delay);

/*
static void _mcuIrqHandler(void)
{
	unsigned flags = i2cReadRegister8(I2cDev_MCU, McuReg_IrqFlags);
	if (flags & MCU_IRQ_PWRBTN_RESET) {
		// Reset request coming from power button
		i2cWriteRegister8(I2cDev_MCU, McuReg_WarmbootFlag, 1);
		i2cWriteRegister8(I2cDev_MCU, McuReg_DoReset, 1);
	} else if (flags & MCU_IRQ_PWRBTN_SHUTDOWN) {
		// Shutdown request coming from power button
		// TODO: how to actually power off the DSi?
		i2cWriteRegister8(I2cDev_MCU, McuReg_WarmbootFlag, 1);
		i2cWriteRegister8(I2cDev_MCU, McuReg_DoReset, 1);
	}
}
*/

static Thread s_mcuThread;
static alignas(8) u8 s_mcuThreadStack[0x200];

static int _mcuThread(void* unused)
{
	for (;;) {
		threadIrqWait2(false, IRQ2_MCU);
		i2cLock();
		unsigned flags = i2cReadRegister8(I2cDev_MCU, McuReg_IrqFlags);
		if (flags & MCU_IRQ_PWRBTN_RESET) {
			// Reset request coming from power button
			i2cWriteRegister8(I2cDev_MCU, McuReg_WarmbootFlag, 1);
			i2cWriteRegister8(I2cDev_MCU, McuReg_DoReset, 1);
		} else if (flags & MCU_IRQ_PWRBTN_SHUTDOWN) {
			// Shutdown request coming from power button
			// TODO: how to actually power off the DSi?
			//i2cWriteRegister8(I2cDev_MCU, McuReg_WarmbootFlag, 1);
			//i2cWriteRegister8(I2cDev_MCU, McuReg_DoReset, 1);
			i2cWriteRegister8(I2cDev_MCU, McuReg_DoReset, 2);
			pmicWriteRegister(PmicReg_Control, 0x40);
		}
		i2cUnlock();
	}

	return 0;
}


void mcuInit(void)
{
	// Retrieve MCU firmware version, and set up the correct I2C delay for it
	i2cLock();
	unsigned mcuVersion = i2cReadRegister8(I2cDev_MCU, McuReg_Version);
	i2cUnlock();
	if (mcuVersion <= 0x20) {
		_i2cSetMcuDelay(0x180);
	} else {
		_i2cSetMcuDelay(0x90);
	}

	// Set up MCU thread (max prio!)
	threadPrepare(&s_mcuThread, _mcuThread, NULL, &s_mcuThreadStack[sizeof(s_mcuThreadStack)], 0x00);
	threadStart(&s_mcuThread);

	// Enable MCU interrupt
	//irqSet2(IRQ2_MCU, _mcuIrqHandler);
	irqEnable2(IRQ2_MCU);
}
