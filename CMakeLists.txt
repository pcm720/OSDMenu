# OSDMenu CMake configuration
cmake_minimum_required(VERSION 3.13)

option(RELEASE "Build release package" OFF)

include($ENV{PS2SDK}/ps2dev.cmake)
set(BIN2C "$ENV{PS2SDK}/bin/bin2c")
set(PS2_PACKER "$ENV{PS2DEV}/bin/ps2-packer")

include(${CMAKE_SOURCE_DIR}/cmake/functions.cmake)

# Remove default linkfile from CMAKE_EXE_LINKER_FLAGS_INIT
# Targets will add them via target_link_options
string(REGEX REPLACE "-T[^ ]*linkfile[^ ]*" "" CLEANED_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS_INIT}")
string(REGEX REPLACE "  +" " " CLEANED_LINKER_FLAGS "${CLEANED_LINKER_FLAGS}")
string(STRIP "${CLEANED_LINKER_FLAGS}" CLEANED_LINKER_FLAGS)
set(CMAKE_EXE_LINKER_FLAGS_INIT "${CLEANED_LINKER_FLAGS}")

project(OSDMenu C ASM)

# Find Python 3 for KELF signing
find_program(PYTHON3 NAMES python3)
if(NOT PYTHON3)
    message(WARNING "Python 3 not found. KELF signing will not work. Install Python 3 or set PYTHON3 manually.")
endif()

option(ENABLE_PRINTF "Enable printf debugging output (requires UDPTTY)" OFF)
option(DEBUG "Disable stripping ELFs (keeps debug symbols)" OFF)
set(UDPTTY_IP "192.168.1.6" CACHE STRING "UDPTTY IP address (used when ENABLE_PRINTF is ON)")

# Build smap_udptty if printf is enabled (used by both launcher and mbr)
if(ENABLE_PRINTF)
    set(SMAP_UDPTTY_IRX_FILE "${CMAKE_SOURCE_DIR}/launcher/iop/smap_udptty/smap_udptty.irx")
    add_custom_target(smap_udptty
        COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_SOURCE_DIR}/launcher/iop/smap_udptty" ${CMAKE_MAKE_PROGRAM}
        BYPRODUCTS "${SMAP_UDPTTY_IRX_FILE}"
        COMMENT "Building smap_udptty IOP module using Makefile"
    )
endif()

add_subdirectory(utils/ps1vn)
add_subdirectory(utils/loader)
add_subdirectory(launcher)
add_subdirectory(mbr)

# Build patcher twice: once as osdmenu and once as hosdmenu
set(PATCHER_HOSD OFF)
include(${CMAKE_SOURCE_DIR}/patcher/CMakeLists.txt)

set(PATCHER_HOSD ON)
include(${CMAKE_SOURCE_DIR}/patcher/CMakeLists.txt)

# Build release package
if(RELEASE)
    # Build installer only for release
    add_subdirectory(utils/installer)

    # Get binary directories for file paths
    get_target_property(mbr_binary_dir osdmbr BINARY_DIR)
    get_target_property(launcher_binary_dir launcher BINARY_DIR)
    get_target_property(installer_binary_dir osdmbr-installer BINARY_DIR)

    # Custom launcher, MBR and MBR installer paths
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/release/osdmbr/payloads/osdmbr.elf"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/release/osdmbr/payloads"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:osdmbr>" "${CMAKE_BINARY_DIR}/release/osdmbr/payloads/osdmbr.elf"
        DEPENDS osdmbr
        COMMENT "Copying osdmbr.elf to release package"
    )
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/release/launcher/launcher.elf"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/release/launcher"
        COMMAND ${CMAKE_COMMAND} -E copy "${launcher_binary_dir}/launcher.elf" "${CMAKE_BINARY_DIR}/release/launcher/launcher.elf"
        DEPENDS launcher
        COMMENT "Copying launcher.elf to release package"
    )
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/release/osdmbr/osdmbr-installer.elf"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/release/osdmbr"
        COMMAND ${CMAKE_COMMAND} -E copy "${installer_binary_dir}/osdmbr-installer.elf" "${CMAKE_BINARY_DIR}/release/osdmbr/osdmbr-installer.elf"
        DEPENDS osdmbr-installer
        COMMENT "Copying osdmbr-installer.elf to release package"
    )

    add_custom_target(release ALL
        # Create release directory structure
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/release"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/release/patcher/kelf"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/release/osdmbr/payloads"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/release/launcher"

        # Copy examples
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/examples" "${CMAKE_BINARY_DIR}/release/examples"

        # Copy osdmenu.elf and generate PSU
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/osdmenu/osdmenu.elf" "${CMAKE_BINARY_DIR}/release/osdmenu.elf"
        COMMAND ${PYTHON3} "${CMAKE_SOURCE_DIR}/utils/psu/makepsu.py" "${CMAKE_BINARY_DIR}/release/SYS_OSDMENU.psu" "SYS_OSDMENU" "${CMAKE_SOURCE_DIR}/utils/psu/res" "${CMAKE_BINARY_DIR}/release/osdmenu.elf"

        # Copy hosdmenu.elf
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/hosdmenu/hosdmenu.elf" "${CMAKE_BINARY_DIR}/release/hosdmenu.elf"

        # Copy osdmenu KELF and related files
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/osdmenu/OSDMENU.XLF" "${CMAKE_BINARY_DIR}/release/patcher/kelf/OSDMENU.XLF"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/utils/icn" "${CMAKE_BINARY_DIR}/release/patcher/kelf"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/patcher/README.md" "${CMAKE_BINARY_DIR}/release/patcher/README.md"

        # Copy README.md
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/README.md" "${CMAKE_BINARY_DIR}/release/README.md"

        # Copy osdmbr files
        COMMAND ${CMAKE_COMMAND} -E copy "${mbr_binary_dir}/OSDMBR.XLF" "${CMAKE_BINARY_DIR}/release/osdmbr/OSDMBR.XLF"
        COMMAND ${CMAKE_COMMAND} -E copy "${mbr_binary_dir}/osdmbr.bin" "${CMAKE_BINARY_DIR}/release/osdmbr/payloads/osdmbr.bin"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/mbr/README.md" "${CMAKE_BINARY_DIR}/release/osdmbr/README.md"

        # Copy launcher files
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/launcher/README.md" "${CMAKE_BINARY_DIR}/release/launcher/README.md"

        DEPENDS
            osdmenu
            hosdmenu
            osdmbr
            launcher
            "${CMAKE_BINARY_DIR}/release/osdmbr/osdmbr-installer.elf"
            "${CMAKE_BINARY_DIR}/release/osdmbr/payloads/osdmbr.elf"
            "${CMAKE_BINARY_DIR}/release/launcher/launcher.elf"
        COMMENT "Creating release package"
    )
endif()
