#pragma once

// Common types and definitions
#if !__ASSEMBLER__
#include "isan/types.h"
#else
#include "isan/asm.inc"
#endif

#include "isan/hardware.h"

// arm9 (armv5) platforms: CP15 definitions
#if __ARM_ARCH == 5
#include "isan/cp15.h"
#endif

#if !__ASSEMBLER__

#include "isan/irq.h"

#endif
