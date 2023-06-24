#include "common.h"
#include <calico/system/thread.h>

typedef struct _MwlMacInitReg {
	u16 reg;
	u16 val;
} _MwlMacInitReg;

static u32 s_mwlRfBkReg;

static const u16 s_mwlRfMacRegs[] = {
	0x146,
	0x148,
	0x14a,
	0x14c,
	0x120,
	0x122,
	0x154,
	0x144,
	0x132,
	0x132,
	0x140,
	0x142,
	W_POWER_TX,
	0x124,
	0x128,
	0x150,
};

static const _MwlMacInitReg s_mwlMacInitRegs[] = {
	{ W_MODE_RST,           0 },
	{ W_TXSTATCNT,          0 },
	{ 0x00a,                0 },
	{ W_IE,                 0 },
	{ W_IF,            0xffff },
	{ 0x254,                0 },
	{ W_TXBUF_RESET,   0xffff },
	{ W_TXBUF_BEACON,       0 },
	{ W_LISTENINT,          1 },
	{ W_LISTENCOUNT,        0 },
	{ W_AID_FULL,           0 },
	{ W_AID_LOW,            0 },
	{ W_US_COUNTCNT,        0 },
	{ W_US_COMPARECNT,      0 },
	{ W_CMD_COUNTCNT,       1 },
	{ 0x0ec,           0x3f03 },
	{ 0x1a2,                1 },
	{ 0x1a0,                0 },
	{ W_PRE_BEACON,     0x800 },
	{ W_PREAMBLE,           1 },
	{ 0x0d4,                3 },
	{ 0x0d8,                4 },
	{ W_RX_LEN_CROP,    0x602 },
	{ W_TXBUF_GAPDISP,      0 },
	{ 0x130,            0x146 },
};

static void _mwlRfInit(void)
{
	MwlCalibData* calib = mwlGetCalibData();

	for (unsigned i = 0; i < sizeof(s_mwlRfMacRegs)/sizeof(s_mwlRfMacRegs[0]); i ++) {
		//dietPrint("[MWL] %3X = %4X\n", s_mwlRfMacRegs[i], calib->mac_reg_init[i]);
		MWL_REG(s_mwlRfMacRegs[i]) = calib->mac_reg_init[i];
	}

	unsigned bits = calib->rf_entry_bits & 0x7f;
	unsigned xtraflag = (calib->rf_entry_bits & 0x80) != 0;
	//dietPrint("[MWL] RF bits %u xtra %u\n", bits, xtraflag);
	MWL_REG(W_RF_CNT) = bits | (xtraflag<<8);

	unsigned num_entries = calib->rf_num_entries;

	if (calib->rf_type == 3) {
		// DS Lite and up
		for (unsigned i = 0; i < num_entries; i ++) {
			_mwlRfCmd((5U<<16) | (i<<8) | calib->rf_entries[i]);
		}
	} else /* if (calib->rf_type == 2) */ {
		// DS Phat (XX: and possibly early Japanese DS Lites such as mine?!)
		unsigned bytes = (bits + 7) / 8;
		for (unsigned i = 0; i < num_entries; i ++) {
			u32 cmd = 0;
			__builtin_memcpy(&cmd, &calib->rf_entries[i*bytes], bytes);
			_mwlRfCmd(cmd);

			if ((cmd>>18) == 9) {
				s_mwlRfBkReg = cmd &~ 0x7c00;
				dietPrint("[MWL] RF BkReg=%.8lX\n", s_mwlRfBkReg);
			}
		}
	}
}

static void _mwlBbpInit(void)
{
	MwlCalibData* calib = mwlGetCalibData();
	for (unsigned i = 0; i < sizeof(calib->bb_reg_init); i ++) {
		_mwlBbpWrite(i, calib->bb_reg_init[i]);
	}
}

static void _mwlMacInit(void)
{
	for (unsigned i = 0; i < sizeof(s_mwlMacInitRegs)/sizeof(s_mwlMacInitRegs[0]); i ++) {
		const _MwlMacInitReg* r = &s_mwlMacInitRegs[i];
		MWL_REG(r->reg) = r->val;
	}
}

static void _mwlMacDefaults(void)
{
	MwlCalibData* calib = mwlGetCalibData();
	MWL_REG(W_MACADDR_0) = calib->mac_addr[0];
	MWL_REG(W_MACADDR_1) = calib->mac_addr[1];
	MWL_REG(W_MACADDR_2) = calib->mac_addr[2];
	MWL_REG(W_TX_RETRYLIMIT) = 7;
	MWL_REG(W_MODE_WEP) = MwlMode_LocalGuest | (1U<<3) | (1U<<6);
	MWL_REG(0x290) = 0; // Not using 2nd antenna

	_mwlBbpWrite(0x13, 0);    // CCA operation
	_mwlBbpWrite(0x35, 0x1f); // ED (Energy Detection) threshold

	// XX: Stuff below related to host mode/beacon tx. Probably unnecessary
	MWL_REG(W_BEACONINT) = 500;
	MWL_REG(W_PREAMBLE) |= 6; // short preamble
}

void mwlDevWakeUp(void)
{
	MWL_REG(W_POWER_US) = 0;
	threadSleep(8000);
	MWL_REG(W_BB_POWER) = 0;
	if (mwlGetCalibData()->rf_type == 2) {
		unsigned regval = _mwlBbpRead(1);
		_mwlBbpWrite(1, regval &~ 0x80);
		_mwlBbpWrite(1, regval);
		threadSleep(40000);
	}
	_mwlRfInit();
}

void mwlDevReset(void)
{
	_mwlMacInit();
	_mwlRfInit();
	_mwlBbpInit();
	_mwlMacDefaults();
}

void mwlDevSetChannel(unsigned ch)
{
	MwlCalibData* calib = mwlGetCalibData();
	if (!(calib->enabled_ch_mask & (1U << ch))) {
		return;
	}

	ch --;
	if (ch >= 14) {
		return;
	}

	unsigned powerforce_backup = MWL_REG(W_POWERFORCE);
	MWL_REG(W_POWERFORCE) = 0x8001; // force sleep?
	unsigned powerstate, rf_status;
	do {
		powerstate = MWL_REG(W_POWERSTATE);
		rf_status = MWL_REG(W_RF_STATUS);
	} while ((powerstate>>8) != 2 || (rf_status != 0 && rf_status != 9));

	if (calib->rf_type == 3) {
		MwlChanCalibV3* v3 = (MwlChanCalibV3*)&calib->rf_entries[calib->rf_num_entries];

		unsigned bb_num_regs = v3->bb_num_regs;
		unsigned rf_num_regs = calib->rf_num_regs;

		MwlChanRegV3* bb_regs = &v3->regs[0];
		MwlChanRegV3* rf_regs = &v3->regs[bb_num_regs];

		for (unsigned i = 0; i < bb_num_regs; i ++) {
			_mwlBbpWrite(bb_regs[i].index, bb_regs[i].values[ch]);
		}

		for (unsigned i = 0; i < rf_num_regs; i ++) {
			_mwlRfCmd(rf_regs[i].values[ch] | (rf_regs[i].index<<8) | (5U<<16));
		}
	} else /* if (calib->rf_type == 2) */ {
		unsigned bytes = ((calib->rf_entry_bits&0x7f) + 7) / 8;
		MwlChanCalibV2* v2 = (MwlChanCalibV2*)&calib->rf_entries[bytes*calib->rf_num_entries];

		unsigned rf5 = 0, rf6 = 0;
		__builtin_memcpy(&rf5, v2->rf_regs[ch].reg_0x05, bytes);
		__builtin_memcpy(&rf6, v2->rf_regs[ch].reg_0x06, bytes);

		_mwlRfCmd(rf5);
		_mwlRfCmd(rf6);

		if (s_mwlRfBkReg & 0x10000) {
			if (!(s_mwlRfBkReg & 0x8000)) {
				_mwlRfCmd(s_mwlRfBkReg | ((v2->rf_0x09_bits[ch]&0x1f)<<10));
			}
		} else {
			_mwlBbpWrite(0x1e, v2->bb_0x1e_values[ch]);
		}
	}

	MWL_REG(W_POWERFORCE) = powerforce_backup;
	MWL_REG(0x048) = 3;

	dietPrint("[MWL] Switched to ch%2u\n", ch+1);
}

void mwlDevSetMode(MwlMode mode)
{
	if (s_mwlState.status > MwlStatus_Idle) {
		return;
	}

	s_mwlState.mode = mode;

	switch (s_mwlState.mode) {
		default: return;

		case MwlMode_LocalHost:
			s_mwlState.rx_pos = 0x10c4;
			s_mwlState.tx_pos[0] = 0xaa0;
			s_mwlState.tx_pos[1] = 0x958;
			s_mwlState.tx_pos[2] = 0x334;
			s_mwlState.tx_beacon_pos = 0x238;
			s_mwlState.tx_cmd_pos = 0;
			break;

		case MwlMode_LocalGuest:
			s_mwlState.rx_pos = 0xbfc;
			s_mwlState.tx_pos[0] = 0x5d8;
			s_mwlState.tx_pos[1] = 0x490;
			s_mwlState.tx_pos[2] = 0x468;
			s_mwlState.tx_reply_pos[0] = 0x234;
			s_mwlState.tx_reply_pos[1] = 0;
			break;

		case MwlMode_Infra:
			s_mwlState.rx_pos = 0x794;
			s_mwlState.tx_pos[0] = 0x170;
			s_mwlState.tx_pos[1] = 0x28;
			s_mwlState.tx_pos[2] = 0;
			break;
	}

	dietPrint("[MWL] RX buf sz = 0x%x\n", MWL_MAC_RAM_SZ - s_mwlState.rx_pos);
}

void mwlDevSetBssid(const void* bssid)
{
	__builtin_memcpy(s_mwlState.bssid, bssid, 6);

	if (s_mwlState.status > MwlStatus_Idle) {
		// Update hardware registers too
		MWL_REG(W_BSSID_0) = s_mwlState.bssid[0];
		MWL_REG(W_BSSID_1) = s_mwlState.bssid[1];
		MWL_REG(W_BSSID_2) = s_mwlState.bssid[2];

		// Enable receiving "all" beacons and not just BSSID's beacon when we have a multicast (i.e. not real) BSSID?
		if (s_mwlState.bssid[0] & 1) {
			MWL_REG(W_RXFILTER) |= 0x400;
		} else {
			MWL_REG(W_RXFILTER) &= ~0x400;
		}
	}
}

void mwlDevShutdown(void)
{
	if (mwlGetCalibData()->rf_type == 2) {
		_mwlRfCmd(0xc008);
	}
	_mwlBbpWrite(0x1e, _mwlBbpRead(0x1e) | 0x3f);
	MWL_REG(W_BB_POWER) = 0x800d;
	MWL_REG(W_POWER_US) = 1;
}
