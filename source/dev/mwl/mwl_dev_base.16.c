#include "common.h"
#include <calico/system/thread.h>

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

void mwlDevShutdown(void)
{
}
