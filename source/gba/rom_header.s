#include <calico/asm.inc>

.section .romhdr, "ax", %progbits
.align 2

.global __gba_boot_mode
.global __gba_mb_slave_id

#ifndef MULTIBOOT
.global __gba_rom_header
__gba_rom_header:
	b __gba_start    @ (0x000) ROM Entrypoint
#else
.global __gba_rom_header_mb
__gba_rom_header_mb:
	b __gba_start_mb @ (0x000) ROM Entrypoint
#endif
	.fill  156,1,0   @ (0x004) Nintendo Logo
	.fill  16,1,0    @ (0x0a0) Game Title
	.ascii "01"      @ (0x0b0) Maker Code
	.byte  0x96      @ (0x0b2) Fixed Value
	.byte  0         @ (0x0b3) Main Unit Code
	.byte  0         @ (0x0b4) Device Type
	.fill  7,1,0     @ (0x0b5) Reserved Area
	.byte  0         @ (0x0bc) Software Version
	.byte  0xf0      @ (0x0bd) Complement Check
	.hword 0         @ (0x0be) Reserved Area

#ifndef MULTIBOOT
1:	b 1b             @ (0x0c0) RAM Entrypoint
#else
	b __gba_start_mb @ (0x0c0) RAM Entrypoint
#endif
__gba_boot_mode:
	.byte  0         @ (0x0c4) Boot mode
__gba_mb_slave_id:
	.byte  0         @ (0x0c5) Slave ID Number
	.fill  10,1,0    @ (0x0c6) Unused
