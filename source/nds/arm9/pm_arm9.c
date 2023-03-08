#include <calico/types.h>
#include <calico/arm/cache.h>
#include <calico/nds/pxi.h>
#include <calico/nds/pm.h>
#include "../pxi/pm.h"

unsigned pmGetBatteryState(void)
{
	u32 msg = pxiPmMakeMsg(PxiPmMsg_GetBatteryState, 0);
	return pxiSendAndReceive(PxiChannel_Power, msg);
}

bool pmReadNvram(void* data, u32 addr, u32 len)
{
	u32 msg = pxiPmMakeMsg(PxiPmMsg_ReadNvram, 0);
	u32 args[] = {
		(u32)data,
		addr,
		len,
	};

	armDCacheFlush(data, len);
	return pxiSendWithDataAndReceive(PxiChannel_Power, msg, args, 3);
}
