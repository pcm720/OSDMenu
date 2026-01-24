# Embeddable ELF loader

set(SOURCES
    src/loader.c
)

add_subdirectory(../egsm ${CMAKE_BINARY_DIR}/utils/egsm)

add_executable(loader ${SOURCES})
target_include_directories(loader PRIVATE
    ../egsm/include
    $ENV{PS2SDK}/ee/include
    $ENV{PS2SDK}/common/include
)

target_compile_options(loader PRIVATE
    -Os
    -G0
    -Wall
    -fdata-sections
    -ffunction-sections
)

# Use custom linker script
target_link_options(loader PRIVATE
    -T${CMAKE_CURRENT_SOURCE_DIR}/linkfile
)

link_newlib_nano(loader
    fileXio
    iopreboot
    patches
    kernel
    cdvd
    cglue
    pthread
    pthreadglue
)

set_target_properties(loader PROPERTIES
    LINK_FLAGS "-Wl,--gc-sections -s"
    OUTPUT_NAME "loader"
    RUNTIME_OUTPUT_NAME "loader.elf"
)

# Add eGSM ELF as dependency
elf_to_source(egsm egsm_source egsm_elf "")
add_dependencies(loader egsm)
target_sources(loader PRIVATE ${egsm_source})
target_compile_definitions(loader PRIVATE EGSM_ELF_SIZE)
