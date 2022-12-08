#include <calico/types.h>
#include <calico/arm/common.h>
#include <calico/system/thread.h>
#include <calico/system/mailbox.h>

static ThrListNode s_mailboxQueue;

bool mailboxSend(Mailbox* mb, u32 message)
{
	ArmIrqState st = armIrqLockByPsr();

	if_unlikely (mb->pending_slots == mb->num_slots) {
		armIrqUnlockByPsr(st);
		return false;
	}

	unsigned next_slot = mb->cur_slot + mb->pending_slots++;
	if (next_slot >= mb->num_slots) {
		next_slot -= mb->num_slots;
	}

	mb->slots[next_slot] = message;
	threadUnblockOneByValue(&s_mailboxQueue, (u32)mb);

	armIrqUnlockByPsr(st);
	return true;
}

u32 mailboxRecv(Mailbox* mb)
{
	ArmIrqState st = armIrqLockByPsr();

	if (!mb->pending_slots) {
		threadBlock(&s_mailboxQueue, (u32)mb);
	}

	u32 message = mb->slots[mb->cur_slot++];
	mb->pending_slots --;
	if (mb->cur_slot >= mb->num_slots) {
		mb->cur_slot -= mb->num_slots;
	}

	armIrqUnlockByPsr(st);
	return message;
}
