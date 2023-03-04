cmake_minimum_required(VERSION 3.13)

set(DKP_PLATFORM_BOOTSTRAP TRUE)
set(DKP_GBA_PLATFORM_LIBRARY calico)
set(DKP_NDS_PLATFORM_LIBRARY calico)

project(calico
	VERSION 0.0.1
	LANGUAGES C ASM
)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set(CMAKE_INSTALL_PREFIX "${DEVKITPRO}/calico" CACHE PATH "" FORCE)
endif()

if(NINTENDO_GBA)
	set(PLATFORM_SUFFIX "_gba")
	set(ARM7 TRUE)
elseif(NINTENDO_DS)
	if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "armv4t")
		set(PLATFORM_SUFFIX "_ds7")
		set(ARM7 TRUE)
	elseif("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "armv5te")
		set(PLATFORM_SUFFIX "_ds9")
		set(ARM9 TRUE)
	else()
		message(FATAL_ERROR "Invalid NDS processor, must be armv4t or armv5te")
	endif()
else()
	message(FATAL_ERROR "Unsupported platform")
endif()

#------------------------------------------------------------------------------

# Define static library target
add_library(${PROJECT_NAME} STATIC)
set_target_properties(${PROJECT_NAME} PROPERTIES RELEASE_POSTFIX    "${PLATFORM_SUFFIX}")
set_target_properties(${PROJECT_NAME} PROPERTIES MINSIZEREL_POSTFIX "${PLATFORM_SUFFIX}")
set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX      "${PLATFORM_SUFFIX}d")

# Add compiler flags
target_compile_options(${PROJECT_NAME} PRIVATE
	# Common C/C++ options
	-Wall -Werror

	# C++ specific options
	$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti>
)

# Add include directories
target_include_directories(${PROJECT_NAME} PRIVATE
	include
)

target_sources(${PROJECT_NAME} PRIVATE
	source/system/irq.c
	source/system/tick.c
	source/system/thread_cold.c
	source/system/thread_hot.32.c
	source/system/mutex.c
	source/system/mailbox.c
	source/system/dietprint.c
	source/system/newlib_syscalls.c

	source/arm/arm-copy-fill.32.s
	source/arm/arm-context.32.s
	source/arm/arm-shims.32.c
)

if(NOT ARM7)
	target_sources(${PROJECT_NAME} PRIVATE
		source/arm/arm-cache.32.s
	)
endif()

if(NINTENDO_GBA)
	target_sources(${PROJECT_NAME} PRIVATE
		source/gba/rom_header.s
		source/gba/rom_header_mb.s

		source/gba/crt0.s
		source/gba/crt0_mb.s

		source/gba/bios.s
		source/gba/irq_handler.32.s
	)
endif()

if(NINTENDO_DS)
	target_sources(${PROJECT_NAME} PRIVATE
		source/nds/startup.crt0.c
		source/nds/utils.crt0.s

		source/nds/bios.s
		source/nds/bios.twl.s
		source/nds/irq_handler.32.s
		source/nds/pxi.c
		source/nds/smutex.32.c
		source/nds/keypad.c
		source/nds/pm.c

		source/nds/netbuf.c
		source/dev/wlan.c
	)

	if(ARM9)
		target_sources(${PROJECT_NAME} PRIVATE
			source/nds/arm9/bootstub_arm9.s
			source/nds/arm9/mpu_setup.crt0.s
			source/nds/arm9/sys_startup.c

			source/nds/arm9/arm7_debug.c
			source/nds/arm9/pm_arm9.c
			source/nds/arm9/touch.c
			source/nds/arm9/sound.c
			source/nds/arm9/blk.c
			source/nds/arm9/wlmgr.c
		)
	endif()

	if(ARM7)
		target_sources(${PROJECT_NAME} PRIVATE
			source/nds/arm7/bootstub_arm7.s

			source/nds/arm7/debug.c
			source/nds/arm7/rtc.c
			source/nds/arm7/spi.c
			source/nds/arm7/pmic.c
			source/nds/arm7/nvram.c
			source/nds/arm7/tsc.c
			source/nds/arm7/env.c
			source/nds/arm7/pm_arm7.c
			source/nds/arm7/touch.c

			source/nds/arm7/i2c.twl.c
			source/nds/arm7/mcu.twl.c
			source/nds/arm7/codec.twl.c
			source/nds/arm7/gpio.twl.c

			source/nds/arm7/tmio.twl.32.c
			source/nds/arm7/sdmmc.twl.32.c
			source/nds/arm7/sdio.twl.32.c
			source/nds/arm7/ar6k.twl.32.c

			source/nds/arm7/sound/sound.c
			source/nds/arm7/sound/sound_pxi.c

			source/nds/arm7/blk.c
			source/nds/arm7/blk.twl.32.c

			source/nds/arm7/wlmgr.c
			source/nds/arm7/wifi.twl.32.c
			source/nds/arm7/wpa.twl.32.c
			source/nds/arm7/wpa_hmac_sha1.twl.32.c
			source/nds/arm7/wpa_aes.twl.s
		)
	endif()
endif()

include(GNUInstallDirs)

# Install the library
install(
	TARGETS ${PROJECT_NAME}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Install linker scripts
install(
	DIRECTORY ${PROJECT_SOURCE_DIR}/share/
	DESTINATION ${CMAKE_INSTALL_LIBDIR}
	FILES_MATCHING
		PATTERN "*.ld"
)

# Install headers
install(
	DIRECTORY ${PROJECT_SOURCE_DIR}/include/
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
	FILES_MATCHING
		PATTERN "*.h"
		PATTERN "*.hpp"
		PATTERN "*.inc"
)

# Install ancillary files
install(
	DIRECTORY ${PROJECT_SOURCE_DIR}/share/
	DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}
	FILES_MATCHING
		PATTERN "*.specs"
		PATTERN "*.bmp"
)
