#pragma once
#include "../types.h"
#include "tmio.h"

#define SDIO_CMD_GO_IDLE           (TMIO_CMD_INDEX(0)  | TMIO_CMD_RESP_NONE)
#define SDIO_CMD_GET_RELATIVE_ADDR (TMIO_CMD_INDEX(3)  | TMIO_CMD_RESP_48)
#define SDIO_CMD_SEND_OP_COND      (TMIO_CMD_INDEX(5)  | TMIO_CMD_RESP_48_NOCRC)
#define SDIO_CMD_SELECT_CARD       (TMIO_CMD_INDEX(7)  | TMIO_CMD_RESP_48_BUSY)
#define SDIO_CMD_RW_DIRECT         (TMIO_CMD_INDEX(52) | TMIO_CMD_RESP_48)

#define SDIO_RW_DIRECT_DATA(_x) ((_x)&0xff)
#define SDIO_RW_DIRECT_ADDR(_x) (((_x)&0x1ffff)<<9)
#define SDIO_RW_DIRECT_WR_RD    (1U<<27)
#define SDIO_RW_DIRECT_FUNC(_x) (((_x)&7)<<28)
#define SDIO_RW_DIRECT_READ     (0U<<31)
#define SDIO_RW_DIRECT_WRITE    (1U<<31)

#define SDIO_BLOCK_SZ 128

typedef struct SdioManfid {
	u16 code;
	u16 id;
} SdioManfid;

typedef struct SdioCard {
	TmioCtl* ctl;
	TmioPort port;

	u16 rca;
	SdioManfid manfid;
	u8 revision;
	u8 caps;
} SdioCard;

bool sdioCardInit(SdioCard* card, TmioCtl* ctl, unsigned port);
bool sdioCardReadDirect(SdioCard* card, unsigned func, unsigned addr, void* out, size_t size);
bool sdioCardWriteDirect(SdioCard* card, unsigned func, unsigned addr, const void* in, size_t size);
