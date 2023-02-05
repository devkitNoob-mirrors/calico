#include <calico/types.h>
#include <calico/system/irq.h>
#include <calico/system/thread.h>
#include <calico/nds/system.h>
#include <calico/nds/env.h>
#include <calico/nds/lcd.h>
#include <calico/nds/touch.h>
#include <calico/nds/arm7/codec.h>
#include "../transfer.h"

typedef struct TouchState {
	s32 xscale, yscale;
	s32 xoffset, yoffset;
} TouchState;

static TouchState s_touchState;

static Thread s_touchSrvThread;
alignas(8) static u8 s_touchSrvThreadStack[0x100];

static void _touchCalcPos(TouchData* data)
{
	int px = (data->rawx*s_touchState.xscale - s_touchState.xoffset + s_touchState.xscale/2)>>19;
	int py = (data->rawy*s_touchState.yscale - s_touchState.yoffset + s_touchState.yscale/2)>>19;

	if (px < 0) px = 0;
	else if (px > LCD_WIDTH-1) px = LCD_WIDTH-1;
	if (py < 0) py = 0;
	else if (py > LCD_HEIGHT-1) py = LCD_HEIGHT-1;

	data->px = px;
	data->py = py;
}

static int _touchSrvThreadMain(void* arg)
{
	// Configure VCount interrupt
	lcdSetVCountCompare(true, (unsigned)arg);
	irqEnable(IRQ_VCOUNT);

	for (;;) {
		threadIrqWait(false, IRQ_VCOUNT);

		TouchData data;
		bool valid = touchRead(&data);
		u16 state = s_transferRegion->touch_state ^ TOUCH_BUF;

		if (valid) {
			state |= TOUCH_VALID;

			// Copy data into transfer region
			TouchData* out = (TouchData*)s_transferRegion->touch_data[state & TOUCH_BUF];
			*out = data;
		} else {
			state &= ~TOUCH_VALID;
		}

		// Update state in transfer region
		s_transferRegion->touch_state = state;
	}

	return 0;
}

void touchInit(void)
{
	touchLoadCalibration();

	spiLock();
	if (cdcIsTwlMode()) {
		cdcTscInit();
	} else {
		// TODO: NDS mode TSC
	}
	spiUnlock();
}

void touchStartServer(unsigned lyc, u8 thread_prio)
{
	threadPrepare(&s_touchSrvThread, _touchSrvThreadMain, (void*)lyc, &s_touchSrvThreadStack[sizeof(s_touchSrvThreadStack)], thread_prio);
	threadStart(&s_touchSrvThread);
}

void touchLoadCalibration(void)
{
#define CALIB g_envUserSettings->touch_calib
	s_touchState.xscale = ((CALIB.lcd_x2 - CALIB.lcd_x1) << 19) / (CALIB.raw_x2 - CALIB.raw_x1);
	s_touchState.yscale = ((CALIB.lcd_y2 - CALIB.lcd_y1) << 19) / (CALIB.raw_y2 - CALIB.raw_y1);
	s_touchState.xoffset = ((CALIB.raw_x1 + CALIB.raw_x2) * s_touchState.xscale - ((CALIB.lcd_x1 + CALIB.lcd_x2) << 19)) / 2;
	s_touchState.yoffset = ((CALIB.raw_y1 + CALIB.raw_y2) * s_touchState.yscale - ((CALIB.lcd_y1 + CALIB.lcd_y2) << 19)) / 2;
#undef CALIB
}

bool touchRead(TouchData* out)
{
	bool ok;

	spiLock();
	if (cdcIsTwlMode()) {
		CdcTscBuffer buf;
		ok = cdcTscReadBuffer(&buf);

		if (ok) {
			out->rawx = buf.x;
			out->rawy = buf.y;
		}
	} else {
		// TODO: NDS mode TSC
		ok = false;
	}
	spiUnlock();

	if (ok) {
		_touchCalcPos(out);
	}

	return ok;
}
