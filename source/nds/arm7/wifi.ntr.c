#include <calico/types.h>
#include <calico/dev/mwl.h>
#include <calico/nds/env.h>
#include <calico/nds/system.h>
#include <calico/nds/pm.h>
#include <calico/nds/arm7/gpio.h>
#include <calico/nds/arm7/ntrwifi.h>
#include <string.h>

#define NTRWIFI_DEBUG

#ifdef NTRWIFI_DEBUG
#include <calico/system/dietprint.h>
#else
#define dietPrint(...) ((void)0)
#endif

static struct {
	WlanBssDesc* bssTable;
	unsigned bssCount;
	NtrWifiScanCompleteFn cb;
	void* user;
} s_scanVars;

static void _ntrwifiOnBssInfo(WlanBssDesc* bssInfo, WlanBssExtra* bssExtra, unsigned rssi)
{
	if (!s_scanVars.bssTable) {
		return;
	}

	// Add new entry or find existing entry to overwrite
	WlanBssDesc* desc = wlanFindOrAddBss(s_scanVars.bssTable, &s_scanVars.bssCount, bssInfo->bssid, bssInfo->rssi);
	if (desc) {
		*desc = *bssInfo;
	}
}

static u32 _ntrwifiOnScanEnd(void)
{
	dietPrint("[NTRWIFI] Scan complete\n");

	WlanBssDesc* bss_table = s_scanVars.bssTable;
	if (bss_table) {
		s_scanVars.bssTable = NULL;
		if (s_scanVars.cb) {
			s_scanVars.cb(s_scanVars.user, bss_table, s_scanVars.bssCount);
		}
	}

	mwlDevStop();
	return 0;
}

bool ntrwifiInit(void)
{
	// Ensure Mitsumi calibration data is loaded
	if (!mwlCalibLoad()) {
		return false;
	}

	// Ensure Mitsumi is selected
	if (systemIsTwlMode()) {
		gpioSetWlModule(GpioWlModule_Mitsumi);
	}

	// Enable blinkenlights
	pmSetPowerLed(PmLedMode_BlinkFast);

	// Supply power to Mitsumi
	pmPowerOn(POWCNT_WL_MITSUMI);
	REG_EXMEMCNT2 = 0x30;

	// Boot up and initialize the Mitsumi wireless device
	dietPrint("[NTRWIFI] W_ID = 0x%.4X\n", MWL_REG(W_ID));
	mwlDevWakeUp();
	mwlDevReset();
	mwlDevSetMode(MwlMode_Infra);

	// Install callbacks
	MwlMlmeCallbacks* cb = mwlMlmeGetCallbacks();
	cb->onBssInfo = _ntrwifiOnBssInfo;
	cb->onScanEnd = _ntrwifiOnScanEnd;

	// Copy wireless interface settings
	MwlCalibData* calib = mwlGetCalibData();
	memcpy(g_envExtraInfo->wlmgr_macaddr, calib->mac_addr, 6);
	g_envExtraInfo->wlmgr_channel_mask = calib->enabled_ch_mask;
	g_envExtraInfo->wlmgr_hdr_headroom_sz = sizeof(WlanMacHdr) + sizeof(NetLlcSnapHdr) - sizeof(NetMacHdr);

	return true;
}

void ntrwifiExit(void)
{
	mwlDevStop();
	mwlDevShutdown();
	pmPowerOff(POWCNT_WL_MITSUMI);
	pmSetPowerLed(PmLedMode_Steady);
}

bool ntrwifiStartScan(WlanBssDesc* out_table, WlanBssScanFilter const* filter, NtrWifiScanCompleteFn cb, void* user)
{
	if (s_scanVars.bssTable) {
		return false; // already scanning
	}

	s_scanVars.bssTable = out_table;
	s_scanVars.bssCount = 0;
	s_scanVars.cb = cb;
	s_scanVars.user = user;

	if (filter->target_ssid_len) {
		dietPrint("[NTRWIFI] Scan: %.*s\n", filter->target_ssid_len, filter->target_ssid);
	} else {
		dietPrint("[NTRWIFI] Scan (Passive)\n");
	}

	unsigned dwell_time = filter->target_ssid_len ? 60 : 200; // shorter scan time for active scans

	if (!mwlMlmeScan(filter, dwell_time)) {
		s_scanVars.bssTable = NULL;
		return false;
	}

	return true;
}

bool ntrwifiAssociate(WlanBssDesc const* bss, WlanAuthData const* auth, NtrWifiAssocFn cb, void* user)
{
	dietPrint("[NTRWIFI] Assoc: %.*s\n", bss->ssid_len, bss->ssid);

	// TODO
	return false;
}

bool ntrwifiDisassociate(void)
{
	// TODO
	return false;
}

bool ntrwifiTx(NetBuf* pPacket)
{
	// TODO
	netbufFree(pPacket);
	return true;
}
