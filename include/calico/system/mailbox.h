#pragma once
#include "../types.h"
#include "thread.h"

/*! @addtogroup sync
	@{
*/
/*! @name Mailbox
	@{
*/

MK_EXTERN_C_START

typedef struct Mailbox {
	u32* slots;
	u8 num_slots;
	u8 cur_slot;
	u8 pending_slots;
	u8 send_waiters : 4;
	u8 recv_waiters : 4;
} Mailbox;

MK_INLINE void mailboxPrepare(Mailbox* mb, u32* slots, unsigned num_slots)
{
	mb->slots = slots;
	mb->num_slots = num_slots;
	mb->cur_slot = 0;
	mb->pending_slots = 0;
}

bool mailboxTrySend(Mailbox* mb, u32 message);
bool mailboxTryRecv(Mailbox* mb, u32* out);

u32 mailboxRecv(Mailbox* mb);

MK_EXTERN_C_END

//! @}

//! @}
