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

static struct {
	Mutex mutex;
	u32 romcnt;
	u32 cache_pos;

	bool first_init;
	NtrCardMode mode;
} s_ntrcardState;

alignas(CACHE_ALIGN) u8 s_ntrcardCache[NTRCARD_SECTOR_SZ];

MK_INLINE bool _ntrcardIsOpenByArm9(void)
{
	return (REG_EXMEMCNT & EXMEMCNT_NDS_SLOT_ARM7) == 0;
}

MK_INLINE bool _ntrcardIsOpenByArm7(void)
{
	return (s_transferRegion->exmemcnt_mirror & EXMEMCNT_NDS_SLOT_ARM7) != 0;
}

MK_INLINE bool _ntrcardIsInitOrMainMode(void)
{
	return s_ntrcardState.mode == NtrCardMode_Init || s_ntrcardState.mode == NtrCardMode_Main;
}

MK_INLINE NtrCardMode _ntrcardInitOrMainCmdImpl(NtrCardMode init_cmd, NtrCardMode main_cmd)
{
	return s_ntrcardState.mode == NtrCardMode_Main ? main_cmd : init_cmd;
}

#define _ntrcardInitOrMainCmd(_cmd) _ntrcardInitOrMainCmdImpl(NtrCardCmd_Init##_cmd, NtrCardCmd_Main##_cmd)

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

	s_ntrcardState.cache_pos = UINT32_MAX;

	if (!s_ntrcardState.first_init) {
		s_ntrcardState.first_init = true;

		if (g_envBootParam->boot_src == EnvBootSrc_Card) {
			// If we are booted from Slot-1, assume the card is in Main mode
			// (this is true for both flashcards and emulators)
			s_ntrcardState.mode = NtrCardMode_Main;
			s_ntrcardState.romcnt = g_envAppNdsHeader->cardcnt_normal;
			s_ntrcardState.romcnt &= ~NTRCARD_ROMCNT_BLK_SIZE(7);
			s_ntrcardState.romcnt |= NTRCARD_ROMCNT_START | NTRCARD_ROMCNT_NO_RESET;
		} else {
			// Otherwise, make no assumptions - leave card mode undefined
			s_ntrcardState.mode = NtrCardMode_None;
			s_ntrcardState.romcnt = 0;
		}
	}

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
	REG_DMAxCNT_L(dma_ch) = 1;
	REG_DMAxCNT_H(dma_ch) =
		DMA_MODE_DST(DmaMode_Increment) |
		DMA_MODE_SRC(DmaMode_Fixed) |
		DMA_MODE_REPEAT |
		DMA_UNIT_32 |
		DMA_TIMING(DmaTiming_Slot1) |
		DMA_START;

	REG_NTRCARD_ROMCNT = romcnt;

	while (REG_NTRCARD_ROMCNT & NTRCARD_ROMCNT_BUSY) {
		threadIrqWait(false, IRQ_SLOT1_TX);
	}

	REG_DMAxCNT_H(dma_ch) = 0;
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

MK_NOINLINE static void _ntrcardGetChipId(NtrChipId* out)
{
	while (REG_NTRCARD_ROMCNT & NTRCARD_ROMCNT_BUSY);
	REG_NTRCARD_CNT = NTRCARD_CNT_MODE_ROM | NTRCARD_CNT_TX_IE | NTRCARD_CNT_ENABLE;
	REG_NTRCARD_ROMCMD_HI = __builtin_bswap32(_ntrcardInitOrMainCmd(GetChipId) << 24);
	REG_NTRCARD_ROMCMD_LO = 0;

	u32 romcnt = s_ntrcardState.romcnt | NTRCARD_ROMCNT_BLK_SIZE(NtrCardBlkSize_4);
	_ntrcardRecvByCpu(romcnt, out);
}

MK_NOINLINE static void _ntrcardRomReadSector(int dma_ch, u32 offset, void* buf)
{
	while (REG_NTRCARD_ROMCNT & NTRCARD_ROMCNT_BUSY);
	REG_NTRCARD_CNT = NTRCARD_CNT_MODE_ROM | NTRCARD_CNT_TX_IE | NTRCARD_CNT_ENABLE;
	REG_NTRCARD_ROMCMD_HI = __builtin_bswap32((offset >> 8) | (_ntrcardInitOrMainCmd(RomRead) << 24));
	REG_NTRCARD_ROMCMD_LO = __builtin_bswap32(offset << 24);

	u32 romcnt = s_ntrcardState.romcnt | NTRCARD_ROMCNT_BLK_SIZE(NtrCardBlkSize_Sector);

	if (dma_ch >= 0) {
		_ntrcardRecvByDma(romcnt, buf, dma_ch & 3, NTRCARD_SECTOR_SZ);
	} else {
		_ntrcardRecvByCpu(romcnt, buf);
	}
}

bool ntrcardOpen(void)
{
	mutexLock(&s_ntrcardState.mutex);
	bool rc = _ntrcardOpen();
	mutexUnlock(&s_ntrcardState.mutex);
	return rc;
}

void ntrcardClose(void)
{
	mutexLock(&s_ntrcardState.mutex);
	_ntrcardClose();
	mutexUnlock(&s_ntrcardState.mutex);
}

NtrCardMode ntrcardGetMode(void)
{
	return s_ntrcardState.mode;
}

MK_INLINE bool _ntrcardIsAligned(int dma_ch, uptr addr)
{
	unsigned align = 4;
#ifdef ARM9
	if (dma_ch >= 0) {
		align = ARM_CACHE_LINE_SZ;
	}
#endif
	return (addr & (align-1)) == 0;
}

MK_INLINE unsigned _ntrcardCanAccess(int dma_ch, uptr addr)
{
	bool ret = _ntrcardIsAligned(dma_ch, addr);
#ifdef ARM9
	if (ret && dma_ch >= 0) {
		ret = !(addr < MM_MAINRAM || (addr >= MM_DTCM && addr < MM_TWLWRAM_MAP));
	}
#endif
	return ret;
}

bool ntrcardGetChipId(NtrChipId* out)
{
	mutexLock(&s_ntrcardState.mutex);

	if_unlikely (!_ntrcardIsOpenBySelf() || !_ntrcardIsInitOrMainMode()) {
		mutexUnlock(&s_ntrcardState.mutex);
		return false;
	}

	_ntrcardGetChipId(out);
	mutexUnlock(&s_ntrcardState.mutex);

	return true;
}

bool ntrcardRomReadSector(int dma_ch, u32 offset, void* buf)
{
	if (!_ntrcardCanAccess(dma_ch, (uptr)buf)) {
		return false;
	}

	mutexLock(&s_ntrcardState.mutex);

	if_unlikely (!_ntrcardIsOpenBySelf() || !_ntrcardIsInitOrMainMode()) {
		mutexUnlock(&s_ntrcardState.mutex);
		return false;
	}

	_ntrcardRomReadSector(dma_ch, offset, buf);
	mutexUnlock(&s_ntrcardState.mutex);

	return true;
}

bool ntrcardRomRead(int dma_ch, u32 offset, void* buf, u32 size)
{
	u8* out = (u8*)buf;

	mutexLock(&s_ntrcardState.mutex);

	if_unlikely (!_ntrcardIsOpenBySelf() || !_ntrcardIsInitOrMainMode()) {
		mutexUnlock(&s_ntrcardState.mutex);
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
		} else if (cur_sector == s_ntrcardState.cache_pos || !_ntrcardCanAccess(dma_ch, (uptr)out)) {
			copy_src = s_ntrcardCache;
		} else {
			cur_buf = out;
		}

		if (cur_buf != s_ntrcardCache || s_ntrcardState.cache_pos != cur_sector) {
			_ntrcardRomReadSector(dma_ch, cur_sector, cur_buf);
		}

		if (copy_src) {
			s_ntrcardState.cache_pos = cur_sector;
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

	mutexUnlock(&s_ntrcardState.mutex);
	return true;
}
