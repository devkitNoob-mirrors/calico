#pragma once

// Common types and definitions
#if !__ASSEMBLER__
#include "isan/types.h"
#else
#include "isan/asm.inc"
#endif

#include "isan/hardware.h"

// ARM9 platforms: CP15 definitions
#if defined(IS_DS9) || defined(IS_3DS)
#include "isan/cp15.h"
#endif

#if !__ASSEMBLER__
// etc
#endif
