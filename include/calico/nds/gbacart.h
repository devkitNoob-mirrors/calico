#pragma once
#if !defined(__NDS__)
#error "This header file is only for NDS"
#endif

#include "../types.h"
#include "mm.h"
#include "system.h"

/*! @addtogroup gbacart
	@{
*/

#define GBA_WAIT_SRAM_MASK (3U<<0)
#define GBA_WAIT_SRAM_10   (0U<<0)
#define GBA_WAIT_SRAM_8    (1U<<0)
#define GBA_WAIT_SRAM_6    (2U<<0)
#define GBA_WAIT_SRAM_18   (3U<<0)

#define GBA_WAIT_ROM_N_MASK (3U<<2)
#define GBA_WAIT_ROM_N_10   (0U<<2)
#define GBA_WAIT_ROM_N_8    (1U<<2)
#define GBA_WAIT_ROM_N_6    (2U<<2)
#define GBA_WAIT_ROM_N_18   (3U<<2)

#define GBA_WAIT_ROM_S_MASK (1U<<4)
#define GBA_WAIT_ROM_S_6    (0U<<4)
#define GBA_WAIT_ROM_S_4    (1U<<4)

#define GBA_WAIT_ROM_MASK (GBA_WAIT_ROM_N_MASK|GBA_WAIT_ROM_S_MASK)

#define GBA_PHI_MASK  (3U<<5)
#define GBA_PHI_LOW   (0U<<5)
#define GBA_PHI_4_19  (1U<<5)
#define GBA_PHI_8_38  (2U<<5)
#define GBA_PHI_16_76 (3U<<5)

#define GBA_ALL_MASK (GBA_WAIT_SRAM_MASK|GBA_WAIT_ROM_MASK|GBA_PHI_MASK)

MK_EXTERN_C_START

bool gbacartOpen(void);

void gbacartClose(void);

bool gbacartIsOpen(void);

unsigned gbacartSetTiming(unsigned mask, unsigned timing);

MK_EXTERN_C_END

//! @}
