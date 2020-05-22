#pragma once

// Memory map & IO register definitions
#if defined(IS_GBA)
#include "isan/gba_mm.h"
#include "isan/gba_io.h"
#elif defined(IS_DS7) || defined(IS_DS9)
// TODO: DS
#elif defined(IS_3DS9)
// TODO: 3DS
#else
#error "Unsupported platform."
#endif
