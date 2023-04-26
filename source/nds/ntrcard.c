#include <string.h>
#include <calico/types.h>
#if defined(ARM9)
#include <calico/arm/cache.h>
#endif
#include <calico/system/irq.h>
#include <calico/system/thread.h>
#include <calico/system/mutex.h>
#include <calico/nds/system.h>
#include <calico/nds/dma.h>
#include <calico/nds/ntrcard.h>
#include "transfer.h"

#if defined(ARM9)
#define CACHE_ALIGN ARM_CACHE_LINE_SZ
#elif defined(ARM7)
#define CACHE_ALIGN 4
#endif

static Mutex s_ntrcardMutex;
static u32 s_ntrcardNormalCnt;

static u32 s_ntrcardCachePos;
alignas(CACHE_ALIGN) u8 s_ntrcardCache[NTRCARD_SECTOR_SZ];

MEOW_INLINE bool _ntrcardIsOpenByArm9(void)
{
	return (REG_EXMEMCNT & EXMEMCNT_NDS_SLOT_ARM7) == 0;
}

MEOW_INLINE bool _ntrcardIsOpenByArm7(void)
{
	return (s_transferRegion->exmemcnt_mirror & EXMEMCNT_NDS_SLOT_ARM7) != 0;
}

#if defined(ARM9)
#define _ntrcardIsOpenBySelf  _ntrcardIsOpenByArm9
#define _ntrcardIsOpenByOther _ntrcardIsOpenByArm7
#elif defined(ARM7)
#define _ntrcardIsOpenBySelf  _ntrcardIsOpenByArm7
#define _ntrcardIsOpenByOther _ntrcardIsOpenByArm9
#endif

static bool _ntrcardOpen(void)
{
	if (_ntrcardIsOpenBySelf()) {
		return true;
	}

	if (_ntrcardIsOpenByOther()) {
		return false;
	}

#if defined(ARM9)
	REG_EXMEMCNT &= ~EXMEMCNT_NDS_SLOT_ARM7;
#elif defined(ARM7)
	s_transferRegion->exmemcnt_mirror |= EXMEMCNT_NDS_SLOT_ARM7;
#endif

	irqEnable(IRQ_SLOT1_TX);

	s_ntrcardNormalCnt = g_envAppNdsHeader->cardcnt_normal;
	s_ntrcardNormalCnt &= ~NTRCARD_ROMCNT_BLK_SIZE(7);
	s_ntrcardNormalCnt |= NTRCARD_ROMCNT_START | NTRCARD_ROMCNT_NO_RESET;
	s_ntrcardCachePos = UINT32_MAX;

	return true;
}

static void _ntrcardClose(void)
{
	if (!_ntrcardIsOpenBySelf()) {
		return;
	}

	irqDisable(IRQ_SLOT1_TX);
	REG_NTRCARD_CNT = 0;

#if defined(ARM9)
	REG_EXMEMCNT |= EXMEMCNT_NDS_SLOT_ARM7;
#elif defined(ARM7)
	s_transferRegion->exmemcnt_mirror &= ~EXMEMCNT_NDS_SLOT_ARM7;
#endif
}

static void _ntrcardRecvByDma(u32 romcnt, void* buf, unsigned dma_ch, u32 size)
{
	dmaBusyWait(dma_ch);
#ifdef ARM9
	armDCacheInvalidate(buf, size);
#endif
	REG_DMAxSAD(dma_ch) = (uptr)&REG_NTRCARD_FIFO;
	REG_DMAxDAD(dma_ch) = (uptr)buf;
	REG_DMAxCNT_L(dma_ch) = size/4;
	REG_DMAxCNT_H(dma_ch) =
		DMA_MODE_DST(DmaMode_Increment) |
		DMA_MODE_SRC(DmaMode_Fixed) |
		DMA_UNIT_32 |
		DMA_TIMING(DmaTiming_Slot1) |
		DMA_ENABLE;

	REG_NTRCARD_ROMCNT = romcnt;

	while (REG_NTRCARD_ROMCNT & NTRCARD_ROMCNT_BUSY) {
		threadIrqWait(false, IRQ_SLOT1_TX);
	}
}

static void _ntrcardRecvByCpu(u32 romcnt, void* buf)
{
	REG_NTRCARD_ROMCNT = romcnt;

	u32* buf32 = (u32*)buf;
	do {
		romcnt = REG_NTRCARD_ROMCNT;
		if (romcnt & NTRCARD_ROMCNT_DATA_READY) {
			*buf32++ = REG_NTRCARD_FIFO;
		}
	} while (romcnt & NTRCARD_ROMCNT_BUSY);
}

MEOW_NOINLINE static void _ntrcardRomReadSector(int dma_ch, u32 offset, void* buf)
{
	while (REG_NTRCARD_ROMCNT & NTRCARD_ROMCNT_BUSY);
	REG_NTRCARD_CNT = NTRCARD_CNT_MODE_ROM | NTRCARD_CNT_TX_IE | NTRCARD_CNT_ENABLE;
	REG_NTRCARD_ROMCMD_HI = __builtin_bswap32((offset >> 8) | (0xb7 << 24));
	REG_NTRCARD_ROMCMD_LO = __builtin_bswap32(offset << 24);

	u32 romcnt = s_ntrcardNormalCnt | NTRCARD_ROMCNT_BLK_SIZE(NtrCardBlkSize_Sector);

	if (dma_ch >= 0) {
		_ntrcardRecvByDma(romcnt, buf, dma_ch & 3, NTRCARD_SECTOR_SZ);
	} else {
		_ntrcardRecvByCpu(romcnt, buf);
	}
}

bool ntrcardOpen(void)
{
	mutexLock(&s_ntrcardMutex);
	bool rc = _ntrcardOpen();
	mutexUnlock(&s_ntrcardMutex);
	return rc;
}

void ntrcardClose(void)
{
	mutexLock(&s_ntrcardMutex);
	_ntrcardClose();
	mutexUnlock(&s_ntrcardMutex);
}

MEOW_INLINE bool _ntrcardIsAligned(int dma_ch, uptr addr)
{
	unsigned align = 4;
#ifdef ARM9
	if (dma_ch >= 0) {
		align = ARM_CACHE_LINE_SZ;
	}
#endif
	return (addr & (align-1)) == 0;
}

MEOW_INLINE unsigned _ntrcardCanAccess(int dma_ch, uptr addr)
{
	bool ret = _ntrcardIsAligned(dma_ch, addr);
#ifdef ARM9
	if (ret && dma_ch >= 0) {
		ret = !(addr < MM_MAINRAM || (addr >= MM_DTCM && addr < MM_TWLWRAM_BANK_SZ));
	}
#endif
	return ret;
}

bool ntrcardRomReadSector(int dma_ch, u32 offset, void* buf)
{
	if (!_ntrcardCanAccess(dma_ch, (uptr)buf)) {
		return false;
	}

	mutexLock(&s_ntrcardMutex);
	_ntrcardRomReadSector(dma_ch, offset, buf);
	mutexUnlock(&s_ntrcardMutex);

	return true;
}

bool ntrcardRomRead(int dma_ch, u32 offset, void* buf, u32 size)
{
	u8* out = (u8*)buf;

	mutexLock(&s_ntrcardMutex);

	if_unlikely (!_ntrcardIsOpenBySelf()) {
		mutexUnlock(&s_ntrcardMutex);
		return false;
	}

	while (size) {
		const void* copy_src = NULL;
		void* cur_buf = s_ntrcardCache;
		u32 cur_size = NTRCARD_SECTOR_SZ;

		u32 cur_sector = offset &~ (NTRCARD_SECTOR_SZ-1);
		u32 sec_offset = offset & (NTRCARD_SECTOR_SZ-1);

		if (sec_offset != 0) {
			cur_size -= sec_offset;
			cur_size = cur_size < size ? cur_size : size;
			copy_src = &s_ntrcardCache[sec_offset];
		} else if (size < cur_size) {
			cur_size = size;
			copy_src = s_ntrcardCache;
		} else if (cur_sector == s_ntrcardCachePos || !_ntrcardCanAccess(dma_ch, (uptr)out)) {
			copy_src = s_ntrcardCache;
		} else {
			cur_buf = out;
		}

		if (cur_buf != s_ntrcardCache || s_ntrcardCachePos != cur_sector) {
			_ntrcardRomReadSector(dma_ch, cur_sector, cur_buf);
		}

		if (copy_src) {
			s_ntrcardCachePos = cur_sector;
			if_likely ((((uptr)copy_src | (uptr)out | cur_size) & 3) == 0) {
				armCopyMem32(out, copy_src, cur_size);
			} else {
				memcpy(out, copy_src, cur_size);
			}
		}

		offset += cur_size;
		out += cur_size;
		size -= cur_size;
	}

	mutexUnlock(&s_ntrcardMutex);
	return true;
}
