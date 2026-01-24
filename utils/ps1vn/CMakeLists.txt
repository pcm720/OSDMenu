# PS1 Video Mode Negator

set(SOURCES
    src/main.c
    src/patches.c
)

add_executable(ps1vn ${SOURCES})

target_include_directories(ps1vn PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    $ENV{PS2SDK}/ee/include
    $ENV{PS2SDK}/common/include
)

target_compile_options(ps1vn PRIVATE
    -Os
    -G0
    -Wall
    -fdata-sections
    -ffunction-sections
)

# Use custom linker script
target_link_options(ps1vn PRIVATE
    -T${CMAKE_CURRENT_SOURCE_DIR}/linkfile
)

link_newlib_nano(ps1vn
    cdvd
    cglue
    pthread
    pthreadglue
    kernel
)

set_target_properties(ps1vn PROPERTIES
    LINK_FLAGS "-Wl,--gc-sections -s"
    OUTPUT_NAME "ps1vn"
    RUNTIME_OUTPUT_NAME "ps1vn.elf"
)
