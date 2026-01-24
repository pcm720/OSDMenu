# Helper functions

# bin2c
function(bin2c_source input_file output_var symbol_name)
    get_filename_component(output_dir "${CMAKE_CURRENT_BINARY_DIR}" ABSOLUTE)
    get_filename_component(output_name "${input_file}" NAME)
    set(output_file "${output_dir}/${output_name}.c")

    add_custom_command(
        OUTPUT "${output_file}"
        COMMAND ${BIN2C} "${input_file}" "${output_file}" "${symbol_name}"
        DEPENDS "${input_file}"
        COMMENT "Converting ${input_file} to C array ${symbol_name}"
        VERBATIM
    )

    set(${output_var} "${output_file}" PARENT_SCOPE)
endfunction()

# IRX to C file with custom section
function(irx_to_source irx_file output_var symbol_name section_name)
    if(DEFINED PATCHER_BINARY_DIR)
        get_filename_component(output_dir "${PATCHER_BINARY_DIR}" ABSOLUTE)
    else()
        get_filename_component(output_dir "${CMAKE_CURRENT_BINARY_DIR}" ABSOLUTE)
    endif()
    get_filename_component(irx_name "${irx_file}" NAME)
    set(output_file "${output_dir}/${irx_name}.c")
    set(temp_file "${output_dir}/${irx_name}.c.tmp")

    if(NOT IS_ABSOLUTE "${irx_file}")
        set(actual_irx "$ENV{PS2SDK}/iop/irx/${irx_file}")
    else()
        set(actual_irx "${irx_file}")
    endif()

    if(section_name)
        add_custom_command(
            OUTPUT "${output_file}"
            COMMAND ${BIN2C} "${actual_irx}" "${temp_file}" "${symbol_name}"
            COMMAND ${CMAKE_COMMAND} -P "${CMAKE_SOURCE_DIR}/cmake/patch_section.cmake" "${temp_file}" "${output_file}" "${section_name}"
            DEPENDS "${actual_irx}"
            COMMENT "Converting IRX ${irx_name} to C array ${symbol_name} with section ${section_name}"
            VERBATIM
        )
    else()
        add_custom_command(
            OUTPUT "${output_file}"
            COMMAND ${BIN2C} "${actual_irx}" "${output_file}" "${symbol_name}"
            DEPENDS "${actual_irx}"
            COMMENT "Converting IRX ${irx_name} to C array ${symbol_name}"
            VERBATIM
        )
    endif()

    set(${output_var} "${output_file}" PARENT_SCOPE)
endfunction()

# ELF to C file with custom section
function(elf_to_source elf_target output_var symbol_name section_name)
    if(DEFINED PATCHER_BINARY_DIR)
        get_filename_component(output_dir "${PATCHER_BINARY_DIR}" ABSOLUTE)
    else()
        get_filename_component(output_dir "${CMAKE_CURRENT_BINARY_DIR}" ABSOLUTE)
    endif()

    get_target_property(elf_name ${elf_target} OUTPUT_NAME)

    set(output_file "${output_dir}/${elf_name}_${symbol_name}.c")
    set(temp_file "${output_dir}/${elf_name}_${symbol_name}.c.tmp")

    set(actual_elf "$<TARGET_FILE:${elf_target}>")

    if(section_name)
        add_custom_command(
            OUTPUT "${output_file}"
            COMMAND ${BIN2C} "${actual_elf}" "${temp_file}" "${symbol_name}"
            COMMAND ${CMAKE_COMMAND} -P "${CMAKE_SOURCE_DIR}/cmake/patch_section.cmake" "${temp_file}" "${output_file}" "${section_name}"
            DEPENDS ${elf_target}
            COMMENT "Converting ELF ${elf_name}.elf to C array ${symbol_name} with section ${section_name}"
            VERBATIM
        )
    else()
        add_custom_command(
            OUTPUT "${output_file}"
            COMMAND ${BIN2C} "$<TARGET_FILE:${elf_target}>" "${output_file}" "${symbol_name}"
            DEPENDS ${elf_target}
            COMMENT "Converting ELF ${elf_name} to C array ${symbol_name}"
            VERBATIM
        )
    endif()

    set(${output_var} "${output_file}" PARENT_SCOPE)
endfunction()

# Pack ELF with ps2-packer
function(pack_elf target_name unpacked_target)
    get_target_property(target_dir ${unpacked_target} BINARY_DIR)
    get_target_property(target_name_base ${unpacked_target} OUTPUT_NAME)
    if(NOT target_name_base)
        get_target_property(target_name_base ${unpacked_target} NAME)
    endif()

    set(unpacked_elf "$<TARGET_FILE:${unpacked_target}>")
    set(packed_elf "${target_dir}/${target_name_base}.elf")

    find_program(PS2_PACKER NAMES ps2-packer PATHS $ENV{PS2DEV}/bin NO_DEFAULT_PATH)
    if(NOT PS2_PACKER)
        set(PS2_PACKER "$ENV{PS2DEV}/bin/ps2-packer")
    endif()

    add_custom_command(
        TARGET ${target_name} POST_BUILD
        COMMAND ${PS2_PACKER} "${unpacked_elf}" "${packed_elf}"
        COMMENT "Packing ELF ${unpacked_elf} to ${packed_elf}"
        VERBATIM
    )

    set_target_properties(${target_name} PROPERTIES
        PACKED_ELF_PATH "${packed_elf}"
    )
endfunction()

# Create KELF with utils/sign.py
function(create_kelf target_name packed_elf kelf_type)
    get_target_property(target_dir ${target_name} BINARY_DIR)
    get_target_property(target_name_base ${target_name} OUTPUT_NAME)
    if(NOT target_name_base)
        set(target_name_base "${target_name}")
    endif()

    if(NOT DEFINED ENV{KELFSERVER_API_ADDRESS})
        message(FATAL_ERROR "KELFSERVER_API_ADDRESS environment variable is not set. Please set it before building KELF files.")
    endif()
    if(NOT DEFINED ENV{KELFSERVER_API_KEY})
        message(FATAL_ERROR "KELFSERVER_API_KEY environment variable is not set. Please set it before building KELF files.")
    endif()

    add_custom_command(
        TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E env
            KELFSERVER_API_ADDRESS=$ENV{KELFSERVER_API_ADDRESS}
            KELFSERVER_API_KEY=$ENV{KELFSERVER_API_KEY}
            ${PYTHON3} "${CMAKE_SOURCE_DIR}/utils/kelf/sign.py" "${kelf_type}" "${packed_elf}" "${kelf_file}"
        COMMENT "Creating KELF ${kelf_file} from ${packed_elf}"
        VERBATIM
    )
endfunction()

# Links newlib-nano instead of newlib
function(link_newlib_nano target_name)
    set(LIB_LIST "c_nano")

    foreach(LIB ${ARGN})
        if(LIB MATCHES "\\.a$")
            list(APPEND LIB_LIST "${LIB}")
        else()
            list(APPEND LIB_LIST "${LIB}")
        endif()
    endforeach()

    target_link_libraries(${target_name} PRIVATE
        -nodefaultlibs
        -lm_nano
        -lgcc
        -Wl,--start-group
        ${LIB_LIST}
        -Wl,--end-group
    )
endfunction()
