#include "common.h"

MEOW_NOINLINE static void _soundPxiProcessCmd(PxiSoundCmd cmd, unsigned imm, const u32* body, unsigned num_words)
{
	switch (cmd) {
		default: {
			dietPrint("[SND] unkcmd%u 0x%X + %u\n", cmd, imm, num_words);
			break;
		}

		case PxiSoundCmd_Synchronize: {
			pxiReply(PxiChannel_Sound, 0);
			break;
		}
	}
}

void _soundPxiProcess(Mailbox* mb, bool do_credit_update)
{
	u32 cmdheader;
	while (mailboxTryRecv(mb, &cmdheader)) {
		unsigned num_words = cmdheader >> 26;
		if (num_words) {
			cmdheader &= (1U<<26)-1;
		}

		PxiSoundCmd cmd = pxiSoundCmdGetType(cmdheader);
		unsigned imm = pxiSoundCmdGetImm(cmdheader);
		do_credit_update |= pxiSoundCmdIsUpdateCredits(cmdheader);
		g_soundState.pxi_credits += 1 + num_words;

		if_likely (num_words == 0) {
			// Process command directly
			_soundPxiProcessCmd(cmd, imm, NULL, 0);
		} else {
			// Receive message body
			u32 body[num_words];
			for (unsigned i = 0; i < num_words; i ++) {
				body[i] = mailboxRecv(mb);
			}

			// Process command
			_soundPxiProcessCmd(cmd, imm, body, num_words);
		}
	}

	if (do_credit_update && g_soundState.pxi_credits) {
		unsigned credits = g_soundState.pxi_credits;
		g_soundState.pxi_credits = 0;
		dietPrint("[SND] Used %u credits\n", credits);
		pxiSend(PxiChannel_Sound, pxiSoundMakeEventMsg(PxiSoundEvent_UpdateCredits, credits));
	}
}
