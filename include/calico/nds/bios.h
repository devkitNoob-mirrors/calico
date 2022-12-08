#pragma once
#include "../types.h"

// Parameters for svcCpuSet
#define SVC_SET_SIZE_16(_sz) (((_sz)/2) & 0x1fffff)
#define SVC_SET_SIZE_32(_sz) (((_sz)/4) & 0x1fffff)
#define SVC_SET_FIXED        (1<<24)
#define SVC_SET_UNIT_16      (0<<26)
#define SVC_SET_UNIT_32      (1<<26)

/* TODO: ABI. This is intended to be returned as r0/r1
typedef struct SvcDivResult {
	s32 quotient;
	s32 remainder;
} SvcDivResult;
*/

typedef struct SvcBitUnpackParams {
	u16 in_length_bytes;
	u8  in_width_bits;
	u8  out_width_bits;
	u32 data_offset    : 31;
	u32 zero_data_flag : 1;
} SvcBitUnpackParams;

void svcSoftReset(void) MEOW_NORETURN;
void svcWaitByLoop(s32 loop_cycles);
void svcHalt(void);
#ifdef ARM7
void svcSleep(void);
void svcSoundBias(bool enable, u32 delay_count);
#endif
//SvcDivResult svcDiv(s32 num, s32 den);
void svcCpuSet(const void* src, void* dst, u32 mode);
u16 svcSqrt(u32 num);
u16 svcGetCRC16(u16 initial_crc, const void* mem, u32 size);
void svcBitUnpack(const void* src, void* dst, SvcBitUnpackParams const* params);
void svcLZ77UncompWram(const void* src, void* dst);
void svcRLUncompWram(const void* src, void* dst);
void svcDiff8bitUnfilterWram(const void* src, void* dst);
void svcDiff16bitUnfilter(const void* src, void* dst);
