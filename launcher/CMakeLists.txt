# OSDMenu Launcher

# Launcher configuration options
option(LAUNCHER_OSDM "Support for handling OSDMenu arguments" ON)
option(LAUNCHER_MMCE "Support for loading ELFs from MMCE" ON)
option(LAUNCHER_USB "Support for loading ELFs from USB" ON)
option(LAUNCHER_ATA "Support for loading ELFs from exFAT internal HDD" ON)
option(LAUNCHER_MX4SIO "Support for loading ELFs from MX4SIO" ON)
option(LAUNCHER_APA "Support for loading ELFs from APA-formatted HDD" ON)
option(LAUNCHER_CDROM "Support for loading PS1 and PS2 CDs/DVDs" ON)
option(LAUNCHER_ILINK "Support for loading ELFs from iLink" OFF)
option(LAUNCHER_UDPBD "Support for loading ELFs from UDPBD" OFF)

# Base sources
set(EE_SOURCES
    src/main.c
    src/common.c
    src/init.c
    src/handler_mc.c
    src/handler_quickboot.c
)

# Common files
list(APPEND EE_SOURCES
    ../common/src/history.c
    ../common/src/game_id.c
    ../common/src/cnf.c
    ../common/src/loader.c
)

# IRX files
set(IRX_FILES
    iomanX.irx
    fileXio.irx
    sio2man.irx
    mcman.irx
    mcserv.irx
)

# ELF files
set(ELF_FILES
    loader.elf
)

set(EE_LIBS
    cdvd
    patches
    kernel
    libdebug.a
    fileXio
    gskit
    dmakit
    mc
    poweroff
    secr
)

# Base modules
set(IRX_SOURCES)
foreach(irx ${IRX_FILES})
    get_filename_component(irx_name ${irx} NAME_WE)
        irx_to_source("$ENV{PS2SDK}/iop/irx/${irx}" irx_source "${irx_name}_irx" "")
    list(APPEND IRX_SOURCES ${irx_source})
endforeach()

# OSDM support
if(LAUNCHER_OSDM)
    list(APPEND EE_SOURCES src/handler_osdm.c)
    list(APPEND IRX_FILES poweroff.irx)
    add_compile_definitions(OSDM)
    set(LAUNCHER_CDROM ON)  # OSDM implies CDROM
endif()

# MMCE support
if(LAUNCHER_MMCE)
    list(APPEND IRX_FILES mmceman.irx)
    add_compile_definitions(MMCE)
    set(SIO2MAN ON)

    # Build mmceman from submodule
    set(MMCEMAN_IRX "${CMAKE_CURRENT_BINARY_DIR}/mmceman.irx")
    file(GLOB MMCEMAN_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/iop/mmceman/mmceman/src/*" "${CMAKE_CURRENT_SOURCE_DIR}/iop/mmceman/mmceman/Makefile")
    add_custom_command(
        OUTPUT ${MMCEMAN_IRX}
        COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_CURRENT_SOURCE_DIR}/iop/mmceman/mmceman" ${CMAKE_MAKE_PROGRAM}
        COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/iop/mmceman/mmceman/irx/mmceman.irx"
            ${MMCEMAN_IRX}
        DEPENDS ${MMCEMAN_SOURCES}
        COMMENT "Building mmceman"
        VERBATIM
    )
endif()

# USB support
if(LAUNCHER_USB)
    list(APPEND EE_SOURCES src/handler_bdm.c)
    list(APPEND IRX_FILES usbd_mini.irx usbmass_bd_mini.irx)
    add_compile_definitions(USB)
    set(BDM ON)
endif()

# APA support
if(LAUNCHER_APA)
    list(APPEND EE_SOURCES src/handler_pfs.c src/handler_patinfo.c ../common/src/patinfo.c)
    list(APPEND IRX_FILES ps2hdd-osd.irx ps2fs.irx secrsif.irx)
    add_compile_definitions(APA)
    set(LAUNCHER_ATA ON)  # APA implies ATA
    set(DEV9 ON)
endif()

# ATA support
if(LAUNCHER_ATA)
    list(APPEND EE_SOURCES src/handler_bdm.c)
    list(APPEND IRX_FILES ata_bd.irx)
    add_compile_definitions(ATA)
    set(BDM ON)
    set(DEV9 ON)
endif()

# MX4SIO support
if(LAUNCHER_MX4SIO)
    list(APPEND EE_SOURCES src/handler_bdm.c)
    list(APPEND IRX_FILES mx4sio_bd_mini.irx)
    add_compile_definitions(MX4SIO)
    set(SIO2MAN ON)
    set(BDM ON)
endif()

# iLink support
if(LAUNCHER_ILINK)
    list(APPEND EE_SOURCES src/handler_bdm.c)
    list(APPEND IRX_FILES iLinkman.irx IEEE1394_bd_mini.irx)
    add_compile_definitions(ILINK)
    set(BDM ON)
endif()

# UDPBD support
if(LAUNCHER_UDPBD)
    list(APPEND EE_SOURCES src/handler_bdm.c)
    add_compile_definitions(UDPBD)
    set(BDM ON)
    set(DEV9 ON)

    # Build smap_udpbd
    list(APPEND IRX_FILES smap_udpbd.irx)
    add_custom_target(smap_udpbd
        COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_CURRENT_SOURCE_DIR}/iop/smap_udpbd" ${CMAKE_MAKE_PROGRAM}
        COMMENT "Building smap_udpbd"
    )
endif()

# CDROM support
if(LAUNCHER_CDROM)
    list(APPEND EE_SOURCES src/handler_cdrom.c)
    list(APPEND IRX_FILES xparam.irx)
    list(APPEND ELF_FILES ps1vn.elf)
    add_compile_definitions(CDROM)
endif()

# Driver dependencies
if(DEV9)
    list(APPEND IRX_FILES ps2dev9.irx)
endif()

if(BDM)
    list(APPEND EE_SOURCES src/handler_bdm.c)
    list(APPEND IRX_FILES bdm.irx bdmfs_fatfs.irx)
endif()

if(ENABLE_PRINTF)
    add_compile_definitions(ENABLE_PRINTF UDPTTYIP="${UDPTTY_IP}")
    if (NOT LAUNCHER_UDPBD)
        list(APPEND IRX_FILES smap_udptty.irx)
    endif()
endif()

# Process all IRX files
foreach(irx ${IRX_FILES})
    get_filename_component(irx_name ${irx} NAME_WE)
    string(REPLACE "-" "_" irx_name_clean ${irx_name})

    if(irx_name STREQUAL "smap_udpbd")
        # smap_udpbd is not a part of PS2SDK
        irx_to_source("${CMAKE_CURRENT_SOURCE_DIR}/iop/smap_udpbd/smap_udpbd.irx" irx_source "smap_udpbd_irx" "")
    elseif(irx_name STREQUAL "smap_udptty")
        # smap_udptty is not a part of PS2SDK
        irx_to_source("${CMAKE_CURRENT_SOURCE_DIR}/iop/smap_udptty/smap_udptty.irx" irx_source "smap_udptty_irx" "")
    elseif(irx_name STREQUAL "mmceman")
        # mmceman is built from submodule
        irx_to_source("${MMCEMAN_IRX}" irx_source "mmceman_irx" "")
    else()
        irx_to_source("$ENV{PS2SDK}/iop/irx/${irx}" irx_source "${irx_name_clean}_irx" "")
    endif()

    list(APPEND IRX_SOURCES ${irx_source})
endforeach()


# Convert ELF files to sources
elf_to_source(loader loader_source "loader_elf" "")
list(APPEND ELF_SOURCES ${loader_source})
if("ps1vn.elf" IN_LIST ELF_FILES)
    elf_to_source(ps1vn ps1vn_source "ps1vn_elf" "")
    list(APPEND ELF_SOURCES ${ps1vn_source})
endif()

# Resource files
set(RES_FILES
    icon_A.sys
    icon_C.sys
    icon_J.sys
)

set(RES_SOURCES)
foreach(res ${RES_FILES})
    get_filename_component(res_name ${res} NAME_WE)
    bin2c_source("${CMAKE_SOURCE_DIR}/common/res/${res}" res_source "${res_name}_sys")
    list(APPEND RES_SOURCES ${res_source})
endforeach()

# Create executable
add_executable(launcher_unc ${EE_SOURCES} ${IRX_SOURCES} ${ELF_SOURCES} ${RES_SOURCES})

# Add dependencies on IOP modules built via Makefiles
if(LAUNCHER_UDPBD)
    add_dependencies(launcher_unc smap_udpbd)
endif()
if(ENABLE_PRINTF)
    add_dependencies(launcher_unc smap_udptty)
endif()

# Add dependencies on ELF targets
add_dependencies(launcher_unc loader)
if("ps1vn.elf" IN_LIST ELF_FILES)
    add_dependencies(launcher_unc ps1vn)
endif()

target_include_directories(launcher_unc PRIVATE
    ../common/include
    include
)

target_compile_options(launcher_unc PRIVATE
    -Os
    -G0
    -Wall
)

# Use the default linker script
target_link_options(launcher_unc PRIVATE
    -T$ENV{PS2SDK}/ee/startup/linkfile
)

link_newlib_nano(launcher_unc
    cdvd
    patches
    kernel
    libdebug.a
    fileXio
    gskit
    dmakit
    mc
    poweroff
    secr
    cglue
    pthread
    pthreadglue
)

if(DEBUG)
    set(launcher_link_flags "")
else()
    set(launcher_link_flags "-s")
endif()
set_target_properties(launcher_unc PROPERTIES
    LINK_FLAGS "${launcher_link_flags}"
)

# Pack ELF
add_custom_target(launcher ALL
    DEPENDS launcher_unc
    COMMAND ${PS2_PACKER} "$<TARGET_FILE:launcher_unc>" "${CMAKE_CURRENT_BINARY_DIR}/launcher.elf"
    COMMENT "Packing launcher.elf"
)

set_target_properties(launcher PROPERTIES
    OUTPUT_NAME "launcher"
)

set_target_properties(launcher_unc PROPERTIES
    OUTPUT_NAME "launcher_unc"
    RUNTIME_OUTPUT_NAME "launcher_unc.elf"
)
