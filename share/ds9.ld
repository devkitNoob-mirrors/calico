/*--------------------------------------------------------------------------------
	** Linker script for DS/DSi homebrew, ARM9 side **
--------------------------------------------------------------------------------*/
OUTPUT_ARCH(arm)
ENTRY(__ds9_bootstub)

MEMORY {
	/* Physical binaries loaded from ROM (LMAs) */
	lma9    : ORIGIN = 0x2004000, LENGTH = 0x27c000
	lma9i   : ORIGIN = 0x2400000, LENGTH = 0x280000

	/* Runtime memory layout for code/data areas (VMAs) */
	vectors : ORIGIN = 0x0000000, LENGTH = 0x20     /* Vectors */
	itcm    : ORIGIN = 0x1ff8020, LENGTH = 0x7fe0   /* ITCM (minus vectors) */
	main    : ORIGIN = 0x2000c00, LENGTH = 0xfcb400 /* Main RAM (usable area for ARM9) */
	dtcm    : ORIGIN = 0x2ff0000, LENGTH = 0x3e80   /* DTCM (minus stacks/reserved BIOS memory) */
}

PHDRS {
	/* ELF program headers for the ARM9 binary */
	stub    PT_LOAD FLAGS(7);
	crt0    PT_LOAD FLAGS(7);
	vectors PT_LOAD FLAGS(7);
	itcm    PT_LOAD FLAGS(7);
	dtcm    PT_LOAD FLAGS(7);
	main    PT_LOAD FLAGS(7);
	ldlist  PT_LOAD FLAGS(7);

	/* ELF program headers for the ARM9i binary */
	twl     PT_LOAD FLAGS(0x100007);
	twlld   PT_LOAD FLAGS(0x100007);
}

/* Default initial stack locations and BIOS variables */
PROVIDE_HIDDEN( __sp_usr     = ORIGIN(dtcm) + LENGTH(dtcm) );
PROVIDE_HIDDEN( __sp_irq     = __sp_usr + 0x100            );
PROVIDE_HIDDEN( __sp_svc     = __sp_irq + 0x40             );
PROVIDE_HIDDEN( __irq_flags  = __sp_svc + 0x38             );
PROVIDE_HIDDEN( __irq_vector = __sp_svc + 0x3c             );

/* Overridable locations for ARM exception vectors (pointing to ARM9 BIOS by default) */
PROVIDE_HIDDEN( __arm_excpt_rst  = 0xffff0000 );
PROVIDE_HIDDEN( __arm_excpt_und  = 0xffff0004 );
PROVIDE_HIDDEN( __arm_excpt_svc  = 0xffff0008 );
PROVIDE_HIDDEN( __arm_excpt_pabt = 0xffff000c );
PROVIDE_HIDDEN( __arm_excpt_dabt = 0xffff0010 );
PROVIDE_HIDDEN( __arm_excpt_irq  = 0xffff0018 );
PROVIDE_HIDDEN( __arm_excpt_fiq  = 0xffff001c );

/* Not used */
PROVIDE_HIDDEN( __end__ = 0 );

SECTIONS {

	/* Secure area (2K) */
	.secure ORIGIN(lma9) + 2K: {
		. += 2K - 4;
		LONG(0)
	} AT>lma9 :stub = 0

	ASSERT( ADDR(.secure) == LOADADDR(.secure) + SIZEOF(.secure), "Invalid secure area" )

	/* Small bootstub that copies crt0 into the scratch boot memory */
	.bootstub ADDR(.secure) : {
		KEEP( *(.bootstub .bootstub.*) )
		. = ALIGN(4);
	} AT>lma9 :stub = 0

	/* Initialization code section (crt0) */
	.crt0 : {
		*(.crt0 .crt0.*)
		*.crt0.* (.text .stub .text.* .gnu.linkonce.t.*)
		*.crt0.* (.glue_7t .glue_7 .vfp11_veneer .v4_bx)
		*.crt0.* (.rodata .rodata.* .gnu.linkonce.r.*)
		*.crt0.* (.rodata1)
		. = ALIGN(32);
	} >main AT>lma9 :crt0 = 0xff

	/* Load information for crt0 */
	PROVIDE_HIDDEN( __crt0_lma   = LOADADDR(.crt0)             );
	PROVIDE_HIDDEN( __crt0_start = ADDR(.crt0)                 );
	PROVIDE_HIDDEN( __crt0_end   = ADDR(.crt0) + SIZEOF(.crt0) );

	/* Ensure crt0 does not overlap initial bootstrap */
	ASSERT( __crt0_end <= ADDR(.bootstub) || __crt0_start >= ADDR(.bootstub) + SIZEOF(.bootstub),
		"Error: crt0 section overlaps initial bootstrap" )

	/* ARM exception vectors (start of ITCM) */
	.vectors : {
		KEEP( *(.vectors) )
	} >vectors AT>lma9 :vectors = 0xff

	/* ITCM section */
	.itcm : {
		*(.itcm .itcm.*)
		*.32.o (.text .stub .text.* .gnu.linkonce.t.*)
		*.32.o (.glue_7t .glue_7 .vfp11_veneer .v4_bx)
		*.32.o (.rodata .rodata.* .gnu.linkonce.r.*)
		*.32.o (.rodata1)
		. = ALIGN(4);
	} >itcm AT>lma9 :itcm = 0xff

	/* Load information for ITCM */
	PROVIDE_HIDDEN( __itcm_lma   = LOADADDR(.vectors)             );
	PROVIDE_HIDDEN( __itcm_start = ADDR(.itcm) - SIZEOF(.vectors) );
	PROVIDE_HIDDEN( __itcm_end   = ADDR(.itcm) + SIZEOF(.itcm)    );
	ASSERT( __itcm_lma + SIZEOF(.vectors) == LOADADDR(.itcm), "Error: gap between vectors and itcm" )

	/* DTCM section */
	.dtcm : {
		*(.dtcm .dtcm.*)
		*.32.o (.data .data.* .gnu.linkonce.d.*)
		*.32.o (.data1)
		. = ALIGN(4);
	} >dtcm AT>lma9 :dtcm = 0xff

	/* Load information for DTCM */
	PROVIDE_HIDDEN( __dtcm_lma   = LOADADDR(.dtcm)             );
	PROVIDE_HIDDEN( __dtcm_start = ADDR(.dtcm)                 );
	PROVIDE_HIDDEN( __dtcm_end   = ADDR(.dtcm) + SIZEOF(.dtcm) );

	/* DTCM BSS area */
	.dtcm.bss (NOLOAD) : {
		*(.sbss .sbss.*)
		*.32.o (.dynbss)
		*.32.o (.bss .bss.* .gnu.linkonce.b.*)
		*.32.o (COMMON)
		. = ALIGN(4);
	} >dtcm :NONE

	/* Information for DTCM BSS */
	PROVIDE_HIDDEN( __dtcm_bss_start = ADDR(.dtcm.bss)                     );
	PROVIDE_HIDDEN( __dtcm_bss_end   = ADDR(.dtcm.bss) + SIZEOF(.dtcm.bss) );

	/* ================= */
	/*   Main RAM area   */
	/* ================= */

	/* Main user code/data section */
	/* Important: This is merged into a single section in order to avoid section alignment problems */
	.main : {
		KEEP( *(SORT_NONE(.init)) )
		EXCLUDE_FILE(*.twl.*) *(.text.unlikely .text.*_unlikely .text.unlikely.*)
		EXCLUDE_FILE(*.twl.*) *(.text.exit .text.exit.*)
		EXCLUDE_FILE(*.twl.*) *(.text.startup .text.startup.*)
		EXCLUDE_FILE(*.twl.*) *(.text.hot .text.hot.*)
		EXCLUDE_FILE(*.twl.*) *(.text .iplt .stub .text.* .gnu.linkonce.t.*)
		EXCLUDE_FILE(*.twl.*) *(.glue_7t .glue_7 .vfp11_veneer .v4_bx)
		KEEP( *(SORT_NONE(.fini)) )

		EXCLUDE_FILE(*.twl.*) *(.rodata .rodata.* .gnu.linkonce.r.*)
		EXCLUDE_FILE(*.twl.*) *(.rodata1)
		EXCLUDE_FILE(*.twl.*) *(.igot.plt)
		EXCLUDE_FILE(*.twl.*) *(.data .data.* .gnu.linkonce.d.*)
		EXCLUDE_FILE(*.twl.*) *(.data1)
		SORT(CONSTRUCTORS)

		. = ALIGN(4);
	} >main AT>lma9 :main = 0xff

	/* ARM exception table */
	.ARM.extab : {
		*(.ARM.extab* .gnu.linkonce.armextab.*)
		. = ALIGN(4);
	} >main AT>lma9 :main = 0xff

	/* ARM exception index table (replaces .eh_frame on AArch32) */
	.ARM.exidx : {
		*(.ARM.exidx* .gnu.linkonce.armexidx.*)
	} >main AT>lma9 :main = 0xff

	/* Symbols needed by exception unwind code */
	PROVIDE_HIDDEN( __exidx_start = ADDR(.ARM.exidx)                      );
	PROVIDE_HIDDEN( __exidx_end   = ADDR(.ARM.exidx) + SIZEOF(.ARM.exidx) );

	/* Exception frame header (not used on AArch32) */
	.eh_frame_hdr : {
		*(.eh_frame_hdr)
		*(.eh_frame_entry .eh_frame_entry.*)
	} >main AT>lma9 :main = 0xff

	/* Exception frame information (not used on AArch32) */
	.eh_frame : {
		KEEP( *(.eh_frame) )
		*(.eh_frame.*)
	} >main AT>lma9 :main = 0xff

	/* GCC language agnostic exception table (not used on AArch32?) */
	.gcc_except_table : {
		*(.gcc_except_table .gcc_except_table.*)
	} >main AT>lma9 :main = 0xff

	/* GNU exception table (not used on AArch32?) */
	.gnu_extab : {
		*(.gnu_extab*)
	} >main AT>lma9 :main = 0xff

	/* Table of special constructors that run before _init */
	.preinit_array : {
		PROVIDE_HIDDEN( __preinit_array_start = . );
		KEEP( *(.preinit_array) )
		PROVIDE_HIDDEN( __preinit_array_end = . );
	} >main AT>lma9 :main = 0xff

	/* Table of global constructors (runs after _init) */
	.init_array : {
		PROVIDE_HIDDEN( __init_array_start = . );
		KEEP( *(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)) )
		KEEP( *(.init_array .ctors) )
		PROVIDE_HIDDEN( __init_array_end = . );
	} >main AT>lma9 :main = 0xff

	/* Table of global destructors (runs before _fini) */
	.fini_array : {
		PROVIDE_HIDDEN( __fini_array_start = . );
		KEEP( *(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)) )
		KEEP( *(.fini_array .dtors) )
		PROVIDE_HIDDEN( __fini_array_end = . );
	} >main AT>lma9 :main = 0xff

	/* Load information for code/data on main RAM */
	PROVIDE_HIDDEN( __main_lma   = LOADADDR(.main)                         );
	PROVIDE_HIDDEN( __main_start = ADDR(.main)                             );
	PROVIDE_HIDDEN( __main_end   = ADDR(.fini_array) + SIZEOF(.fini_array) );

	/* Main BSS area */
	.main.bss (NOLOAD) : {
		EXCLUDE_FILE(*.twl.*) *(.dynbss)
		EXCLUDE_FILE(*.twl.*) *(.bss .bss.* .gnu.linkonce.b.*)
		EXCLUDE_FILE(*.twl.*) *(COMMON)
		. = ALIGN(4);
	} >main :NONE

	/* Information for BSS */
	PROVIDE_HIDDEN( __main_bss_start = ADDR(.main.bss)                     );
	PROVIDE_HIDDEN( __main_bss_end   = ADDR(.main.bss) + SIZEOF(.main.bss) );

	/* Ensure BSS fits within the DS's main RAM */
	. = .;
	ASSERT( . <= 0x23f0000, "Error: BSS section overflows DS memory size (4 MB)" )

	/* DSi exclusive code/data section */
	.twl . : {
		*(.twl .twl.*)
		*.twl.* (.text .stub .text.* .gnu.linkonce.t.*)
		*.twl.* (.glue_7t .glue_7 .vfp11_veneer .v4_bx)
		*.twl.* (.rodata .rodata.* .gnu.linkonce.r.*)
		*.twl.* (.rodata1)
		*.twl.* (.data .data.* .gnu.linkonce.d.*)
		*.twl.* (.data1)
		. = ALIGN(4);
	} >main AT>lma9i :twl = 0xff

	/* Load information for the DSi exclusive section */
	PROVIDE_HIDDEN( __twl_lma   = LOADADDR(.twl)            );
	PROVIDE_HIDDEN( __twl_start = ADDR(.twl)                );
	PROVIDE_HIDDEN( __twl_end   = ADDR(.twl) + SIZEOF(.twl) );

	/* DSi exclusive BSS area */
	.twl.bss (NOLOAD) : {
		*(.twl_bss .twl_bss.*)
		*.twl.* (.dynbss)
		*.twl.* (.bss .bss.* .gnu.linkonce.b.*)
		*.twl.* (COMMON)
		. = ALIGN(4);
	} >main :NONE

	/* Information for DSi exclusive BSS area */
	PROVIDE_HIDDEN( __twl_bss_start = ADDR(.twl.bss)                    );
	PROVIDE_HIDDEN( __twl_bss_end   = ADDR(.twl.bss) + SIZEOF(.twl.bss) );

	/* Define start of main heap (different on DS/DSi) */
	/* The end of main heap is defined at runtime by the ARM7 */
	PROVIDE_HIDDEN( __heap_start_ntr = __main_bss_end );
	PROVIDE_HIDDEN( __heap_start_twl = __twl_bss_end  );

	/* Ensure we can craft loadlists */
	PROVIDE_HIDDEN( __loadlist_lma = __itcm_lma );
	PROVIDE_HIDDEN( __twl_loadlist_lma = __twl_lma );
	ASSERT( __itcm_lma + SIZEOF(.vectors) + SIZEOF(.itcm) == __dtcm_lma,
		"Error: Invalid gap between ITCM and DTCM LMAs" )
	ASSERT( __dtcm_lma + SIZEOF(.dtcm) == __main_lma,
		"Error: Invalid gap between DTCM and main RAM LMAs" )
	ASSERT( ADDR(.fini_array) - __main_start == LOADADDR(.fini_array) - __main_lma,
		"Error: Invalid LMAs in sections within main RAM" )

	/* Craft a loadlist */
	.loadlist : {
		LONG(__itcm_start)
		LONG(__itcm_end)
		LONG(__itcm_end)

		LONG(__dtcm_start)
		LONG(__dtcm_end)
		LONG(__dtcm_bss_end)

		LONG(__main_start)
		LONG(__main_end)
		LONG(__main_bss_end)
	} >lma9 :ldlist = 0xff

	PROVIDE_HIDDEN( __loadlist_start = ADDR(.loadlist)                     );
	PROVIDE_HIDDEN( __loadlist_end   = ADDR(.loadlist) + SIZEOF(.loadlist) );

	/* Craft a loadlist for DSi exclusive sections */
	.twl.loadlist : {
		LONG(__twl_start)
		LONG(__twl_end)
		LONG(__twl_bss_end)
	} >lma9i :twlld = 0xff

	PROVIDE_HIDDEN( __twl_loadlist_start = ADDR(.twl.loadlist)                         );
	PROVIDE_HIDDEN( __twl_loadlist_end   = ADDR(.twl.loadlist) + SIZEOF(.twl.loadlist) );

	/* ====================== */
	/*   Debugging metadata   */
	/* ====================== */

	/* Stabs debugging sections.  */
	.stab          0 : { *(.stab) } :NONE
	.stabstr       0 : { *(.stabstr) } :NONE
	.stab.excl     0 : { *(.stab.excl) } :NONE
	.stab.exclstr  0 : { *(.stab.exclstr) } :NONE
	.stab.index    0 : { *(.stab.index) } :NONE
	.stab.indexstr 0 : { *(.stab.indexstr) } :NONE
	.comment       0 : { *(.comment) } :NONE
	.gnu.build.attributes : { *(.gnu.build.attributes .gnu.build.attributes.*) } :NONE
	/* DWARF debug sections.
	   Symbols in the DWARF debugging sections are relative to the beginning
	   of the section so we begin them at 0. */
	/* DWARF 1 */
	.debug          0 : { *(.debug) } :NONE
	.line           0 : { *(.line) } :NONE
	/* GNU DWARF 1 extensions */
	.debug_srcinfo  0 : { *(.debug_srcinfo) } :NONE
	.debug_sfnames  0 : { *(.debug_sfnames) } :NONE
	/* DWARF 1.1 and DWARF 2 */
	.debug_aranges  0 : { *(.debug_aranges) } :NONE
	.debug_pubnames 0 : { *(.debug_pubnames) } :NONE
	/* DWARF 2 */
	.debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) } :NONE
	.debug_abbrev   0 : { *(.debug_abbrev) } :NONE
	.debug_line     0 : { *(.debug_line .debug_line.* .debug_line_end) } :NONE
	.debug_frame    0 : { *(.debug_frame) } :NONE
	.debug_str      0 : { *(.debug_str) } :NONE
	.debug_loc      0 : { *(.debug_loc) } :NONE
	.debug_macinfo  0 : { *(.debug_macinfo) } :NONE
	/* SGI/MIPS DWARF 2 extensions */
	.debug_weaknames 0 : { *(.debug_weaknames) } :NONE
	.debug_funcnames 0 : { *(.debug_funcnames) } :NONE
	.debug_typenames 0 : { *(.debug_typenames) } :NONE
	.debug_varnames  0 : { *(.debug_varnames) } :NONE
	/* DWARF 3 */
	.debug_pubtypes 0 : { *(.debug_pubtypes) } :NONE
	.debug_ranges   0 : { *(.debug_ranges) } :NONE
	/* DWARF Extension.  */
	.debug_macro    0 : { *(.debug_macro) } :NONE
	.debug_addr     0 : { *(.debug_addr) } :NONE
	.ARM.attributes 0 : { KEEP (*(.ARM.attributes)) KEEP (*(.gnu.attributes)) } :NONE
	.note.gnu.arm.ident 0 : { KEEP (*(.note.gnu.arm.ident)) } :NONE
	/DISCARD/ : { *(.note.GNU-stack) *(.gnu_debuglink) *(.gnu.lto_*) *(.stack) }
}
