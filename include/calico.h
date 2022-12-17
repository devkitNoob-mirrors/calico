#pragma once

// Common types and definitions
#if !__ASSEMBLER__
#include "calico/types.h"
#else
#include "calico/asm.inc"
#endif

// Basic arm definitions
#include "calico/arm/psr.h"

// arm9 (armv5) platforms: CP15 definitions
#if __ARM_ARCH >= 5
#include "calico/arm/cp15.h"
#endif

// System bus frequency definition
#include "calico/system/sysclock.h"

// Memory map & IO register definitions
#if defined(__GBA__)
#include "calico/gba/mm.h"
#include "calico/gba/io.h"
#elif defined(__NDS__)
#include "calico/nds/mm.h"
#include "calico/nds/mm_env.h"
#include "calico/nds/io.h"
#else
#error "Unknown/unsupported platform"
#endif

#if !__ASSEMBLER__

#include "calico/arm/common.h"
#if __ARM_ARCH >= 5
#include "calico/arm/cache.h"
#endif

#include "calico/system/irq.h"
#include "calico/system/tick.h"
#include "calico/system/thread.h"
#include "calico/system/mutex.h"
#include "calico/system/mailbox.h"
#include "calico/system/dietprint.h"

#if defined(__GBA__)
#include "calico/gba/bios.h"
#include "calico/gba/keypad.h"
#include "calico/gba/timer.h"
#include "calico/gba/dma.h"
#endif

#if defined(__NDS__)
#include "calico/nds/system.h"
#include "calico/nds/bios.h"
#include "calico/nds/timer.h"
#include "calico/nds/dma.h"
#include "calico/nds/ndma.h"
#include "calico/nds/env.h"
#include "calico/nds/pxi.h"

#ifdef ARM7
#include "calico/nds/arm7/debug.h"

#include "calico/nds/arm7/spi.h"
#include "calico/nds/arm7/pmic.h"

#include "calico/nds/arm7/i2c.h"
#include "calico/nds/arm7/mcu.h"
#endif

#ifdef ARM9
#include "calico/nds/arm9/arm7_debug.h"
#endif

#endif

#endif
