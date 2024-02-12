#pragma once

/*! @defgroup core Core
	@brief Core types and definitions
	@{
*/

	/*! @defgroup mm Memory map
		@brief General memory areas in the address space
	*/
	/*! @defgroup io I/O registers
		@brief Offsets of memory-mapped registers
	*/

//! @}

#ifdef __NDS__
	/*! @defgroup mm_env Shared
		@ingroup mm
		@brief Addresses of system/reserved memory areas shared between the ARM7 and ARM9
	*/
#endif

/*! @defgroup arm CPU intrinsics
	@brief Low-level wrappers for special functionality of the ARM processor
*/

#if __ARM_ARCH >= 5
	/*! @defgroup cp15 CP15
		@ingroup arm
		@brief System Control coprocessor operations
	*/
	/*! @defgroup cache Cache
		@ingroup cp15
		@brief Cache operations
	*/
#endif

/*! @defgroup system System
	@brief Core operating system functionality
	@{
*/

	/*! @defgroup irq Interrupts
		@brief Interrupt handling
	*/
	/*! @defgroup thread Threads
		@brief Thread management @see @ref thrd_intro
	*/
	/*! @defgroup sync Sync
		@brief Synchronization primitives
	*/
	/*! @defgroup tick Tick
		@brief Timed event scheduling
	*/

//! @}

/*! @defgroup hw Hardware
	@brief Low-level device wrappers
	@{
*/

	/*! @defgroup timer Timers
		@brief System timers
	*/
	/*! @defgroup dma DMA
		@brief Direct Memory Access
	*/
#ifdef __NDS__
	/*! @defgroup ndma NDMA
		@brief New Direct Memory Access (DSi-only)
	*/
	/*! @defgroup scfg SCFG
		@brief System Configuration block (DSi-only)
	*/
#endif

//! @}

/*! @defgroup dev Devices
	@brief High-level device drivers
	@{
*/

	/*! @defgroup bios BIOS
		@brief Predefined routines provided by the built-in system ROM
	*/
#ifdef __NDS__
	/*! @defgroup env Environment
		@brief Definitions for shared memory areas in the environment @see mm_env
	*/
	/*! @defgroup pxi PXI
		@brief ARM9<->ARM7 inter-processor messaging system
	*/
	/*! @defgroup pm Power
		@brief Power and application lifecycle management
	*/
#endif
	/*! @defgroup lcd LCD
		@brief General screen routines
	*/
	/*! @defgroup keypad Keypad
		@brief Button access
	*/
#ifdef __NDS__
	/*! @defgroup touch Touchscreen
		@brief Touchscreen access
	*/
	/*! @defgroup sound Sound
		@brief Sound hardware access
	*/
	/*! @defgroup mic Microphone
		@brief Microphone access
	*/
	/*! @defgroup wlmgr Wireless
		@brief Wireless management
	*/
	/*! @defgroup blkdev Storage
		@brief Block device access
	*/
#ifdef ARM9
	/*! @defgroup ovl Overlays
		@brief DS ROM overlay loading and activation
	*/
#endif
	/*! @defgroup ntrcard NDS slot
		@brief DS card slot access (Slot-1)
	*/
	/*! @defgroup gbacart GBA slot
		@brief GBA cartridge slot access (Slot-2, DS-only)
	*/
#endif

//! @}

#ifdef __NDS__
	/*! @defgroup tlnc TLNC
		@ingroup env
		@brief Autoload protocol (DSi-only)
	*/
	/*! @defgroup netbuf NetBuf
		@ingroup wlmgr
		@brief Network buffer/packet management
	*/
	/*! @defgroup wlan 802.11
		@ingroup wlmgr
		@brief IEEE 802.11 (Wi-Fi) definitions and utilities
	*/
	/*! @defgroup nitrorom NitroROM
		@ingroup ntrcard
		@brief DS ROM filesystem access
	*/
#endif
