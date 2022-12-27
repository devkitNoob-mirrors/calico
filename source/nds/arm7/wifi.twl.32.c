#include <calico/types.h>
#include <calico/system/thread.h>
#include <calico/system/mutex.h>
#include <calico/dev/tmio.h>
#include <calico/dev/sdio.h>
#include <calico/dev/ar6k.h>
#include <calico/nds/bios.h>
#include <calico/nds/ndma.h>
#include <calico/nds/arm7/gpio.h>
#include <calico/nds/arm7/i2c.h>
#include <calico/nds/arm7/mcu.h>

#define TWLWIFI_DEBUG

#ifdef TWLWIFI_DEBUG
#include <calico/system/dietprint.h>
#else
#define dietPrint(...) ((void)0)
#endif

static TmioCtl s_sdioCtl;
static u32 s_sdioCtlBuf[2];
static Thread s_sdioThread;
alignas(8) static u8 s_sdioThreadStack[1024];

static SdioCard s_sdioCard;
static Ar6kDev s_ar6kDev;

static void _sdioIrqHandler(void)
{
	tmioIrqHandler(&s_sdioCtl);
}

static void _twlwifiSetWifiReset(bool reset)
{
	i2cLock();
	unsigned reg = i2cReadRegister8(I2cDev_MCU, McuReg_WifiLed);
	unsigned oldreg = reg;
	reg &= ~(1U<<4);
	if (reset) {
		reg |= 1U<<4;
	}
	i2cWriteRegister8(I2cDev_MCU, McuReg_WifiLed, reg);
	i2cUnlock();
	dietPrint("MCU[0x30]: %.2X -> %.2X\n", oldreg, reg);
}

static bool _twlwifiGetWifiReset(void)
{
	i2cLock();
	unsigned reg = i2cReadRegister8(I2cDev_MCU, McuReg_WifiLed);
	i2cUnlock();
	return (reg & (1U<<4)) != 0;
}

static void _twlwifiSetWifiCompatMode(bool compat)
{
	u16 reg = REG_GPIO_WL;
	u16 oldreg = reg;
	reg &= GPIO_WL_ACTIVE;
	if (compat) {
		reg |= GPIO_WL_MITSUMI;
	}
	REG_GPIO_WL = reg;
	dietPrint("GPIO_WL: %.4X -> %.4X\n", oldreg, reg);
}

bool twlwifiInit(void)
{
	// XX: Below sequence does a full reset of the Atheros hardware, clearing
	// the loaded firmware in the process. We obviously do not want that.
	//_twlwifiSetWifiReset(false);
	//threadSleep(1000);

	// Ensure Atheros is selected and powered on
	_twlwifiSetWifiCompatMode(false);
	if (!_twlwifiGetWifiReset()) {
		_twlwifiSetWifiReset(true);
		threadSleep(1000);
	}

	// Initialize TMIO host controller used for SDIO
	if (!tmioInit(&s_sdioCtl, MM_IO + IO_TMIO1_BASE, MM_IO + IO_TMIO1_FIFO, s_sdioCtlBuf, sizeof(s_sdioCtlBuf)/sizeof(u32))) {
		dietPrint("[TWLWIFI] TMIO init failed\n");
		return false;
	}

	threadPrepare(&s_sdioThread, (ThreadFunc)tmioThreadMain, &s_sdioCtl, &s_sdioThreadStack[sizeof(s_sdioThreadStack)], 0x10);
	threadStart(&s_sdioThread);

	irqSet2(IRQ2_TMIO1, _sdioIrqHandler);
	irqEnable2(IRQ2_TMIO1);

	// Initialize the SDIO card interface
	if (!sdioCardInit(&s_sdioCard, &s_sdioCtl, 0)) {
		dietPrint("[TWLWIFI] SDIO init failed\n");
		return false;
	}

	// Initialize the Atheros wireless device
	if (!ar6kDevInit(&s_ar6kDev, &s_sdioCard)) {
		dietPrint("[TWLWIFI] AR6K init failed\n");
		return false;
	}

	return true;
}
