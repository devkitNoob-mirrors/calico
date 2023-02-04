#include <calico/types.h>
#include <calico/nds/pxi.h>
#include <calico/nds/pm.h>
#include "../pxi/pm.h"

unsigned pmGetBatteryState(void)
{
	u32 msg = pxiPmMakeMsg(PxiPmMsg_GetBatteryState, 0);
	return pxiSendAndReceive(PxiChannel_Power, msg);
}
