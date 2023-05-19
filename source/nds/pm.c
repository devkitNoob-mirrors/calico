#include <calico/types.h>
#include <calico/arm/common.h>
#include <calico/system/irq.h>
#include <calico/system/thread.h>
#include <calico/system/mailbox.h>
#include <calico/nds/env.h>
#include <calico/nds/bios.h>
#include <calico/nds/system.h>
#include <calico/nds/pxi.h>
#include <calico/nds/keypad.h>
#include <calico/nds/pm.h>
#include "pxi/reset.h"
#include "pxi/pm.h"

#if defined(ARM9)
#include <calico/arm/cp15.h>
#include <calico/arm/cache.h>
#elif defined(ARM7)
#include <calico/nds/arm7/gpio.h>
#include <calico/nds/arm7/pmic.h>
#include <calico/nds/arm7/i2c.h>
#include <calico/nds/arm7/mcu.h>
#endif

#include <sys/iosupport.h>

#if defined(ARM9)
#define PM_THREAD_PRIO 0x01
#elif defined(ARM7)
#define PM_THREAD_PRIO (MAIN_THREAD_PRIO-1)
#endif

#define PM_HINGE_SLEEP_THRESHOLD 20

#define PM_FLAG_RESET_ASSERTED (1U << 0)
#define PM_FLAG_RESET_PREPARED (1U << 1)
#define PM_FLAG_SLEEP_ALLOWED  (1U << 2)
#define PM_FLAG_SLEEP_ORDERED  (1U << 3)
#define PM_FLAG_WAKEUP         (1U << 4)

typedef struct PmState {
	PmEventCookie* cookie_list;
	u32 flags;

#if defined(ARM7)
	u8 hinge_counter;
#elif defined(ARM9)
	// future use
#endif

} PmState;

static PmState s_pmState;

static Thread s_pmPxiThread;
static alignas(8) u8 s_pmPxiThreadStack[0x200];

MEOW_NOINLINE static void _pmCallEventHandlers(PmEvent event)
{
	for (PmEventCookie* c = s_pmState.cookie_list; c; c = c->next) {
		c->handler(c->user, event);
	}
}

static void _pmResetPxiHandler(void* user, u32 msg)
{
	switch (pxiResetGetType(msg)) {
		default:
			break;

		case PxiResetMsgType_Reset:
		case PxiResetMsgType_Abort:
			s_pmState.flags |= PM_FLAG_RESET_ASSERTED;
			break;
	}
}

static int _pmPxiThreadMain(void* arg)
{
	// Set up PXI mailbox
	Mailbox mb;
	u32 mb_slots[4];
	mailboxPrepare(&mb, mb_slots, sizeof(mb_slots)/sizeof(u32));

	// Register PXI channels
	pxiSetHandler(PxiChannel_Reset, _pmResetPxiHandler, NULL);
	pxiSetMailbox(PxiChannel_Power, &mb);

	// Handle PXI messages
	for (;;) {
		u32 msg = mailboxRecv(&mb);
		PxiPmMsgType type = pxiPmGetType(msg);
		unsigned imm = pxiPmGetImmediate(msg);
		MEOW_DUMMY(imm);

		switch (type) {
			default: break;

			case PxiPmMsg_Sleep: {
				IrqState st = irqLock();
				s_pmState.flags |= PM_FLAG_SLEEP_ORDERED;
				irqUnlock(st);
				break;
			}

#if defined(ARM9)

			case PxiPmMsg_Wakeup: {
				IrqState st = irqLock();
				s_pmState.flags |= PM_FLAG_WAKEUP;
				irqUnlock(st);
				break;
			}

#elif defined(ARM7)

			case PxiPmMsg_GetBatteryState:
				pxiReply(PxiChannel_Power, pmGetBatteryState());
				break;

			case PxiPmMsg_ReadNvram: {
				void* data = (void*)mailboxRecv(&mb);
				u32 addr = mailboxRecv(&mb);
				u32 len = mailboxRecv(&mb);
				pxiReply(PxiChannel_Power, pmReadNvram(data, addr, len));
				break;
			}

			case PxiPmMsg_MicSetAmp: {
				pmMicSetAmp((imm>>8)&1, imm&0xff);
				pxiReply(PxiChannel_Power, 0);
				break;
			}
#endif

		}
	}

	return 0;
}

#if defined(ARM7)

MEOW_CODE32 MEOW_EXTERN32 MEOW_NOINLINE MEOW_NORETURN static void _pmJumpToNextApp(void)
{
	// Back up DSi mode flag
	bool is_twl = systemIsTwlMode();
	armCompilerBarrier();

	// Copy new ARM7 binary to WRAM if needed
	if (g_envAppNdsHeader->arm7_ram_address >= MM_A7WRAM) {
		vu32* dst = (vu32*)g_envAppNdsHeader->arm7_ram_address; // volatile to avoid memcpy optimization
		u32* src = (u32*)(MM_MAINRAM + MM_MAINRAM_SZ_NTR - 512*1024);
		u32 count = (g_envAppNdsHeader->arm7_size + 3) / 4;
		do {
			*dst++ = *src++;
		} while (count--);
	}

	// Set up MBK regs if needed
	if (is_twl) {
		MEOW_REG(u32, IO_MBK_SLOTWRPROT) = g_envAppTwlHeader->mbk9_setting;
		MEOW_REG(u32, IO_MBK_MAP_A) = g_envAppTwlHeader->arm7_wram_setting[0];
		MEOW_REG(u32, IO_MBK_MAP_B) = g_envAppTwlHeader->arm7_wram_setting[1];
		MEOW_REG(u32, IO_MBK_MAP_C) = g_envAppTwlHeader->arm7_wram_setting[2];
	}

	// Jump to ARM7 entrypoint
	((void(*)(void))g_envAppNdsHeader->arm7_entrypoint)();
	for (;;); // just in case
}

MEOW_CODE32 MEOW_EXTERN32 MEOW_NOINLINE MEOW_NORETURN static void _pmJumpToBootstub(void)
{
	// Remap WRAM_A to the location used by DSi-enhanced (hybrid) apps
	// This is needed for compatibility with libnds v1.x infrastructure
	if (systemIsTwlMode()) {
		MEOW_REG(u32, IO_MBK_MAP_A) = 0x00403000;
	}

	// Jump to ARM7 entrypoint
	((void(*)(void))g_envAppNdsHeader->arm7_entrypoint)();
	for (;;); // just in case
}

MEOW_WEAK void rtcSyncTime(void)
{
	// Dummy function used in case the RTC code isn't linked in
}

#elif defined(ARM9)

MEOW_WEAK void systemUserExit(void)
{
	// Nothing
}

#endif

MEOW_WEAK void systemErrorExit(int rc)
{
	// Nothing
}

void __SYSCALL(exit)(int rc)
{
	// Call error exit handler if needed
	if (rc != 0) {
		systemErrorExit(rc);
	}

	// Call event handlers
	_pmCallEventHandlers(PmEvent_OnReset);

	// Assert reset on the other processor
	pxiSend(PxiChannel_Reset, pxiResetMakeMsg(PxiResetMsgType_Reset));

	// Wait for reset to be asserted on us
	while (!(s_pmState.flags & PM_FLAG_RESET_ASSERTED)) {
		threadSleep(1000);
	}

#if defined(ARM9)

	// Call user deinitialization function
	systemUserExit();

	// Disable all interrupts
	armIrqLockByPsr();
	irqLock();

	// Hang indefinitely if there is no jump target
	if (!pmHasResetJumpTarget()) {
		for (;;);
	}

	// Flush data cache
	armDCacheFlushAll();

	// Retrieve jump target address
	void (* jump_target)(void);
	if (g_envExtraInfo->pm_chainload_flag) {
		jump_target = (void(*)(void)) g_envAppNdsHeader->arm9_entrypoint;

		// Perform PXI sync sequence
		while (PXI_SYNC_RECV(REG_PXI_SYNC) != 1);
		REG_PXI_SYNC = PXI_SYNC_SEND(1);
		while (PXI_SYNC_RECV(REG_PXI_SYNC) != 0);
		REG_PXI_SYNC = PXI_SYNC_SEND(0);
	} else {
		jump_target = g_envNdsBootstub->arm9_entrypoint;
	}

	// Disable PU and caches
	u32 cp15_cr;
	__asm__ __volatile__ ("mrc p15, 0, %0, c1, c0, 0" : "=r" (cp15_cr));
	cp15_cr &= ~(CP15_CR_PU_ENABLE | CP15_CR_DCACHE_ENABLE | CP15_CR_ICACHE_ENABLE | CP15_CR_ROUND_ROBIN);
	__asm__ __volatile__ ("mcr p15, 0, %0, c1, c0, 0" :: "r" (cp15_cr) : "memory");

	// Jump to ARM9 entrypoint
	jump_target();
	for (;;); // just in case

#elif defined(ARM7)

	// On DSi, if the user started to press the power button, wait for the
	// MCU to figure out the desired command (reset/shutdown). Note that
	// we will only see reset requests, as shutdown handling takes effect
	// immediately (see explanation in pmInit).
	if (systemIsTwlMode()) {
		while (mcuGetPwrBtnState() == McuPwrBtnState_Begin) {
			threadSleep(1000);
		}
	}

	// If there is no jump target, reset or shut down the DS
	if (!pmHasResetJumpTarget()) {
		if (systemIsTwlMode()) {
			// Issue reset through MCU
			mcuIssueReset();
		} else {
			// Use PMIC to shut down the DS
			pmicIssueShutdown();
		}
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
	if (g_envExtraInfo->pm_chainload_flag) {
		_pmJumpToNextApp();
	} else {
		_pmJumpToBootstub();
	}

#endif
}

void pmInit(void)
{
	s_pmState.flags = PM_FLAG_SLEEP_ALLOWED;

#if defined(ARM9)
	//...
#elif defined(ARM7)
	// Set up soft-reset key combination
	REG_KEYCNT = KEY_SELECT | KEY_START | KEY_R | KEY_L | KEYCNT_IRQ_ENABLE | KEYCNT_IRQ_AND;
	irqSet(IRQ_KEYPAD, pmPrepareToReset);
	irqEnable(IRQ_KEYPAD);

	// Power off Mitsumi wireless hardware in case it was previously left enabled
	u16 powcnt = REG_POWCNT;
	if (powcnt & POWCNT_WL_MITSUMI) {
		REG_POWCNT = powcnt &~ POWCNT_WL_MITSUMI;
	}

	if (systemIsTwlMode()) {
		// Initialize DSi MCU, and register interrupt handlers for it.
		// Power button:
		//   Begin pressing: we issue a reset request, which is later handled
		//     by the application main loop.
		//   Reset (aka tap): handled by __SYSCALL(exit) (refer to it)
		//   Shutdown (aka hold): immediately shut down (see below explanation)

		// For safety reasons, immediately shut down the DSi upon receiving a
		// power button shutdown/battery empty message. Note that a timeframe
		// exists between the power button being physically pushed and released
		// (see McuReg_PwrBtnHoldDelay), which seems to be intended to allow the
		// running application to finish writing any unsaved data to disk/etc.

		mcuInit();
		mcuIrqSet(MCU_IRQ_PWRBTN_SHUTDOWN|MCU_IRQ_BATTERY_EMPTY, (McuIrqHandler)mcuIssueShutdown);
		mcuIrqSet(MCU_IRQ_PWRBTN_BEGIN, (McuIrqHandler)pmPrepareToReset);

		// Check which wireless hardware is enabled, and switch to Mitsumi if
		// Atheros is active. This is necessary in order to prevent sleep mode
		// from causing a hardware fault/shutdown when the Atheros hardware is
		// left in a dirty/undefined state (such as when using certain software).
		gpioSetWlModule(GpioWlModule_Mitsumi);
	}
#endif

	// Bring up PXI thread
	threadPrepare(&s_pmPxiThread, _pmPxiThreadMain, NULL, &s_pmPxiThreadStack[sizeof(s_pmPxiThreadStack)], PM_THREAD_PRIO);
	threadStart(&s_pmPxiThread);

	// Wait for the other CPU to bring up their PXI thread
	pxiWaitRemote(PxiChannel_Power);
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

	u32 flags = s_pmState.flags;
	if (allowed) {
		flags |= PM_FLAG_SLEEP_ALLOWED;
	} else {
		flags &= ~PM_FLAG_SLEEP_ALLOWED;
	}

	if (flags != s_pmState.flags) {
		s_pmState.flags = flags;
#ifdef ARM7
		s_pmState.hinge_counter = PM_HINGE_SLEEP_THRESHOLD-1;
#endif
	}

	irqUnlock(st);
}

static bool _pmWasOrderedToSleep(void)
{
	bool order = false;
	IrqState st = irqLock();
	u32 flags = s_pmState.flags;
	if (flags & PM_FLAG_SLEEP_ORDERED) {
		order = true;
		s_pmState.flags = flags &~ PM_FLAG_SLEEP_ORDERED;
	}
	irqUnlock(st);
	return order;
}

bool pmHasResetJumpTarget(void)
{
	return g_envExtraInfo->pm_chainload_flag || g_envNdsBootstub->magic == ENV_NDS_BOOTSTUB_MAGIC;
}

void pmClearResetJumpTarget(void)
{
	g_envExtraInfo->pm_chainload_flag = 0;
	g_envNdsBootstub->magic = 0;
}

bool pmMainLoop(void)
{
	bool sleep_allowed = pmIsSleepAllowed();

#ifdef ARM7
	if (sleep_allowed) {
		// Handle hinge-driven auto-sleep
		bool hinge_state = (keypadGetExtState() & KEY_HINGE) != 0;
		if (!hinge_state) {
			s_pmState.hinge_counter = 0;
		} else {
			if (s_pmState.hinge_counter < PM_HINGE_SLEEP_THRESHOLD) {
				s_pmState.hinge_counter ++;
			}

			if (s_pmState.hinge_counter == PM_HINGE_SLEEP_THRESHOLD) {
				// Hinge has been down for a certain number of frames. We should sleep
				s_pmState.hinge_counter = UINT8_MAX;
				pxiSend(PxiChannel_Power, pxiPmMakeMsg(PxiPmMsg_Sleep, 0));
			}
		}
	}
#endif

	if (_pmWasOrderedToSleep()) {
		if (sleep_allowed) {
			pmEnterSleep();
		}
#ifdef ARM7
		else {
			// Sleep is disallowed, so wake up the other CPU right away
			pxiSend(PxiChannel_Power, pxiPmMakeMsg(PxiPmMsg_Wakeup, 0));
		}
#endif
	}

	return !pmShouldReset();
}

void pmEnterSleep(void)
{
	_pmCallEventHandlers(PmEvent_OnSleep);

#ifdef ARM7
	if (systemIsTwlMode()) {
		i2cLock();
	}

	spiLock();
#endif

	ArmIrqState cpsr_if = armIrqLockByPsr();
	IrqState ime = irqLock();

#if defined(ARM9)
	u32 ie = REG_IE;
	REG_IE = ie & (IRQ_PXI_RECV|IRQ_TIMER2);

	u32 powcnt = REG_POWCNT;
	if (powcnt & POWCNT_LCD) {
		REG_POWCNT = powcnt &~ POWCNT_LCD;
	}

	s_pmState.flags &= ~PM_FLAG_WAKEUP;
#elif defined(ARM7)
	u32 ie = REG_IE;
	u32 ie2 = REG_IE2;
	REG_IE = IRQ_HINGE;
	REG_IE2 = 0;

	unsigned pmic_ctl = pmicReadRegister(PmicReg_Control);
	pmicWriteRegister(PmicReg_Control, PMIC_CTRL_LED_BLINK_SLOW);
#endif

	irqUnlock(1);
	armIrqUnlockByPsr(0);

#if defined(ARM9)
	pxiSend(PxiChannel_Power, pxiPmMakeMsg(PxiPmMsg_Sleep, 0));
	do {
		armWaitForIrq();
	} while (!(s_pmState.flags & PM_FLAG_WAKEUP));

	REG_POWCNT = powcnt;
#elif defined(ARM7)
	svcSleep();
#endif

	armIrqLockByPsr();
	REG_IE = ie;
#ifdef ARM7
	REG_IE2 = ie2;
#endif
	irqUnlock(ime);
	armIrqUnlockByPsr(cpsr_if);

#ifdef ARM7
	pmicWriteRegister(PmicReg_Control, pmic_ctl);

	spiUnlock();
	if (systemIsTwlMode()) {
		i2cUnlock();
	}

	rtcSyncTime();
#endif

	_pmCallEventHandlers(PmEvent_OnWakeup);

#ifdef ARM7
	pxiSend(PxiChannel_Power, pxiPmMakeMsg(PxiPmMsg_Wakeup, 0));
#endif
}
