# CMakeLists.txt for tools/gdl_compiler
set(app "gdl_compiler")

# Define the executable for the GDL compiler
add_executable(${app} main.c gdl_parser.c gdl_compiler_ast_actions.c gdl_code_generator.c gdl_bootstrap_generator.c)
target_compile_options(${app} PRIVATE -Wall -Wextra -pedantic)

add_dependencies(${app} all_unit_tests)

# Link the GDL compiler against the easy_pc library
target_link_libraries(${app} PRIVATE easy_pc_shared)

# Set include directories for easy_pc headers
target_include_directories(${app} PRIVATE ${CMAKE_SOURCE_DIR}/include)
