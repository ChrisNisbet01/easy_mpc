cmake_minimum_required(VERSION 3.16)
project(code_gen C)

# Set output directory for generated files
set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

set(GDL_COMPILER_EXECUTABLE ${CMAKE_SOURCE_DIR}/build/tools/gdl_compiler/gdl_compiler)
set(GDL_INPUT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/arithmetic_expression.gdl)
set(GDL_OUTPUT_ARGS "--output-dir=${GENERATED_DIR}")

message(STATUS "GDL Compiler Command: ${GDL_COMPILER_EXECUTABLE} ${GDL_INPUT_FILE} ${GDL_OUTPUT_ARGS}")

# Add custom command to run gdl_compiler
add_custom_command(
    OUTPUT ${GENERATED_DIR}/arithmetic_expression.h
           ${GENERATED_DIR}/arithmetic_expression.c
           ${GENERATED_DIR}/arithmetic_expression_actions.h
    COMMAND ${GDL_COMPILER_EXECUTABLE} ${GDL_INPUT_FILE} ${GDL_OUTPUT_ARGS}
    DEPENDS ${GDL_INPUT_FILE} ${GDL_COMPILER_EXECUTABLE}
    VERBATIM
    COMMENT "Generating parser code for arithmetic_expression_language.gdl"
)

# Add the generated files to the source group for easier handling
# in IDEs and to ensure they are processed by CMake
add_library(arithmetic_expression_generated_sources
    STATIC
    ${GENERATED_DIR}/arithmetic_expression.c
)
target_include_directories(arithmetic_expression_generated_sources
    PUBLIC
    ${GENERATED_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

# Explicitly ensure gdl_compiler is built before the custom command runs
add_dependencies(arithmetic_expression_generated_sources 
  gdl_compiler
)

# Add the application executable
add_executable(code_gen main.c)

# Link against the generated library and easy_pc
target_link_libraries(code_gen
    PRIVATE
    easy_pc
    arithmetic_expression_generated_sources
)

# Include directories for generated files and easy_pc
target_include_directories(code_gen
    PRIVATE
    ${GENERATED_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

# Add our custom example main.c
target_sources(code_gen PRIVATE main.c)
