#include <calico/types.h>
#include <calico/system/thread.h>
#include <calico/system/mailbox.h>
#include <calico/dev/blk.h>
#include <calico/nds/system.h>
#include <calico/nds/pxi.h>
#include <calico/nds/arm7/twlblk.h>

#include "../transfer.h"
#include "../pxi/blkdev.h"

static BlkDevCallbackFn s_blkDevCallback;

static bool s_blkHasTwl;

static Mailbox s_blkPxiMailbox;
static u32 s_blkPxiMailboxData[8];
static Thread s_blkPxiThread;
alignas(8) static u8 s_blkPxiThreadStack[1024];

static int _blkPxiThread(void* unused)
{
	for (;;) {
		u32 msg = mailboxRecv(&s_blkPxiMailbox);

		PxiBlkDevMsgType type = pxiBlkDevMsgGetType(msg);
		u32 imm = pxiBlkDevMsgGetImmediate(msg);
		u32 reply = 0;

		switch (type) {
			default: break;

			case PxiBlkDevMsg_IsPresent:
				reply = blkDevIsPresent((BlkDevice)imm);
				break;

			case PxiBlkDevMsg_Init:
				reply = blkDevInit((BlkDevice)imm);
				break;

			case PxiBlkDevMsg_ReadSectors:
			case PxiBlkDevMsg_WriteSectors: {
				void* buffer = (void*)mailboxRecv(&s_blkPxiMailbox);
				u32 first_sector = mailboxRecv(&s_blkPxiMailbox);
				u32 num_sectors = mailboxRecv(&s_blkPxiMailbox);

				if (type == PxiBlkDevMsg_ReadSectors) {
					reply = blkDevReadSectors((BlkDevice)imm, buffer, first_sector, num_sectors);
				} else /* if (type == PxiBlkDevCmd_WriteSectors) */ {
					reply = blkDevWriteSectors((BlkDevice)imm, buffer, first_sector, num_sectors);
				}
				break;
			}
		}

		pxiReply(PxiChannel_BlkDev, reply);
	}

	return 0;
}

void blkInit(void)
{
	if (systemIsTwlMode()) {
		s_blkHasTwl = twlblkInit();
	}

	mailboxPrepare(&s_blkPxiMailbox, s_blkPxiMailboxData, sizeof(s_blkPxiMailboxData)/sizeof(u32));
	pxiSetMailbox(PxiChannel_BlkDev, &s_blkPxiMailbox);
	threadPrepare(&s_blkPxiThread, _blkPxiThread, NULL, &s_blkPxiThreadStack[sizeof(s_blkPxiThreadStack)], THREAD_MIN_PRIO-1);
	threadStart(&s_blkPxiThread);
}

void blkSetDevCallback(BlkDevCallbackFn fn)
{
	s_blkDevCallback = fn;
}

MEOW_NOINLINE bool blkDevIsPresent(BlkDevice dev)
{
	switch (dev) {
		default:
			return false;

		//case BlkDevice_Dldi: // TODO
		//	return false;

		case BlkDevice_TwlSdCard:
			return s_blkHasTwl && twlSdIsInserted();

		case BlkDevice_TwlNand:
		case BlkDevice_TwlNandAes:
			return s_blkHasTwl;
	}
}

MEOW_NOINLINE bool blkDevInit(BlkDevice dev)
{
	switch (dev) {
		default:
			return false;

		case BlkDevice_TwlSdCard:
			return s_blkHasTwl && twlSdInit();

		case BlkDevice_TwlNand:
		case BlkDevice_TwlNandAes:
			return s_blkHasTwl && twlNandInit();
	}

	return false;
}

MEOW_NOINLINE u32 blkDevGetSectorCount(BlkDevice dev)
{
	if (dev == BlkDevice_TwlNandAes) {
		dev = BlkDevice_TwlNand;
	}

	if (dev >= BlkDevice_Dldi && dev <= BlkDevice_TwlNand) {
		return s_transferRegion->blkdev_sector_count[dev];
	} else {
		return 0;
	}
}

MEOW_NOINLINE bool blkDevReadSectors(BlkDevice dev, void* buffer, u32 first_sector, u32 num_sectors)
{
	switch (dev) {
		default:
			return false;

		case BlkDevice_TwlSdCard:
			return s_blkHasTwl && twlSdReadSectors(buffer, first_sector, num_sectors);

		case BlkDevice_TwlNand:
			return s_blkHasTwl && twlNandReadSectors(buffer, first_sector, num_sectors);

		case BlkDevice_TwlNandAes:
			return s_blkHasTwl && twlNandReadSectorsAes(buffer, first_sector, num_sectors);
	}
}

MEOW_NOINLINE bool blkDevWriteSectors(BlkDevice dev, const void* buffer, u32 first_sector, u32 num_sectors)
{
	switch (dev) {
		default:
			return false;

		case BlkDevice_TwlSdCard:
			return s_blkHasTwl && twlSdWriteSectors(buffer, first_sector, num_sectors);
	}
}
