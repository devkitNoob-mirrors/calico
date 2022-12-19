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

	source/arm/arm-copy-fill.32.s
	source/arm/arm-context.32.s
	source/arm/irq-lock.32.c
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
		source/nds/irq_handler.32.s
		source/nds/pxi.c
	)

	if(ARM9)
		target_sources(${PROJECT_NAME} PRIVATE
			source/nds/bootstub_arm9.s
			source/nds/mpu_setup.crt0.s

			source/nds/arm9/arm7_debug.c
		)
	endif()

	if(ARM7)
		target_sources(${PROJECT_NAME} PRIVATE
			source/nds/bootstub_arm7.s

			source/nds/arm7/debug.c
			source/nds/arm7/pmic.c
			source/nds/arm7/i2c.twl.c
			source/nds/arm7/mcu.twl.c

			source/nds/arm7/tmio.twl.32.c
		)
	endif()
endif()

include(GNUInstallDirs)

# Install the library
install(
	TARGETS ${PROJECT_NAME}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
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
		PATTERN "*.ld"
		PATTERN "*.specs"
		PATTERN "*.bmp"
)
