#pragma once
#if !defined(__NDS__)
#error "This header file is only for NDS"
#endif

#include "../types.h"
#include "io.h"

#define REG_PXI_SYNC MEOW_REG(u16, IO_PXI_SYNC)
#define REG_PXI_CNT  MEOW_REG(u32, IO_PXI_CNT)
#define REG_PXI_SEND MEOW_REG(u32, IO_PXI_SEND)
#define REG_PXI_RECV MEOW_REG(u32, IO_PXI_RECV)

#define PXI_SYNC_RECV(_n)   ((_n) & 0xF)
#define PXI_SYNC_SEND(_n)   (((_n) & 0xF) << 8)
#define PXI_SYNC_IRQ_SEND   (1U << 13)
#define PXI_SYNC_IRQ_ENABLE (1U << 14)

#define PXI_CNT_SEND_EMPTY      (1U << 0)
#define PXI_CNT_SEND_FULL       (1U << 1)
#define PXI_CNT_SEND_IRQ        (1U << 2)
#define PXI_CNT_SEND_CLEAR      (1U << 3)
#define PXI_CNT_RECV_EMPTY      (1U << 8)
#define PXI_CNT_RECV_FULL       (1U << 9)
#define PXI_CNT_RECV_IRQ        (1U << 10)
#define PXI_CNT_ERROR           (1U << 14)
#define PXI_CNT_ENABLE          (1U << 15)

#define PXI_FIFO_LEN_WORDS 16

#define PXI_NO_REPLY UINT32_MAX

// Packet format:
//  Bit    Description
//   0-4    Channel
//   5      Direction (0=request, 1=response)
//   6-31   Immediate (26-bit)

// Extended packet format:
//  Bit    Description
//   0-4    Must be 0x1f (PxiChannel_Extended)
//   5      Direction (0=request, 1=response)
//   6-10   Channel
//   11-15  Number of extra words minus 1
//   16-31  Immediate (16-bit)

typedef enum PxiChannel {
	PxiChannel_Power    = 0,
	PxiChannel_Sound    = 1,
	PxiChannel_System   = 2,
	PxiChannel_Mitsumi  = 3,
	PxiChannel_Atheros  = 4,
	PxiChannel_DLDI     = 5,
	PxiChannel_SDMMC    = 6,

	PxiChannel_Reset    = 12,

	PxiChannel_User0    = 23,
	PxiChannel_User1    = 24,
	PxiChannel_User2    = 25,
	PxiChannel_User3    = 26,
	PxiChannel_User4    = 27,
	PxiChannel_User5    = 28,
	PxiChannel_User6    = 29,
	PxiChannel_User7    = 30,

	PxiChannel_Extended = 31,
	PxiChannel_Count = PxiChannel_Extended,
} PxiChannel;

MEOW_CONSTEXPR u32 pxiMakePacket(PxiChannel ch, bool dir, u32 imm)
{
	return (ch & 0x1f) | ((unsigned)dir << 5) | (imm << 6);
}

MEOW_CONSTEXPR PxiChannel pxiPacketGetChannel(u32 packet)
{
	return (PxiChannel)(packet & 0x1f);
}

MEOW_CONSTEXPR bool pxiPacketIsResponse(u32 packet)
{
	return (packet >> 5) & 1;
}

MEOW_CONSTEXPR bool pxiPacketIsRequest(u32 packet)
{
	return !pxiPacketIsResponse(packet);
}

MEOW_CONSTEXPR u32 pxiPacketGetImmediate(u32 packet)
{
	return packet >> 6;
}

MEOW_CONSTEXPR u32 pxiMakeExtPacket(PxiChannel ch, bool dir, unsigned num_words, u16 imm)
{
	return PxiChannel_Extended | ((unsigned)dir << 5) | ((ch & 0x1f) << 6) | (((num_words - 1) & 0x1f) << 11) | (imm << 16);
}

MEOW_CONSTEXPR PxiChannel pxiExtPacketGetChannel(u32 packet)
{
	return (PxiChannel)((packet >> 11) & 0x1f);
}

MEOW_CONSTEXPR unsigned pxiExtPacketGetNumWords(u32 packet)
{
	return ((packet >> 11) & 0x1f) + 1;
}

MEOW_CONSTEXPR u16 pxiExtPacketGetImmediate(u32 packet)
{
	return packet >> 16;
}

typedef void (* PxiHandlerFn)(void* user, u32 data);
struct Mailbox; // forward declare

MEOW_INLINE void pxiPing(void)
{
	REG_PXI_SYNC |= PXI_SYNC_IRQ_SEND;
}

void pxiWaitForPing(void);

void pxiSetHandler(PxiChannel ch, PxiHandlerFn fn, void* user);
void pxiSetMailbox(PxiChannel ch, struct Mailbox* mb);
void pxiWaitRemote(PxiChannel ch);

void pxiSendPacket(u32 packet);

void pxiSendExtPacket(u32 packet, const u32* data);

void pxiBeginReceive(PxiChannel ch);
u32 pxiEndReceive(PxiChannel ch);

MEOW_INLINE void pxiSend(PxiChannel ch, u32 imm)
{
	pxiSendPacket(pxiMakePacket(ch, false, imm));
}

MEOW_INLINE void pxiSendWithData(PxiChannel ch, u16 imm, const u32* data, u32 num_words)
{
	pxiSendExtPacket(pxiMakeExtPacket(ch, false, num_words, imm), data);
}

MEOW_INLINE void pxiReply(PxiChannel ch, u32 imm)
{
	pxiSendPacket(pxiMakePacket(ch, true, imm));
}

MEOW_INLINE u32 pxiSendAndReceive(PxiChannel ch, u32 imm)
{
	pxiBeginReceive(ch);
	pxiSend(ch, imm);
	return pxiEndReceive(ch);
}

MEOW_INLINE u32 pxiSendWithDataAndReceive(PxiChannel ch, u32 imm, const u32* data, u32 num_words)
{
	pxiBeginReceive(ch);
	pxiSendWithData(ch, imm, data, num_words);
	return pxiEndReceive(ch);
}
