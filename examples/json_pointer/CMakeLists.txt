cmake_minimum_required(VERSION 3.16)
project(json_pointer_example C)

# Set output directory for generated files
set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

set(GDL_COMPILER_EXECUTABLE ${CMAKE_SOURCE_DIR}/build/tools/gdl_compiler/gdl_compiler)
set(GDL_INPUT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/json_pointer.gdl)
set(GDL_OUTPUT_ARGS "--output-dir=${GENERATED_DIR}")

message(STATUS "GDL Compiler Command: ${GDL_COMPILER_EXECUTABLE} ${GDL_INPUT_FILE} ${GDL_OUTPUT_ARGS}")

# Add custom command to run gdl_compiler
add_custom_command(
    OUTPUT ${GENERATED_DIR}/json_pointer.h
           ${GENERATED_DIR}/json_pointer.c
           ${GENERATED_DIR}/json_pointer_actions.h
    COMMAND ${GDL_COMPILER_EXECUTABLE} ${GDL_INPUT_FILE} ${GDL_OUTPUT_ARGS}
    DEPENDS ${GDL_INPUT_FILE} ${GDL_COMPILER_EXECUTABLE}
    VERBATIM
    COMMENT "Generating parser code for json_pointer.gdl"
)

# Add the generated files to a library
add_library(json_pointer_generated_sources
    STATIC
    ${GENERATED_DIR}/json_pointer.c
)
target_include_directories(json_pointer_generated_sources
    PUBLIC
    ${GENERATED_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

# Explicitly ensure gdl_compiler is built before the custom command runs
add_dependencies(json_pointer_generated_sources 
  gdl_compiler
)

# Add the application executable
add_executable(json_pointer_example main.c)

# Link against the generated library and easy_pc
target_link_libraries(json_pointer_example
    PRIVATE
    easy_pc
    json_pointer_generated_sources
)

# Include directories for generated files and easy_pc
target_include_directories(json_pointer_example
    PRIVATE
    ${GENERATED_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

# Add our custom example main.c
target_sources(json_pointer_example PRIVATE main.c)
