#pragma once
#if !defined(__GBA__)
#error "This header file is only for GBA"
#endif

#include "../types.h"
#include "io.h"

#define REG_KEYINPUT MEOW_REG(u16, IO_KEYINPUT)
#define REG_KEYCNT   MEOW_REG(u16, IO_KEYCNT)

#define KEYCNT_IRQ_ENABLE (1<<14)
#define KEYCNT_IRQ_OR     (0<<15)
#define KEYCNT_IRQ_AND    (1<<15)

enum
{
	KEY_A      = (1<<0),
	KEY_B      = (1<<1),
	KEY_SELECT = (1<<2),
	KEY_START  = (1<<3),
	KEY_RIGHT  = (1<<4),
	KEY_LEFT   = (1<<5),
	KEY_UP     = (1<<6),
	KEY_DOWN   = (1<<7),
	KEY_R      = (1<<8),
	KEY_L      = (1<<9),
	KEY_MASK   = 0x3ff,
};

typedef struct Keypad {
	u16 old;
	u16 cur;
} Keypad;

MEOW_INLINE void keypadRead(Keypad* k)
{
	k->old = k->cur;
	k->cur = ~REG_KEYINPUT & KEY_MASK;
}

MEOW_INLINE u16 keypadHeld(Keypad const* k)
{
	return k->cur;
}

MEOW_INLINE u16 keypadDown(Keypad const* k)
{
	return ~k->old & k->cur;
}

MEOW_INLINE u16 keypadUp(Keypad const* k)
{
	return k->old & ~k->cur;
}
