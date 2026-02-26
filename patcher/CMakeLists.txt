# OSDMenu Patcher

# DISCLAIMER: By disabling the Free McBoot splash screen, the user acknowledges
# and agrees that they are in violation of Rule 2 of the Free McBoot License.
# The user accepts full responsibility for any legal actions that may be taken
# as a result of this violation, including but not limited to being sued.
# Proceed at your own risk, and may the legal forces be ever in your favor.
option(PATCHER_ENABLE_SPLASH "Enable Free McBoot splash screen" ON)
if(NOT DEFINED PATCHER_HOSD)
    set(PATCHER_HOSD OFF)
endif()

# Set variant-specific variables based on PATCHER_HOSD
if(PATCHER_HOSD)
    set(PATCHER_BINARY_DIR "${CMAKE_BINARY_DIR}/hosdmenu")
    set(patcher_unc_target "hosdmenu_unc")
    set(patcher_target "hosdmenu")
else()
    set(PATCHER_BINARY_DIR "${CMAKE_BINARY_DIR}/osdmenu")
    set(patcher_unc_target "osdmenu_unc")
    set(patcher_target "osdmenu")
endif()

set(PATCHER_CNF_FILE "" CACHE STRING "Path to embedded CNF file")
set(PATCHER_KELF_TYPE "fmcb" CACHE STRING "KELF type (fmcb, etc.)")

# Get git version
find_package(Git)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --always --dirty --tags --exclude nightly
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
else()
    set(GIT_VERSION "unknown")
endif()

set(PATCHER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/patcher")
set(EE_SOURCES
    ${PATCHER_SOURCE_DIR}/src/main.c
    ${PATCHER_SOURCE_DIR}/src/settings.c
    ${PATCHER_SOURCE_DIR}/src/init.c
    ${PATCHER_SOURCE_DIR}/src/launcher.c
    ${PATCHER_SOURCE_DIR}/src/patches_common.c
    ${PATCHER_SOURCE_DIR}/src/patches_fmcb.c
    ${PATCHER_SOURCE_DIR}/src/gs.c
    ${PATCHER_SOURCE_DIR}/src/patches_browser.c
    ${PATCHER_SOURCE_DIR}/src/patches_gs.c
    ${PATCHER_SOURCE_DIR}/src/patches_version.c
    ${PATCHER_SOURCE_DIR}/src/patches_ps1drv.c
    ${PATCHER_SOURCE_DIR}/src/patches_osd.c
)

# HOSDMenu build
set(EE_LIBS)
if(PATCHER_HOSD)
    list(APPEND EE_LIBS fileXio)
    set(IRX_FILES
        iomanX.irx
        fileXio.irx
        ps2dev9.irx
        ps2atad.irx
        ps2hdd-osd.irx
        ps2fs.irx
        legacy_ps2atad.irx
    )
    # Build the legacy atad module
    add_custom_target(legacy_ps2atad
        COMMAND ${CMAKE_COMMAND} -E chdir "${PATCHER_SOURCE_DIR}/iop/atad" ${CMAKE_MAKE_PROGRAM}
        BYPRODUCTS "${PATCHER_SOURCE_DIR}/iop/atad/ps2atad.irx"
        COMMENT "Building legacy ps2atad"
    )
    set(ps2atad_target "legacy_ps2atad")
else()
    set(IRX_FILES)
endif()

set(ELF_FILES launcher.elf)

# Splash screen
if(PATCHER_ENABLE_SPLASH)
    list(APPEND EE_SOURCES ${PATCHER_SOURCE_DIR}/src/splash.c)
    list(APPEND EE_LIBS dma)
endif()

# Process IRX files
set(IRX_SOURCES)
foreach(irx ${IRX_FILES})
    get_filename_component(irx_name ${irx} NAME_WE)
    string(REPLACE "-" "_" irx_name_clean ${irx_name})

    if(irx_name STREQUAL "legacy_ps2atad")
        # The old ps2atad is not a part of PS2SDK
        set(ps2atad_irx_path "${PATCHER_SOURCE_DIR}/iop/atad/ps2atad.irx")
        set(irx_source "${PATCHER_BINARY_DIR}/legacy_ps2atad_irx.c")
        set(temp_file "${PATCHER_BINARY_DIR}/legacy_ps2atad_irx.c.tmp")
        add_custom_command(
            OUTPUT ${irx_source}
            COMMAND ${BIN2C} "${ps2atad_irx_path}" "${temp_file}" "legacy_ps2atad_irx"
            COMMAND ${CMAKE_COMMAND} -P "${CMAKE_SOURCE_DIR}/cmake/patch_section.cmake" "${temp_file}" "${irx_source}" "._legacy_atad"
            DEPENDS ${ps2atad_target} "${ps2atad_irx_path}"
            COMMENT "Converting IRX legacy_ps2atad.irx to C array with section ._legacy_atad"
            VERBATIM
        )
    else()
        irx_to_source("$ENV{PS2SDK}/iop/irx/${irx}" irx_source "${irx_name_clean}_irx" "._irx")
    endif()

    list(APPEND IRX_SOURCES ${irx_source})
endforeach()

# Build launcher.elf
# Requires custom command because launcher target is uncompressed
set(ELF_SOURCES)
get_target_property(launcher_binary_dir launcher BINARY_DIR)
set(packed_launcher_elf "${launcher_binary_dir}/launcher.elf")
set(launcher_source "${PATCHER_BINARY_DIR}/launcher_elf.c")
set(launcher_temp "${PATCHER_BINARY_DIR}/launcher_elf.c.tmp")
add_custom_command(
    OUTPUT ${launcher_source}
    COMMAND ${BIN2C} "${packed_launcher_elf}" "${launcher_temp}" "launcher_elf"
    COMMAND ${CMAKE_COMMAND} -P "${CMAKE_SOURCE_DIR}/cmake/patch_section.cmake" "${launcher_temp}" "${launcher_source}" "._launcher"
    DEPENDS launcher "${packed_launcher_elf}"
    COMMENT "Converting ELF launcher.elf to C array with section ._launcher"
    VERBATIM
)
list(APPEND ELF_SOURCES ${launcher_source})

# Add embedded CNF file
if(NOT PATCHER_HOSD AND PATCHER_CNF_FILE)
    get_filename_component(cnf_file "${PATCHER_CNF_FILE}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
    if(NOT EXISTS "${cnf_file}")
        message(FATAL_ERROR "CNF file not found: ${cnf_file}")
    endif()

    get_filename_component(cnf_name "${cnf_file}" NAME)
    set(cnf_source "${PATCHER_BINARY_DIR}/${cnf_name}.c")
    set(cnf_temp "${PATCHER_BINARY_DIR}/${cnf_name}.c.tmp")

    add_custom_command(
        OUTPUT ${cnf_source}
        COMMAND ${BIN2C} "${cnf_file}" "${cnf_temp}" "embedded_cnf"
        COMMAND ${CMAKE_COMMAND} -P "${CMAKE_SOURCE_DIR}/cmake/patch_section.cmake" "${cnf_temp}" "${cnf_source}" "._cnf"
        DEPENDS "${cnf_file}"
        COMMENT "Converting CNF file to C array with section ._cnf"
        VERBATIM
    )
    set(CNF_FILE ${cnf_source})
endif()

add_executable(${patcher_unc_target} ${EE_SOURCES} ${IRX_SOURCES} ${ELF_SOURCES} ${CNF_FILE})

# Add dependencies on IOP modules
if(PATCHER_HOSD)
    add_dependencies(${patcher_unc_target} ${ps2atad_target})
endif()
target_include_directories(${patcher_unc_target} PRIVATE
    ${CMAKE_SOURCE_DIR}/common/include
    ${PATCHER_SOURCE_DIR}/include
)

target_compile_options(${patcher_unc_target} PRIVATE
    -O2
    -G0
    -Wall
    -fdata-sections
    -ffunction-sections
)

target_compile_definitions(${patcher_unc_target} PRIVATE
    GIT_VERSION="${GIT_VERSION}"
)
if(PATCHER_HOSD)
    target_compile_definitions(${patcher_unc_target} PRIVATE HOSD)
endif()
if(NOT PATCHER_HOSD AND PATCHER_CNF_FILE)
    target_compile_definitions(${patcher_unc_target} PRIVATE EMBED_CNF)
endif()
if(PATCHER_ENABLE_SPLASH)
    target_compile_definitions(${patcher_unc_target} PRIVATE ENABLE_SPLASH)
endif()

# Use custom linker script
target_link_options(${patcher_unc_target} PRIVATE
    -T${PATCHER_SOURCE_DIR}/linkfile
)

set(PATCHER_LINK_LIBS
    patches
    dma
    cdvd
    cglue
    kernel
    pthread
    pthreadglue
)
# Add fileXio only for HOSDMenu build
if(PATCHER_HOSD)
    list(APPEND PATCHER_LINK_LIBS fileXio)
endif()

link_newlib_nano(${patcher_unc_target} ${PATCHER_LINK_LIBS})

if(DEBUG)
    set(patcher_link_flags "-Wl,--gc-sections")
else()
    set(patcher_link_flags "-Wl,--gc-sections -s")
endif()
set_target_properties(${patcher_unc_target} PROPERTIES
    LINK_FLAGS "${patcher_link_flags}"
)

# Pack ELF
add_custom_target(${patcher_target} ALL
    DEPENDS ${patcher_unc_target}
    COMMAND ${PS2_PACKER} "$<TARGET_FILE:${patcher_unc_target}>" "${PATCHER_BINARY_DIR}/${patcher_target}.elf"
    COMMENT "Packing ${patcher_target}.elf"
)

set_target_properties(${patcher_target} PROPERTIES
    OUTPUT_NAME "${patcher_target}"
)

# Build OSDMenu KELF if environment variables are set
if(NOT PATCHER_HOSD AND DEFINED ENV{KELFSERVER_API_ADDRESS} AND DEFINED ENV{KELFSERVER_API_KEY})
    add_custom_command(
        TARGET ${patcher_target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E env
            KELFSERVER_API_ADDRESS=$ENV{KELFSERVER_API_ADDRESS}
            KELFSERVER_API_KEY=$ENV{KELFSERVER_API_KEY}
            ${PYTHON3} "${CMAKE_SOURCE_DIR}/utils/scripts/kelfsign.py" "${PATCHER_KELF_TYPE}" "${PATCHER_BINARY_DIR}/${patcher_target}.elf" "${PATCHER_BINARY_DIR}/OSDMENU.XLF"
        COMMENT "Creating KELF file"
    )
    message(STATUS "OSDMenu KELF build enabled")
endif()

set_target_properties(${patcher_unc_target} PROPERTIES
    OUTPUT_NAME "${patcher_unc_target}"
    RUNTIME_OUTPUT_NAME "${patcher_unc_target}.elf"
    RUNTIME_OUTPUT_DIRECTORY "${PATCHER_BINARY_DIR}"
)
set_target_properties(${patcher_target} PROPERTIES
    OUTPUT_NAME "${patcher_target}"
    RUNTIME_OUTPUT_DIRECTORY "${PATCHER_BINARY_DIR}"
)
