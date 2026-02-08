# Helper script to patch section attribute in bin2c output
# Usage: cmake -P patch_section.cmake <input_file> <output_file> <section_name>

set(INPUT_FILE "${CMAKE_ARGV3}")
set(OUTPUT_FILE "${CMAKE_ARGV4}")
set(SECTION_NAME "${CMAKE_ARGV5}")

if(NOT INPUT_FILE OR NOT OUTPUT_FILE OR NOT SECTION_NAME)
    message(FATAL_ERROR "Usage: cmake -P patch_section.cmake <input_file> <output_file> <section_name>")
endif()

file(READ "${INPUT_FILE}" CONTENT)
string(REPLACE "= {" "__attribute__((section(\"${SECTION_NAME}\"))) = {" CONTENT "${CONTENT}")
file(WRITE "${OUTPUT_FILE}" "${CONTENT}")
