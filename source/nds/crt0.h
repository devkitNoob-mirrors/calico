#pragma once
#include <calico/types.h>

#define CRT0_MAGIC_ARM7 0x37444f4d // MOD7
#define CRT0_MAGIC_ARM9 0x39444f4d // MOD9

typedef struct Crt0LoadListEntry {
	uptr start;
	uptr end;
	uptr bss_end;
} Crt0LoadListEntry;

typedef struct Crt0LoadList {
	uptr lma;
	Crt0LoadListEntry const* start;
	Crt0LoadListEntry const* end;
} Crt0LoadList;

typedef struct Crt0Header {
	u32 magic;
	Crt0LoadList ll_ntr;
	Crt0LoadList ll_twl;
} Crt0Header;

typedef struct Crt0Header9 {
	Crt0Header base;
	uptr dldi_addr;
} Crt0Header9;

void crt0CopyMem32(uptr dst, uptr src, uptr size);
void crt0FillMem32(uptr dst, u32 value, uptr size);
