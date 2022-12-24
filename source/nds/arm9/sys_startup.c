#include <calico/types.h>
#include <calico/nds/mm.h>
#include <calico/nds/io.h>
#include <calico/nds/dma.h>
#include <calico/nds/irq.h>
#include <calico/nds/env.h>

MEOW_WEAK void systemUserStartup(void)
{
	// Nothing
}

MEOW_WEAK void systemStartup(void)
{
	// Clear video display registers
	dmaStartFill32(3, (void*)(MM_IO + IO_GFX_A), 0, IO_TVOUTCNT-IO_DISPCNT);
	dmaBusyWait(3);
	dmaStartFill32(3, (void*)(MM_IO + IO_GFX_B), 0, IO_TVOUTCNT-IO_DISPCNT);
	dmaBusyWait(3);

	// Unmap VRAM
	MEOW_REG(u32, IO_VRAMCNT_A) = 0;
	MEOW_REG(u16, IO_VRAMCNT_E) = 0;
	MEOW_REG(u8,  IO_VRAMCNT_G) = 0;
	MEOW_REG(u16, IO_VRAMCNT_H) = 0;

	// Call user initialization function
	systemUserStartup();
}
