# eGSM

set(SOURCES
    src/main.c
    src/ee_exception_l2.S
)

add_executable(egsm ${SOURCES})
target_include_directories(egsm PRIVATE
    include
    $ENV{PS2SDK}/ee/include
    $ENV{PS2SDK}/common/include
)

target_compile_options(egsm PRIVATE
    -Os
    -G0
    -Wall
    -fdata-sections
    -ffunction-sections
)

# Use custom linker script
target_link_options(egsm PRIVATE
    -T${CMAKE_CURRENT_SOURCE_DIR}/linkfile
)

link_newlib_nano(egsm
    kernel
)

set_target_properties(egsm PROPERTIES
    LINK_FLAGS "-Wl,--gc-sections -s"
    OUTPUT_NAME "egsm"
    RUNTIME_OUTPUT_NAME "egsm.elf"
)
