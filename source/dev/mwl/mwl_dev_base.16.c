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

void mwlDevShutdown(void)
{
	if (mwlGetCalibData()->rf_type == 2) {
		_mwlRfCmd(0xc008);
	}
	_mwlBbpWrite(0x1e, _mwlBbpRead(0x1e) | 0x3f);
	MWL_REG(W_BB_POWER) = 0x800d;
	MWL_REG(W_POWER_US) = 1;
}
