# Easy PC CMake Macros

function(epc_generate_grammar)
    set(options)
    set(oneValueArgs TARGET GDL_FILE OUTPUT_DIR)
    set(multiValueArgs)
    cmake_parse_arguments(EPC_GEN "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT EPC_GEN_TARGET)
        message(FATAL_ERROR "EPC_GENERATE_GRAMMAR: TARGET argument is required")
    endif()

    if(NOT EPC_GEN_GDL_FILE)
        message(FATAL_ERROR "EPC_GENERATE_GRAMMAR: GDL_FILE argument is required")
    endif()

    if(NOT EPC_GEN_OUTPUT_DIR)
        set(EPC_GEN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
    endif()

    file(MAKE_DIRECTORY "${EPC_GEN_OUTPUT_DIR}")

    # Get the base name of the GDL file (without extension)
    get_filename_component(GDL_BASE_NAME "${EPC_GEN_GDL_FILE}" NAME_WE)

    set(GENERATED_C "${EPC_GEN_OUTPUT_DIR}/${GDL_BASE_NAME}.c")
    set(GENERATED_H "${EPC_GEN_OUTPUT_DIR}/${GDL_BASE_NAME}.h")
    set(GENERATED_ACTIONS_H "${EPC_GEN_OUTPUT_DIR}/${GDL_BASE_NAME}_actions.h")

    # Add custom command to run gdl_compiler
    add_custom_command(
        OUTPUT "${GENERATED_C}" "${GENERATED_H}" "${GENERATED_ACTIONS_H}"
        COMMAND gdl_compiler "${EPC_GEN_GDL_FILE}" "--output-dir=${EPC_GEN_OUTPUT_DIR}"
        DEPENDS "${EPC_GEN_GDL_FILE}" gdl_compiler
        VERBATIM
        COMMENT "Generating parser code for ${GDL_BASE_NAME}.gdl"
    )

    # Create a library with the generated sources
    add_library("${EPC_GEN_TARGET}" STATIC "${GENERATED_C}")
    target_include_directories("${EPC_GEN_TARGET}" PUBLIC "${EPC_GEN_OUTPUT_DIR}")
    target_link_libraries("${EPC_GEN_TARGET}" PUBLIC easy_pc)

    # Ensure gdl_compiler is built before this library
    add_dependencies("${EPC_GEN_TARGET}" gdl_compiler)

endfunction()
