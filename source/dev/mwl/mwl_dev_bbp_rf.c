// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#include "common.h"

MK_INLINE void _mwlBbpBusyWait(void)
{
	while (MWL_REG(W_BB_BUSY) & 1);
}

MK_INLINE void _mwlRfBusyWait(void)
{
	while (MWL_REG(W_RF_BUSY) & 1);
}

unsigned _mwlBbpRead(unsigned reg)
{
	MWL_REG(W_BB_CNT) = 0x6000 | reg;
	_mwlBbpBusyWait();
	unsigned value = MWL_REG(W_BB_READ);
	//dietPrint("[MWL] rd BBP[0x%.2x] = 0x%.2x\n", reg, value);
	return value;
}

void _mwlBbpWrite(unsigned reg, unsigned value)
{
	MWL_REG(W_BB_WRITE) = value;
	MWL_REG(W_BB_CNT) = 0x5000 | reg;
	_mwlBbpBusyWait();
	//dietPrint("[MWL] wr BBP[0x%.2x] = 0x%.2x\n", reg, value);
}

void _mwlRfCmd(u32 cmd)
{
	MWL_REG(W_RF_DATA1) = cmd;
	MWL_REG(W_RF_DATA2) = cmd>>16;
	_mwlRfBusyWait();
	//dietPrint("[MWL] RF cmd %.8lx\n", cmd);
}
