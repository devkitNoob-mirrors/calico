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
				NetBuf* buf = _mwlMgmtMakeProbeReq(
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

static void _mwlMlmeJoinTimeout(TickTask* t)
{
	_mwlSetMlmeState(MwlMlmeState_JoinDone);
}

void _mwlMlmeHandleJoin(WlanBeaconHdr* beaconHdr, WlanBssDesc* bssInfo, WlanBssExtra* bssExtra)
{
	tickTaskStop(&s_mwlState.timeout_task);

	// XX: Official code validates here the beacon against the info provided by the user.
	// We instead prefer to believe the user did not lie to us.

	// Configure power saving and beacon timing
	MWL_REG(W_LISTENINT) = bssExtra->tim->dtim_period;
	MWL_REG(W_LISTENCOUNT) = 0;
	MWL_REG(W_BEACONINT) = beaconHdr->interval;
	MWL_REG(W_POWER_TX) |= 1;

	// Successful join!
	s_mwlState.has_beacon_sync = 1;
	_mwlSetMlmeState(MwlMlmeState_JoinDone);
}

static void _mwlMlmeJoinDone(void)
{
	_mwlSetMlmeState(MwlMlmeState_Idle);

	// Invoke callback if needed
	if (s_mwlState.mlme_cb.onJoinEnd) {
		s_mwlState.mlme_cb.onJoinEnd(s_mwlState.has_beacon_sync);
	}
}

static void _mwlMlmeAuthTimeout(TickTask* t)
{
	_mwlSetMlmeState(MwlMlmeState_AuthDone);
}

static void _mwlMlmeAuthRaceWorkaround(TickTask* t)
{
	_mwlSetMlmeState(MwlMlmeState_AuthBusy);
}

void _mwlMlmeHandleAuth(NetBuf* pPacket)
{
	WlanAuthHdr* hdr = netbufPopHeaderType(pPacket, WlanAuthHdr);
	if (!hdr) {
		dietPrint("[MWL] Bad auth packet\n");
		return;
	}

	dietPrint("[MWL] Auth alg=%u seq=%u st=%u\n", hdr->algorithm_id, hdr->sequence_num, hdr->status);
	if (hdr->algorithm_id != s_mwlState.wep_enabled) {
		dietPrint("[MWL] Bad auth algo\n");
		return;
	}

	if (hdr->algorithm_id == 0) { // Open System
		if (hdr->sequence_num == 2) {
			s_mwlState.mlme.auth.status = hdr->status;
			_mwlSetMlmeState(MwlMlmeState_AuthDone);
		}
	} else if (hdr->algorithm_id == 1) { // Shared Key
		if (hdr->status != 0 || hdr->sequence_num == 4) {
			s_mwlState.mlme.auth.status = hdr->status;
			_mwlSetMlmeState(MwlMlmeState_AuthDone);
		} else if (hdr->sequence_num == 2) {
			WlanIeHdr* ie = netbufPopHeaderType(pPacket, WlanIeHdr);
			if (!ie || ie->id != WlanEid_ChallengeText || !ie->len) {
				dietPrint("[MWL] Missing challenge text\n");
				return;
			}

			void* data = netbufPopHeader(pPacket, ie->len);
			if (!data) {
				dietPrint("[MWL] Truncated challenge text\n");
				return;
			}

			// Discard any previously crafted challenge reply packets
			// (see below comment for explanation)
			tickTaskStop(&s_mwlState.periodic_task);
			if (s_mwlState.mlme.auth.pTxPacket) {
				netbufFree(s_mwlState.mlme.auth.pTxPacket);
			}

			// Create a new challenge reply packet.
			// XX: Some routers (such as my ASUS RT-N12E C1) have a bugged implementation of the 802.11
			// Shared Key authentication protocol. Namely, they send out two consecutive challenge
			// packets (with different challenge texts!), seemingly due to a bugged retry timeout.
			// Authentication only works when the reply packet contains the *latest* challenge text.
			// A race condition occurs when the client replies to the first packet after the router has
			// already decided to send a second packet. For this reason, we delay sending the reply by
			// 100ms in order to ensure that we always include the latest received challenge text.
			hdr->sequence_num = 3;
			s_mwlState.mlme.auth.pTxPacket = _mwlMgmtMakeAuth(s_mwlState.bssid, hdr, data, ie->len);
			if (s_mwlState.mlme.auth.pTxPacket) {
				tickTaskStart(&s_mwlState.periodic_task,_mwlMlmeAuthRaceWorkaround, ticksFromUsec(100000), 0);
			}
		}
	}
}

static void _mwlMlmeAuthSend(void)
{
	NetBuf* pPacket = s_mwlState.mlme.auth.pTxPacket;
	if (pPacket) {
		s_mwlState.mlme.auth.pTxPacket = NULL;
		mwlDevTx(1, pPacket, NULL, NULL);
	}
}

void _mwlMlmeAuthFreeReply(void)
{
	if (s_mwlState.mlme.auth.pTxPacket) {
		netbufFree(s_mwlState.mlme.auth.pTxPacket);
		s_mwlState.mlme.auth.pTxPacket = NULL;
	}
}

static void _mwlMlmeAuthDone(void)
{
	tickTaskStop(&s_mwlState.timeout_task);
	tickTaskStop(&s_mwlState.periodic_task);

	// Free any unsent authentication packets
	_mwlMlmeAuthFreeReply();

	unsigned status = s_mwlState.mlme.auth.status;
	_mwlSetMlmeState(MwlMlmeState_Idle);

	if (status == 0) {
		// Success! We are now authenticated
		s_mwlState.status = MwlStatus_Class2;
	}

	// Invoke callback if needed
	if (s_mwlState.mlme_cb.onAuthEnd) {
		s_mwlState.mlme_cb.onAuthEnd(status);
	}
}

static void _mwlMlmeAssocTimeout(TickTask* t)
{
	_mwlSetMlmeState(MwlMlmeState_AssocDone);
}

static void _mwlMlmeAssocSend(void)
{
	NetBuf* pPacket = _mwlMgmtMakeAssocReq(s_mwlState.bssid, s_mwlState.mlme.assoc.fake_cck_rates);
	if (pPacket) {
		mwlDevTx(1, pPacket, NULL, NULL);
	}
}

void _mwlMlmeHandleAssocResp(NetBuf* pPacket)
{
	WlanAssocRespHdr* hdr = netbufPopHeaderType(pPacket, WlanAssocRespHdr);
	if (!hdr) {
		dietPrint("[MWL] Bad assoc resp packet\n");
		return;
	}

	s_mwlState.mlme.assoc.status = hdr->status;
	_mwlSetMlmeState(MwlMlmeState_AssocDone);
}

static void _mwlMlmeAssocDone(void)
{
	tickTaskStop(&s_mwlState.timeout_task);

	unsigned status = s_mwlState.mlme.assoc.status;
	_mwlSetMlmeState(MwlMlmeState_Idle);

	if (status == 0) {
		// Success! We are now associated
		s_mwlState.status = MwlStatus_Class3;
	} else {
		// Association failure means losing authentication
		s_mwlState.status = MwlStatus_Class1;
	}

	// Invoke callback if needed
	if (s_mwlState.mlme_cb.onAssocEnd) {
		s_mwlState.mlme_cb.onAssocEnd(status);
	}
}

static void _mwlMlmeDeauthTxCb(void* arg, MwlTxEvent evt, MwlDataTxHdr* hdr)
{
	if (evt == MwlTxEvent_Queued) {
		return;
	}

	if (_mwlSetMlmeState(MwlMlmeState_OnStateLost)) {
		s_mwlState.mlme.loss.reason = 36;
		s_mwlState.mlme.loss.new_class = MwlStatus_Class1;
	}
}

static void _mwlMlmeOnStateLost(void)
{
	unsigned reason = s_mwlState.mlme.loss.reason;
	MwlStatus new_class = s_mwlState.mlme.loss.new_class;

	s_mwlState.status = new_class;
	_mwlSetMlmeState(MwlMlmeState_Idle);

	_mwlRxQueueClear();
	for (unsigned i = 0; i < 3; i ++) {
		_mwlTxQueueClear(i);
	}

	if (s_mwlState.mlme_cb.onStateLost) {
		s_mwlState.mlme_cb.onStateLost(new_class, reason);
	}
}

void _mwlMlmeTask(void)
{
	MwlMlmeState state = s_mwlState.mlme_state;

	if (state >= MwlMlmeState_ScanSetup && state <= MwlMlmeState_ScanBusy) {
		_mwlMlmeTaskScan(state);
	} else if (state == MwlMlmeState_JoinDone) {
		_mwlMlmeJoinDone();
	} else if (state == MwlMlmeState_AuthBusy) {
		_mwlMlmeAuthSend();
	} else if (state == MwlMlmeState_AuthDone) {
		_mwlMlmeAuthDone();
	} else if (state == MwlMlmeState_AssocBusy) {
		_mwlMlmeAssocSend();
	} else if (state == MwlMlmeState_AssocDone) {
		_mwlMlmeAssocDone();
	} else if (state == MwlMlmeState_OnStateLost) {
		_mwlMlmeOnStateLost();
	}
}

MwlMlmeCallbacks* mwlMlmeGetCallbacks(void)
{
	return &s_mwlState.mlme_cb;
}

bool mwlMlmeScan(WlanBssScanFilter const* filter, unsigned ch_dwell_time)
{
	// Validate parameters/state
	if (!filter || ch_dwell_time < 10 ||
		s_mwlState.mode < MwlMode_LocalGuest || s_mwlState.status > MwlStatus_Class1 || s_mwlState.has_beacon_sync) {
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

bool mwlMlmeJoin(WlanBssDesc const* bssInfo, unsigned timeout)
{
	// Validate parameters/state
	if (!bssInfo || timeout < 100 ||
		s_mwlState.mode < MwlMode_LocalGuest || s_mwlState.status > MwlStatus_Class1 || s_mwlState.has_beacon_sync) {
		return false;
	}

	// Ensure we can join
	if (s_mwlState.status != MwlStatus_Idle && !_mwlSetMlmeState(MwlMlmeState_Preparing)) {
		return false;
	}

	// Apply hardware configuration
	mwlDevSetChannel(bssInfo->channel);
	mwlDevSetBssid(bssInfo->bssid);
	mwlDevSetSsid(bssInfo->ssid, bssInfo->ssid_len);
	mwlDevSetPreamble((bssInfo->ieee_caps & (1U<<5)) != 0);

	// Start device if needed
	if (s_mwlState.status == MwlStatus_Idle) {
		mwlDevStart();
	}

	// Start joining task with the specified timeout
	tickTaskStart(&s_mwlState.timeout_task, _mwlMlmeJoinTimeout, ticksFromUsec(timeout*1000), 0);
	return _mwlSetMlmeState(MwlMlmeState_JoinBusy);
}

bool mwlMlmeAuthenticate(unsigned timeout)
{
	// Validate parameters/state
	if (timeout < 100 ||
		s_mwlState.mode < MwlMode_LocalGuest || s_mwlState.status != MwlStatus_Class1 || !s_mwlState.has_beacon_sync) {
		return false;
	}

	// Ensure we can authenticate
	if (!_mwlSetMlmeState(MwlMlmeState_Preparing)) {
		return false;
	}

	WlanAuthHdr auth = {
		.algorithm_id = s_mwlState.wep_enabled, // Open System (0) or Shared Key (1)
		.sequence_num = 1, // First message
		.status       = 0, // Reserved
	};

	NetBuf* pPacket = _mwlMgmtMakeAuth(s_mwlState.bssid, &auth, NULL, 0);
	if (!pPacket) {
		_mwlSetMlmeState(MwlMlmeState_Idle);
		return false;
	}

	// Set up state vars
	s_mwlState.mlme.auth.pTxPacket = pPacket;
	s_mwlState.mlme.auth.status = 16; // Timeout

	// Start authentication task with the specified timeout
	tickTaskStart(&s_mwlState.timeout_task, _mwlMlmeAuthTimeout, ticksFromUsec(timeout*1000), 0);
	return _mwlSetMlmeState(MwlMlmeState_AuthBusy);
}

bool mwlMlmeAssociate(unsigned timeout, bool fake_cck_rates)
{
	// Validate parameters/state
	if (timeout < 100 ||
		s_mwlState.mode < MwlMode_LocalGuest || s_mwlState.status != MwlStatus_Class2) {
		return false;
	}

	// Ensure we can associate
	if (!_mwlSetMlmeState(MwlMlmeState_Preparing)) {
		return false;
	}

	// Set up state vars
	s_mwlState.mlme.assoc.status = 1; // Unspecified failure
	s_mwlState.mlme.assoc.fake_cck_rates = fake_cck_rates;

	// Start association task with the specified timeout
	tickTaskStart(&s_mwlState.timeout_task, _mwlMlmeAssocTimeout, ticksFromUsec(timeout*1000), 0);
	return _mwlSetMlmeState(MwlMlmeState_AssocBusy);
}

bool mwlMlmeDeauthenticate(void)
{
	// Validate state
	if (s_mwlState.mode < MwlMode_LocalGuest || s_mwlState.status != MwlStatus_Class3) {
		return false;
	}

	// Ensure we can deauthenticate
	if (!_mwlSetMlmeState(MwlMlmeState_Preparing)) {
		return false;
	}

	// Send deauthentication packet
	NetBuf* pPacket = _mwlMgmtMakeDeauth(s_mwlState.bssid, 36);
	if (pPacket) {
		mwlDevTx(1, pPacket, _mwlMlmeDeauthTxCb, NULL);
		return true;
	}

	return false;
}
