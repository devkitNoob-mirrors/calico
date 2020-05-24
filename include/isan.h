#pragma once

// Common types and definitions
#if !__ASSEMBLER__
#include "isan/types.h"
#else
#include "isan/asm.inc"
#endif

// Basic arm definitions
#include "isan/arm.h"

// arm9 (armv5) platforms: CP15 definitions
#if __ARM_ARCH == 5
#include "isan/cp15.h"
#endif

// Memory map & IO register definitions
#if defined(IS_GBA)
#include "isan/gba/mm.h"
#include "isan/gba/io.h"
#endif

#if defined(IS_DS7) || defined(IS_DS9)
// TODO: DS
#endif

#if defined(IS_3DS9)
// TODO: 3DS
#endif

#if !__ASSEMBLER__

#include "isan/irq.h"

#endif
