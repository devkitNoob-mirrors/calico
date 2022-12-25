#pragma once
#include "../types.h"
#include "thread.h"

typedef struct Mailbox {
	u32* slots;
	u8 num_slots;
	u8 cur_slot;
	u8 pending_slots;
	u8 send_waiters : 4;
	u8 recv_waiters : 4;
} Mailbox;

MEOW_INLINE void mailboxPrepare(Mailbox* mb, u32* slots, unsigned num_slots)
{
	mb->slots = slots;
	mb->num_slots = num_slots;
	mb->cur_slot = 0;
	mb->pending_slots = 0;
}

bool mailboxTrySend(Mailbox* mb, u32 message);

u32 mailboxRecv(Mailbox* mb);
