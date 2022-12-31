#pragma once
#include <calico/types.h>
#include <calico/dev/sdio.h>
#include <calico/dev/ar6k.h>

#define AR6K_DEBUG

#ifdef AR6K_DEBUG
#include <calico/system/dietprint.h>
#else
#define dietPrint(...) ((void)0)
#endif

typedef struct Ar6kIrqProcRegs {
	u8 host_int_status;
	u8 cpu_int_status;
	u8 error_int_status;
	u8 counter_int_status;
	u8 mbox_frame;
	u8 rx_lookahead_valid;
	u8 _pad[2];
	u32 rx_lookahead[2];
} Ar6kIrqProcRegs;

typedef struct Ar6kIrqEnableRegs {
	u8 int_status_enable;
	u8 cpu_int_status_enable;
	u8 error_status_enable;
	u8 counter_int_status_enable;
} Ar6kIrqEnableRegs;

// Internal API
bool _ar6kDevPollMboxMsgRecv(Ar6kDev* dev, u32* lookahead, unsigned attempts);
bool _ar6kDevSendPacket(Ar6kDev* dev, const void* pktmem, size_t pktsize);
bool _ar6kDevRecvPacket(Ar6kDev* dev, void* pktmem, size_t pktsize);
