#include <calico/asm.inc>
#include <calico/dev/dldi_defs.h>

#define DLDI_ALLOCATED_SPACE DLDI_SIZE_16KB

.section .dldi, "ax", %progbits
.align 2
.global __dldi_start

__dldi_start:
	@ Header magic
	.word  DLDI_MAGIC_VAL
	.asciz DLDI_MAGIC_STRING

	@ Properties
	.byte  1              @ version_num
	.byte  DLDI_SIZE_16KB @ driver_sz_log2
	.byte  0              @ fix_flags
	.byte  DLDI_SIZE_16KB @ alloc_sz_log2

	@ Interface name
1:	.ascii "Default (No interface)"
	.space DLDI_FRIENDLY_NAME_LEN - (. - 1b)

	@ Address table
	.word  __dldi_start @ dldi_start
	.word  .Ldldi_end   @ dldi_end
	.word  0            @ glue_start
	.word  0            @ glue_end
	.word  0            @ got_start
	.word  0            @ got_end
	.word  0            @ bss_start
	.word  0            @ bss_end

	@ Disc interface
	.ascii "DLDI"       @ io_type
	.word  0            @ features
	.word  .Ldummy_func @ startup
	.word  .Ldummy_func @ is_inserted
	.word  .Ldummy_func @ read_sectors
	.word  .Ldummy_func @ write_sectors
	.word  .Ldummy_func @ clear_status
	.word  .Ldummy_func @ shutdown

.Ldummy_func:
	mov r0, #0
	bx  lr

.pool
.space (1<<DLDI_ALLOCATED_SPACE) - (. - __dldi_start)
.Ldldi_end:
