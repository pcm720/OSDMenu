# OSDMenu MBR Installer

set(SOURCES
    src/main.c
    src/init.c
)

set(IRX_FILES
    iomanX.irx
    fileXio.irx
    ps2dev9.irx
    ps2atad.irx
    ps2fs.irx
    ps2hdd-osd.irx
)

set(IRX_SOURCES)
foreach(irx ${IRX_FILES})
    get_filename_component(irx_name ${irx} NAME_WE)
    string(REPLACE "-" "_" irx_name_clean ${irx_name})
    irx_to_source("$ENV{PS2SDK}/iop/irx/${irx}" irx_source "${irx_name_clean}_irx" "")
    list(APPEND IRX_SOURCES ${irx_source})
endforeach()

# Embed the default MBR config file
bin2c_source("${CMAKE_CURRENT_SOURCE_DIR}/config/osdmbr.cnf" osdmbrcnf_source "osdmbrcnf_bin")

# OSDMBR.XLF is created in the mbr build directory
get_target_property(mbr_binary_dir osdmbr BINARY_DIR)
set(osdmbr_kelf_file "${mbr_binary_dir}/OSDMBR.XLF")

# KELF environment variables should be set if this subdirectory is included
if(DEFINED ENV{KELFSERVER_API_ADDRESS} AND DEFINED ENV{KELFSERVER_API_KEY})
    # KELF will be built, depend on osdmbr target
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/payload_bin.c"
        COMMAND ${BIN2C} "${osdmbr_kelf_file}" "${CMAKE_CURRENT_BINARY_DIR}/payload_bin.c" "payload_bin"
        DEPENDS osdmbr "${osdmbr_kelf_file}"
        COMMENT "Converting OSDMBR.XLF to payload_bin"
    )
else()
    message(FATAL_ERROR "KELF environment variables are not set. The installer requires OSDMBR.XLF, created only when KELF signing is enabled.")
endif()

add_executable(osdmbr-installer_unc ${SOURCES} ${IRX_SOURCES} ${ps2hdd_source} ${osdmbrcnf_source})
target_sources(osdmbr-installer_unc PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/payload_bin.c")
target_include_directories(osdmbr-installer_unc PRIVATE include)

target_compile_options(osdmbr-installer_unc PRIVATE
    -O2
    -G0
    -Wall
)

link_newlib_nano(osdmbr-installer_unc
    patches
    libdebug.a
    fileXio
    poweroff
    cdvd
    kernel
    cglue
    pthread
    pthreadglue
)

if(DEBUG)
    set(installer_link_flags "")
else()
    set(installer_link_flags "-s")
endif()
set_target_properties(osdmbr-installer_unc PROPERTIES
    LINK_FLAGS "${installer_link_flags}"
    OUTPUT_NAME "osdmbr-installer_unc"
    RUNTIME_OUTPUT_NAME "osdmbr-installer_unc.elf"
)

add_custom_target(osdmbr-installer ALL
    DEPENDS osdmbr-installer_unc
    COMMAND ${PS2_PACKER} "$<TARGET_FILE:osdmbr-installer_unc>" "${CMAKE_CURRENT_BINARY_DIR}/osdmbr-installer.elf"
    COMMENT "Packing osdmbr-installer.elf"
)
