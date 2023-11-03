#pragma once
#include "../types.h"
#include "../arm/common.h"

MK_EXTERN_C_START

typedef struct NetBuf NetBuf;

typedef enum NetEtherType {
	NetEtherType_First = 0x0600,
	NetEtherType_IPv4  = 0x0800,
	NetEtherType_ARP   = 0x0806,
	NetEtherType_IPv6  = 0x86DD,
	NetEtherType_EAPOL = 0x888E,
} NetEtherType;

typedef struct NetMacHdr {
	u8 dst_mac[6];
	u8 src_mac[6];
	u16 len_or_ethertype_be;
} NetMacHdr;

typedef struct NetLlcSnapHdr {
	u8 dsap;
	u8 ssap;
	u8 control;
	u8 oui[3];
	u16 ethertype_be;
} NetLlcSnapHdr;

typedef struct NetBufListNode {
	NetBuf* next;
	NetBuf* prev;
} NetBufListNode;

struct NetBuf {
	NetBufListNode link;
	u16 flags;
	u16 capacity;
	u16 pos;
	u16 len;
	u32 reserved[4];
};

typedef enum NetBufPool {
	NetBufPool_Tx = 0,
	NetBufPool_Rx = 1,

	NetBufPool_Count
} NetBufPool;

NetBuf* netbufAlloc(unsigned hdr_headroom_sz, unsigned data_sz, NetBufPool pool);
void netbufFlush(NetBuf* nb);
void netbufFree(NetBuf* nb);

MK_INLINE void* netbufGet(NetBuf* nb) {
	return (u8*)(nb+1) + nb->pos;
}

#define netbufPushHeaderType(_nb, _type)  ((_type*)netbufPushHeader ((_nb), sizeof(_type)))
#define netbufPopHeaderType(_nb, _type)   ((_type*)netbufPopHeader  ((_nb), sizeof(_type)))
#define netbufPushTrailerType(_nb, _type) ((_type*)netbufPushTrailer((_nb), sizeof(_type)))
#define netbufPopTrailerType(_nb, _type)  ((_type*)netbufPopTrailer ((_nb), sizeof(_type)))

MK_INLINE void* netbufPushHeader(NetBuf* nb, unsigned size) {
	if (nb->pos < size) {
		return NULL;
	} else {
		nb->pos -= size;
		nb->len += size;
		return netbufGet(nb);
	}
}

MK_INLINE void* netbufPopHeader(NetBuf* nb, unsigned size) {
	void* hdr = NULL;
	if (nb->len >= size) {
		hdr = netbufGet(nb);
		nb->pos += size;
		nb->len -= size;
	}
	return hdr;
}

MK_INLINE void* netbufPushTrailer(NetBuf* nb, unsigned size) {
	void* trailer = NULL;
	if (nb->pos + nb->len + size <= nb->capacity) {
		trailer = (u8*)netbufGet(nb) + nb->len;
		nb->len += size;
	}
	return trailer;
}

MK_INLINE void* netbufPopTrailer(NetBuf* nb, unsigned size) {
	void* trailer = NULL;
	if (nb->len >= size) {
		trailer = (u8*)netbufGet(nb) + nb->len - size;
		nb->len -= size;
	}
	return trailer;
}

MK_INLINE void netbufQueueAppend(NetBufListNode* q, NetBuf* nb) {
	nb->link.next = NULL;
	if (q->next) {
		q->prev->link.next = nb;
	} else {
		q->next = nb;
	}
	q->prev = nb;
}

MK_INLINE NetBuf* netbufQueueRemoveOne(NetBufListNode* q) {
	NetBuf* ret = q->next;
	if (ret) {
		q->next = ret->link.next;
	}
	return ret;
}

MK_INLINE NetBuf* netbufQueueRemoveAll(NetBufListNode* q) {
	return (NetBuf*)armSwapWord(0, (u32*)&q->next);
}

MK_EXTERN_C_END
