#include "common.h"
#include <calico/system/thread.h>

#define MWL_MLME_SCAN_SPACING 5

bool _mwlSetMlmeState(MwlMlmeState state)
{
	IrqState st = irqLock();
	MwlMlmeState cur = s_mwlState.mlme_state;
	bool ok = state != MwlMlmeState_Preparing || cur == MwlMlmeState_Idle;
	if (ok) {
		s_mwlState.mlme_state = state;
	}
	irqUnlock(st);

	if (ok && state > MwlMlmeState_Preparing) {
		_mwlPushTask(MwlTask_MlmeProcess);
	}

	return ok;
}

void _mwlMlmeOnBssInfo(WlanBssDesc* bssInfo, WlanBssExtra* bssExtra, unsigned rssi)
{
	if (!s_mwlState.mlme_cb.onBssInfo) {
		return;
	}

	// Apply SSID filter if necessary
	unsigned filter_len = s_mwlState.mlme.scan.filter.target_ssid_len;
	if (filter_len == 0 || (filter_len == bssInfo->ssid_len &&
		__builtin_memcmp(bssInfo->ssid, s_mwlState.mlme.scan.filter.target_ssid, filter_len) == 0)) {
		s_mwlState.mlme_cb.onBssInfo(bssInfo, bssExtra, rssi);
	}
}

MEOW_CONSTEXPR unsigned _mwlMlmeCalcNextScanCh(unsigned ch, unsigned ch_mask)
{
	MEOW_ASSUME(ch_mask != 0);

	// Apply channel spacing increment
	ch += MWL_MLME_SCAN_SPACING;

	// Wrap around if there are no more enabled channels from this point on
	if (!(ch_mask &~ ((1U<<ch)-1))) {
		ch = 0;
	}

	// Retrieve first enabled channel in the mask
	while (!(ch_mask & (1U<<ch))) {
		ch ++;
	}

	return ch;
}

static void _mwlMlmeScanTimeout(TickTask* t)
{
	s_mwlState.mlme.scan.dwell_elapsed += s_mwlState.mlme.scan.update_period;
	bool dwell_over = s_mwlState.mlme.scan.dwell_elapsed >= s_mwlState.mlme.scan.ch_dwell_ticks;
	_mwlSetMlmeState(dwell_over ? MwlMlmeState_ScanSetup : MwlMlmeState_ScanBusy);
}

MEOW_NOINLINE static void _mwlMlmeTaskScan(MwlMlmeState state)
{
	switch (state) {
		default: break;

		case MwlMlmeState_ScanSetup: {
			// Check if there are no more channels to scan
			unsigned ch_mask = s_mwlState.mlme.scan.filter.channel_mask;
			if (ch_mask == 0) {
				if (s_mwlState.mlme_cb.onScanEnd) {
					// Attempt to refill the channel scan mask
					ch_mask = s_mwlState.mlme_cb.onScanEnd() & mwlGetCalibData()->enabled_ch_mask;
				}

				// If we still have no more channels: end the scan
				if (ch_mask == 0) {
					_mwlSetMlmeState(MwlMlmeState_Idle);
					break;
				}

				// Restart the scan
				s_mwlState.mlme.scan.cur_ch = 16;
			}

			// Find next channel to scan
			unsigned ch = _mwlMlmeCalcNextScanCh(s_mwlState.mlme.scan.cur_ch, ch_mask);

			// Switch to new channel
			s_mwlState.mlme.scan.cur_ch = ch;
			s_mwlState.mlme.scan.filter.channel_mask = ch_mask &~ (1U << ch); // flag as explored
			mwlDevSetChannel(ch);

			// Initialize dwell counter and update period
			s_mwlState.mlme.scan.dwell_elapsed = 0;
			s_mwlState.mlme.scan.update_period = s_mwlState.mlme.scan.ch_dwell_ticks;

			// If performing an active scan: schedule 4 probe requests per channel
			if (s_mwlState.mlme.scan.filter.target_ssid_len) {
				s_mwlState.mlme.scan.update_period = (s_mwlState.mlme.scan.update_period + 3) / 4;
			}

			// Set state
			_mwlSetMlmeState(MwlMlmeState_ScanBusy);
			break;
		}

		case MwlMlmeState_ScanBusy: {
			// If performing an active scan: send probe request
			if (s_mwlState.mlme.scan.filter.target_ssid_len) {
				NetBuf* buf = _mwlMgmtMakeProbeRequest(
					s_mwlState.mlme.scan.filter.target_bssid,
					s_mwlState.mlme.scan.filter.target_ssid,
					s_mwlState.mlme.scan.filter.target_ssid_len
				);
				if (buf) {
					mwlDevTx(1, buf, NULL, NULL);
				}
			}

			tickTaskStart(&s_mwlState.timeout_task, _mwlMlmeScanTimeout, s_mwlState.mlme.scan.update_period, 0);
			break;
		}
	}
}

void _mwlMlmeTask(void)
{
	MwlMlmeState state = s_mwlState.mlme_state;

	if (state >= MwlMlmeState_ScanSetup && state <= MwlMlmeState_ScanBusy) {
		_mwlMlmeTaskScan(state);
	}
}

MwlMlmeCallbacks* mwlMlmeGetCallbacks(void)
{
	return &s_mwlState.mlme_cb;
}

bool mwlMlmeScan(WlanBssScanFilter const* filter, unsigned ch_dwell_time)
{
	if (!filter || ch_dwell_time < 10 || s_mwlState.status > MwlStatus_Class1) {
		return false;
	}

	// Enforce enabled channel mask, bail out if empty
	unsigned ch_mask = filter->channel_mask & mwlGetCalibData()->enabled_ch_mask;
	if (!ch_mask) {
		return false;
	}

	// Ensure we can scan
	if (s_mwlState.status != MwlStatus_Idle && !_mwlSetMlmeState(MwlMlmeState_Preparing)) {
		return false;
	}

	// Initialize scanning vars
	s_mwlState.mlme.scan.filter = *filter;
	s_mwlState.mlme.scan.filter.channel_mask = ch_mask;
	s_mwlState.mlme.scan.cur_ch = 16;
	s_mwlState.mlme.scan.ch_dwell_ticks = ticksFromUsec(ch_dwell_time*1000);

	// Set up BSSID filter (or all-FF to disable)
	mwlDevSetBssid(s_mwlState.mlme.scan.filter.target_bssid);

	// Set up short preamble (2mbps) support for passive scans, and long preamble for active scans
	mwlDevSetPreamble(s_mwlState.mlme.scan.filter.target_ssid_len == 0);

	// Start device if needed
	if (s_mwlState.status == MwlStatus_Idle) {
		mwlDevStart();
	}

	// Start scanning task
	return _mwlSetMlmeState(MwlMlmeState_ScanSetup);
}
