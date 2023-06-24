#include "common.h"
#include <calico/system/thread.h>

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

static void _mwlMlmeScanTimeout(TickTask* t)
{
	_mwlSetMlmeState(MwlMlmeState_ScanSetup);
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
				s_mwlState.mlme.scan.filter.channel_mask = ch_mask;
				s_mwlState.mlme.scan.cur_ch = 0;
			}

			// Find next channel to scan
			unsigned ch = s_mwlState.mlme.scan.cur_ch;
			while (!(ch_mask & (1U<<ch))) {
				ch ++;
			}

			// Switch to new channel
			s_mwlState.mlme.scan.filter.channel_mask &= ~(1U << ch); // flag as explored
			s_mwlState.mlme.scan.cur_ch = ch;
			mwlDevSetChannel(ch);

			// XX: Send probe request frame when ssid is provided

			_mwlSetMlmeState(MwlMlmeState_ScanBusy);
			tickTaskStart(&s_mwlState.timeout_task, _mwlMlmeScanTimeout, ticksFromUsec(s_mwlState.mlme.scan.ch_dwell_time*1000), 0);
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
	if (!filter || ch_dwell_time < 10 || s_mwlState.status != MwlStatus_Class1) {
		return false;
	}

	// Enforce enabled channel mask, bail out if empty
	unsigned ch_mask = filter->channel_mask & mwlGetCalibData()->enabled_ch_mask;
	if (!ch_mask) {
		return false;
	}

	// Ensure we can scan
	if (!_mwlSetMlmeState(MwlMlmeState_Preparing)) {
		return false;
	}

	// Initialize scanning vars
	s_mwlState.mlme.scan.filter = *filter;
	s_mwlState.mlme.scan.filter.channel_mask = ch_mask;
	s_mwlState.mlme.scan.ch_dwell_time = ch_dwell_time;
	s_mwlState.mlme.scan.cur_ch = 0;

	// Set up BSSID filter (or all-FF to disable)
	mwlDevSetBssid(s_mwlState.mlme.scan.filter.target_bssid);

	// Start scanning task
	return _mwlSetMlmeState(MwlMlmeState_ScanSetup);
}
