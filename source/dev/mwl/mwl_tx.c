#include "common.h"
#include <calico/arm/common.h>
#include <calico/system/thread.h>
#include <calico/nds/dma.h>

MK_INLINE u32 _mwlGenWepIv(void)
{
	u16 lo = MWL_REG(W_RANDOM) * 0x101;
	u16 hi = MWL_REG(W_RANDOM) & 0xff;
	return lo | (hi << 16);
}

static void _mwlTxWrite(const void* src, unsigned len)
{
	const u16* src16 = (const u16*)src;

	while (len > 1) {
		MWL_REG(W_TXBUF_WR_DATA) = *src16++;
		len -= 2;
	}

	if (len) {
		MWL_REG(W_TXBUF_WR_DATA) = *src16 & 0xff;
	}
}

static unsigned _mwlTxQueueWrite(unsigned qid, NetBuf* pPacket)
{
	MwlDataTxHdr hdr = { 0 };
	WlanMacHdr* machdr = (WlanMacHdr*)netbufGet(pPacket);

	// Fill in hardware header
	hdr.service_rate = 20;
	hdr.mpdu_len = pPacket->len + 4; // add FCS
	if (machdr->fc.wep) {
		hdr.mpdu_len += 8; // add WEP IV/key ID + ICV
	}

	// Ensure packet fits in Wifi RAM
	if_unlikely ((hdr.mpdu_len - 4) >= s_mwlState.tx_size[qid]) {
		return 0;
	}

	// Calculate value for W_TXBUF_LOCx
	unsigned reg = (s_mwlState.tx_pos[qid]/2) | (1U<<15);
	if (machdr->fc.type == WlanFrameType_Control) {
		reg |= 1U<<13;
	} else if (machdr->fc.type == WlanFrameType_Management && machdr->fc.subtype == WlanMgmtType_ProbeResp) {
		reg |= 1U<<12;
	}

	// Adjust service rate if needed
	if_likely (machdr->fc.type == WlanFrameType_Data) {
		hdr.service_rate = 20; // XX: maybe not assume all APs can handle this
	}

	// Write hardware header
	MWL_REG(W_TXBUF_WR_ADDR) = s_mwlState.tx_pos[qid];
	_mwlTxWrite(&hdr, sizeof(hdr));

	if (machdr->fc.wep) {
		// Write IEEE header
		_mwlTxWrite(machdr, sizeof(*machdr));

		// Write WEP IV/key ID
		u32 wep_ctrl = _mwlGenWepIv() | (0U << 30); // XX: always using key id = 0
		MWL_REG(W_TXBUF_WR_DATA) = wep_ctrl;
		MWL_REG(W_TXBUF_WR_DATA) = wep_ctrl>>16;

		// Write packet body
		_mwlTxWrite(machdr+1, pPacket->len - sizeof(*machdr));

		// Write WEP ICV
		MWL_REG(W_TXBUF_WR_DATA) = 0;
		MWL_REG(W_TXBUF_WR_DATA) = 0;
	} else {
		// Write the entire packet
		_mwlTxWrite(machdr, pPacket->len);
	}

	return reg;
}

MK_NOINLINE static void _mwlTxQueueKick(unsigned qid)
{
	MwlTxQueue* q = &s_mwlState.tx_queues[qid];
	unsigned bit = 1U<<qid;
	mutexLock(&s_mwlState.tx_mutex);

	bool can_kick;
	do {
		NetBuf* pPacket = q->list.next;

		can_kick = pPacket && (s_mwlState.tx_busy & bit) == 0;
		if (can_kick) {
			q->list.next = pPacket->link.next;

			q->cb  = (MwlTxCallback)pPacket->reserved[0];
			q->arg = (void*)pPacket->reserved[1];

			unsigned reg = _mwlTxQueueWrite(qid, pPacket);
			netbufFree(pPacket);

			if (reg) {
				MWL_REG(W_TXBUF_LOC1 + qid*4) = reg;
				s_mwlState.tx_busy |= bit;
			}

			if (q->cb) {
				q->cb(q->arg, reg ? MwlTxEvent_Queued : MwlTxEvent_Dropped, NULL);
			}
		}
	} while (can_kick);

	mutexUnlock(&s_mwlState.tx_mutex);
}

void _mwlTxEndTask(void)
{
	unsigned kick_bits = 0;
	mutexLock(&s_mwlState.tx_mutex);

	for (unsigned i = 0; i < 3; i ++) {
		unsigned qid = 2 - i;
		unsigned bit = 1U<<qid;
		MwlTxQueue* q = &s_mwlState.tx_queues[qid];

		if ((MWL_REG(W_TXBUF_LOC1 + qid*4) & (1U<<15)) || !(s_mwlState.tx_busy & bit)) {
			continue;
		}

		kick_bits |= bit;
		s_mwlState.tx_busy &= ~bit;
		MwlDataTxHdr* tx_hdr = (MwlDataTxHdr*)(MWL_MAC_RAM_ADDR + s_mwlState.tx_pos[qid]);
		bool ok = (tx_hdr->status & 2) == 0;

		// XX: Handle WEP retries here

		if (q->cb) {
			q->cb(q->arg, ok ? MwlTxEvent_Done : MwlTxEvent_Error, tx_hdr);
		}
	}

	mutexUnlock(&s_mwlState.tx_mutex);

	for (unsigned i = 0; kick_bits && i < 3; i ++) {
		unsigned qid = 2 - i;
		unsigned bit = 1U<<qid;

		if (kick_bits & bit) {
			kick_bits &= ~bit;
			_mwlTxQueueKick(qid);
		}
	}
}

void _mwlTxQueueClear(unsigned qid)
{
	unsigned bit = 1U<<qid;
	MwlTxQueue* q = &s_mwlState.tx_queues[qid];
	mutexLock(&s_mwlState.tx_mutex);

	if ((s_mwlState.tx_busy & bit) && q->cb) {
		s_mwlState.tx_busy &= ~bit;
		q->cb(q->arg, MwlTxEvent_Dropped, NULL);
	}

	NetBuf* next;
	for (NetBuf* pPacket = q->list.next; pPacket; pPacket = next) {
		next = pPacket->link.next;
		q->cb  = (MwlTxCallback)pPacket->reserved[0];
		q->arg = (void*)pPacket->reserved[1];
		netbufFree(pPacket);

		if (q->cb) {
			q->cb(q->arg, MwlTxEvent_Dropped, NULL);
		}
	}

	__builtin_memset(q, 0, sizeof(*q));
	mutexUnlock(&s_mwlState.tx_mutex);
}

void mwlDevTx(unsigned qid, NetBuf* pPacket, MwlTxCallback cb, void* arg)
{
	MwlTxQueue* q = &s_mwlState.tx_queues[qid];

	pPacket->reserved[0] = (u32)cb;
	pPacket->reserved[1] = (u32)arg;

	mutexLock(&s_mwlState.tx_mutex);
	netbufQueueAppend(&q->list, pPacket);
	mutexUnlock(&s_mwlState.tx_mutex);

	_mwlTxQueueKick(qid);
}
