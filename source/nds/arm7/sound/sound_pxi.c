#include "common.h"

MEOW_NOINLINE MEOW_CODE32 static void _soundPxiProcessCmd(PxiSoundCmd cmd, unsigned imm, const void* body, unsigned num_words)
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

		case PxiSoundCmd_Start: {
			for (unsigned i = 0; imm; i ++) {
				unsigned bit = 1U<<i;
				if (!(imm & bit)) {
					continue;
				}

				imm &= ~bit;
				soundChStart(i);
			}
			break;
		}

		case PxiSoundCmd_Stop: {
			for (unsigned i = 0; imm; i ++) {
				unsigned bit = 1U<<i;
				if (!(imm & bit)) {
					continue;
				}

				imm &= ~bit;
				soundChStop(i);
			}
			break;
		}

		case PxiSoundCmd_PreparePcm: {
			const PxiSoundArgPreparePcm* arg = (const PxiSoundArgPreparePcm*)body;
			soundChPreparePcm(
				arg->ch, arg->vol, (SoundVolDiv)arg->voldiv, imm, arg->timer,
				(SoundMode)arg->mode, (SoundFmt)arg->fmt, (const void*)(MM_MAINRAM + arg->sad*4), arg->pnt, arg->len);

			if (arg->start) {
				soundChStart(arg->ch);
			}
			break;
		}

		case PxiSoundCmd_PreparePsg: {
			const PxiSoundArgPreparePsg* arg = (const PxiSoundArgPreparePsg*)body;
			unsigned ch = arg->ch + 8;
			soundChPreparePsg(ch, arg->vol, (SoundVolDiv)arg->voldiv, imm, arg->timer, (SoundDuty)arg->duty);

			if (arg->start) {
				soundChStart(ch);
			}
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
