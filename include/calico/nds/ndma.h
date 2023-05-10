#pragma once
#include "../types.h"
#include "../system/sysclock.h"

#if defined(__NDS__)
#include "../nds/io.h"
#else
#error "This header file is only for DSi"
#endif

// Explanation of the three sizes that affect a NDMA operation:
// 1) Block size (REG_NDMAxCNT.NDMA_BLK_WORDS(x)):
//      Size of the atomic transfer unit in words. Usually corresponds to
//      FIFO size (e.g. AES has 0x10 words, SDMMC has 512 bytes = 0x80 words)
// 2) Single transfer size (REG_NDMAxWCNT):
//      Size of a single transfer in words. Must be a multiple of block size.
// 3) Total operation size (REG_NDMAxTCNT):
//      Total desired size of the operation in words. Only used by NdmaTxMode_Timing.
//      The operation is split up into multiple transfers. The last one
//      may be smaller in order to copy the remaining data.

#define REG_NDMAGCNT       MEOW_REG(u32, IO_NDMAGCNT)
#define REG_NDMAxSAD(_x)   MEOW_REG(u32, IO_NDMAxSAD(_x))
#define REG_NDMAxDAD(_x)   MEOW_REG(u32, IO_NDMAxDAD(_x))
#define REG_NDMAxTCNT(_x)  MEOW_REG(u32, IO_NDMAxTCNT(_x))
#define REG_NDMAxWCNT(_x)  MEOW_REG(u32, IO_NDMAxWCNT(_x))
#define REG_NDMAxBCNT(_x)  MEOW_REG(u32, IO_NDMAxBCNT(_x))
#define REG_NDMAxFDATA(_x) MEOW_REG(u32, IO_NDMAxFDATA(_x))
#define REG_NDMAxCNT(_x)   MEOW_REG(u32, IO_NDMAxCNT(_x))

#define NDMA_G_RR_CYCLES(_x) ((__builtin_ffs(_x)&0xf)<<16)
#define NDMA_G_FIXED_PRIO    (0<<31)
#define NDMA_G_ROUND_ROBIN   (1<<31)

#define NDMA_B_PRESCALER_1  (0<<16)
#define NDMA_B_PRESCALER_4  (1<<16)
#define NDMA_B_PRESCALER_16 (2<<16)
#define NDMA_B_PRESCALER_64 (3<<16)

#define NDMA_DST_MODE(_x)  (((_x)&3)<<10)
#define NDMA_DST_RELOAD    (1<<12)
#define NDMA_SRC_MODE(_x)  (((_x)&3)<<13)
#define NDMA_SRC_RELOAD    (1<<15)
#define NDMA_BLK_WORDS(_x) (((__builtin_ffs(_x)-1)&0xf)<<16)
#define NDMA_TIMING(_x)    (((_x)&0xf)<<24)
#define NDMA_TX_MODE(_x)   (((_x)&3)<<28)
#define NDMA_IRQ_ENABLE    (1<<30)
#define NDMA_START         (1<<31)

MEOW_EXTERN_C_START

typedef enum NdmaMode {
	NdmaMode_Increment = 0,
	NdmaMode_Decrement = 1,
	NdmaMode_Fixed     = 2,
	NdmaMode_FillData  = 3,
} NdmaMode;

typedef enum NdmaTxMode {
	NdmaTxMode_Timing       = 0, // multiple transfers (of size REG_NDMAxWCNT) until reaching total size (REG_NDMAxTCNT), last tx may be smaller
	NdmaTxMode_Immediate    = 1, // only a single transfer (of size REG_NDMAxWCNT)
	NdmaTxMode_TimingRepeat = 2, // indefinite number of transfers (of size REG_NDMAxWCNT)
} NdmaTxMode;

typedef enum NdmaTiming {
	NdmaTiming_Timer0    = 0,
	NdmaTiming_Timer1    = 1,
	NdmaTiming_Timer2    = 2,
	NdmaTiming_Timer3    = 3,
	NdmaTiming_Slot1     = 4,
	NdmaTiming_Slot1_2   = 5, // DSi prototype
	NdmaTiming_VBlank    = 6,
#if defined(ARM7)
	NdmaTiming_Mitsumi   = 7, // aka DS Wireless
	NdmaTiming_Tmio0     = 8, // connected to SD/eMMC
	NdmaTiming_Tmio1     = 9, // connected to SDIO (Atheros Wireless)
	NdmaTiming_AesWrFifo = 10,
	NdmaTiming_AesRdFifo = 11,
	NdmaTiming_MicData   = 12,
#elif defined(ARM9)
	NdmaTiming_HBlank    = 7,
	NdmaTiming_DispStart = 8,
	NdmaTiming_MemDisp   = 9,
	NdmaTiming_3dFifo    = 10,
	NdmaTiming_Camera    = 11,
#endif
} NdmaTiming;

MEOW_CONSTEXPR u32 ndmaCalcBlockTimer(unsigned prescaler, unsigned freq)
{
	unsigned basefreq = SYSTEM_CLOCK;
	basefreq >>= ((prescaler>>16)&2) * 2;
	unsigned interval = (basefreq + freq/2) / freq;
	if (interval < 1) interval = 1;
	else if (interval > 0xffff) interval = 0xffff;
	return interval | prescaler;
}

MEOW_INLINE bool ndmaIsBusy(unsigned id)
{
	return REG_NDMAxCNT(id) & NDMA_START;
}

MEOW_INLINE void ndmaBusyWait(unsigned id)
{
	while (ndmaIsBusy(id));
}

MEOW_INLINE void ndmaStartCopy32(unsigned id, void* dst, const void* src, size_t size)
{
	REG_NDMAxSAD(id) = (u32)src;
	REG_NDMAxDAD(id) = (u32)dst;
	REG_NDMAxBCNT(id) = 0;
	REG_NDMAxWCNT(id) = size/4;
	REG_NDMAxCNT(id) =
		NDMA_DST_MODE(NdmaMode_Increment) |
		NDMA_SRC_MODE(NdmaMode_Increment) |
		NDMA_BLK_WORDS(1) |
		NDMA_TX_MODE(NdmaTxMode_Immediate) |
		NDMA_START;
}

MEOW_INLINE void ndmaStartFill32(unsigned id, void* dst, u32 value, size_t size)
{
	REG_NDMAxFDATA(id) = value;
	REG_NDMAxDAD(id) = (u32)dst;
	REG_NDMAxBCNT(id) = 0;
	REG_NDMAxWCNT(id) = size/4;
	REG_NDMAxCNT(id) =
		NDMA_DST_MODE(NdmaMode_Increment) |
		NDMA_SRC_MODE(NdmaMode_FillData) |
		NDMA_BLK_WORDS(1) |
		NDMA_TX_MODE(NdmaTxMode_Immediate) |
		NDMA_START;
}

MEOW_EXTERN_C_END
