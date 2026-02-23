cmake_minimum_required(VERSION 3.16)
set(APP json_pointer)

project(${APP} C)

# Generate the grammar library using the macro
epc_generate_grammar(
    TARGET json_pointer_grammar
    GDL_FILE ${CMAKE_CURRENT_SOURCE_DIR}/json_pointer.gdl
)

# Add the application executable
add_executable(${APP}
    main.c
    json_pointer_ast_actions.c
)

add_dependencies(${APP} all_unit_tests)

# Link against the generated library and easy_pc
target_link_libraries(${APP}
    PRIVATE
    easy_pc
    json_pointer_grammar
)

# Include directories for easy_pc
target_include_directories(${APP}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
