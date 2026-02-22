set(app "simple_calc")

add_executable(${app} 
  main.c 
  simple_calc_ast_actions.c 
  ast_evaluator.c 
  function_definitions.c
  grammar.c
)

target_compile_options(${app} PRIVATE -Wall -Wextra -pedantic -Werror)

target_link_libraries(${app} PRIVATE
    easy_pc
    m # Link with the math library
)

target_include_directories(${app} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} # For ast.h, ast_evaluator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

target_compile_features(${app} PRIVATE c_std_11) # Enable C11 features

