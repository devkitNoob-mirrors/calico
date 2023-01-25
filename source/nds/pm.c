#include <calico/types.h>
#include <calico/arm/common.h>
#include <calico/system/irq.h>
#include <calico/system/thread.h>
#include <calico/nds/env.h>
#include <calico/nds/system.h>
#include <calico/nds/pxi.h>
#include <calico/nds/pm.h>

#if defined(ARM9)
#include <calico/arm/cp15.h>
#include <calico/arm/cache.h>
#elif defined(ARM7)
#include <calico/nds/arm7/pmic.h>
#include <calico/nds/arm7/i2c.h>
#include <calico/nds/arm7/mcu.h>
#endif

#include <sys/iosupport.h>

#define PM_FLAG_RESET_ASSERTED (1U << 0)
#define PM_FLAG_RESET_PREPARED (1U << 1)
#define PM_FLAG_SLEEP_ALLOWED  (1U << 2)

typedef struct PmState {
	PmEventCookie* cookie_list;
	u32 flags;
} PmState;

static PmState s_pmState;

MEOW_NOINLINE static void _pmCallEventHandlers(PmEvent event)
{
	for (PmEventCookie* c = s_pmState.cookie_list; c; c = c->next) {
		c->handler(c->user, event);
	}
}

static void _pmResetPxiHandler(void* user, u32 data)
{
	switch ((data >> 8) & 0x7f) {
		default:
			break;

		case 0x10: // "Reset"
		case 0x20: // "Terminate"
			s_pmState.flags |= PM_FLAG_RESET_ASSERTED;
			break;
	}
}

#if defined(ARM7)

MEOW_CODE32 MEOW_EXTERN32 MEOW_NOINLINE MEOW_NORETURN static void _pmJumpToBootstub(void)
{
	// Remap WRAM_A to the location used by DSi-enhanced (hybrid) apps
	// This is needed for compatibility with libnds
	if (systemIsTwlMode()) {
		MEOW_REG(u32, IO_MBK_MAP_A) = 0x00403000;
	}

	// Jump to ARM7 entrypoint
	((void(*)(void))g_envAppNdsHeader->arm7_entrypoint)();
	for (;;); // just in case
}

#endif

void __SYSCALL(exit)(int rc)
{
	MEOW_DUMMY(rc); // TODO: Do something useful with this

	// Call event handlers
	_pmCallEventHandlers(PmEvent_OnReset);

	// Assert reset on the other processor
	pxiSend(PxiChannel_Reset, 0x10 << 8);

	// Wait for reset to be asserted on us
	while (!(s_pmState.flags & PM_FLAG_RESET_ASSERTED)) {
		threadSleep(1000);
	}

#if defined(ARM9)

	// Disable all interrupts
	armIrqLockByPsr();
	irqLock();

	// Hang indefinitely if there is no jump target
	if (!pmHasResetJumpTarget()) {
		for (;;);
	}

	// Flush caches
	armDCacheFlushAll();
	armICacheInvalidateAll();

	// Disable PU and caches
	u32 cp15_cr;
	__asm__ __volatile__ ("mrc p15, 0, %0, c1, c0, 0" : "=r" (cp15_cr));
	cp15_cr &= ~(CP15_CR_PU_ENABLE | CP15_CR_DCACHE_ENABLE | CP15_CR_ICACHE_ENABLE | CP15_CR_ROUND_ROBIN);
	__asm__ __volatile__ ("mcr p15, 0, %0, c1, c0, 0" :: "r" (cp15_cr));

	// Jump to ARM9 bootstub entrypoint
	g_envNdsBootstub->arm9_entrypoint();

#elif defined(ARM7)

	// If there is no jump target, reset or shut down the DS
	if (!pmHasResetJumpTarget()) {
		if (systemIsTwlMode()) {
			// Issue reset through MCU
			i2cLock();
			i2cWriteRegister8(I2cDev_MCU, McuReg_WarmbootFlag, 1);
			i2cWriteRegister8(I2cDev_MCU, McuReg_DoReset, 1);
		} else {
			// Use PMIC to shut down the DS
			pmicWriteRegister(PmicReg_Control, 0x40);
		}

		// Above should have killed us. Otherwise: infinite loop for safety
		for (;;);
	}

	// Disable interrupts
	armIrqLockByPsr();
	irqLock();

	// Perform PXI sync sequence
	REG_PXI_SYNC = PXI_SYNC_SEND(1);
	while (PXI_SYNC_RECV(REG_PXI_SYNC) != 1);
	REG_PXI_SYNC = PXI_SYNC_SEND(0);
	while (PXI_SYNC_RECV(REG_PXI_SYNC) != 0);

	// Clear PXI FIFO
	while (!(REG_PXI_CNT & PXI_CNT_RECV_EMPTY)) {
		MEOW_DUMMY(REG_PXI_RECV);
	}

	// Jump to new ARM7 entrypoint
	_pmJumpToBootstub();

#endif
}

void pmInit(void)
{
	s_pmState.flags = PM_FLAG_SLEEP_ALLOWED;

	pxiSetHandler(PxiChannel_Reset, _pmResetPxiHandler, NULL);

#if defined(ARM9)
	//...
#elif defined(ARM7)
	if (systemIsTwlMode()) {
		mcuInit();
	}
#endif
}

void pmAddEventHandler(PmEventCookie* cookie, PmEventFn handler, void* user)
{
	if (!handler) {
		return;
	}

	IrqState st = irqLock();
	cookie->next = s_pmState.cookie_list;
	cookie->handler = handler;
	cookie->user = user;
	s_pmState.cookie_list = cookie;
	irqUnlock(st);
}

void pmRemoveEventHandler(PmEventCookie* cookie)
{
	// TODO
}

bool pmShouldReset(void)
{
	return s_pmState.flags & (PM_FLAG_RESET_ASSERTED|PM_FLAG_RESET_PREPARED);
}

void pmPrepareToReset(void)
{
	IrqState st = irqLock();
	s_pmState.flags |= PM_FLAG_RESET_PREPARED;
	irqUnlock(st);
}

bool pmIsSleepAllowed(void)
{
	return s_pmState.flags & PM_FLAG_SLEEP_ALLOWED;
}

void pmSetSleepAllowed(bool allowed)
{
	IrqState st = irqLock();
	if (allowed) {
		s_pmState.flags |= PM_FLAG_SLEEP_ALLOWED;
	} else {
		s_pmState.flags &= ~PM_FLAG_SLEEP_ALLOWED;
	}
	irqUnlock(st);
}

bool pmHasResetJumpTarget(void)
{
	return g_envNdsBootstub->magic == ENV_NDS_BOOTSTUB_MAGIC;
}

void pmClearResetJumpTarget(void)
{
	g_envNdsBootstub->magic = 0;
}

bool pmMainLoop(void)
{
	// TODO: Sleep mode handling
	return !pmShouldReset();
}
