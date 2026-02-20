enable_testing()

add_executable(TerminalParsersTest 
    AllTests.cpp
    TerminalParsersTest.cpp
)

target_include_directories(TerminalParsersTest  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib
)

target_link_libraries(TerminalParsersTest  PRIVATE
    easy_pc
    CppUTest
    CppUTestExt
)

add_test(
    NAME TerminalParsersTest 
    COMMAND TerminalParsersTest 
)

add_executable(TerminalParsersNewTest
    AllTests.cpp
    TerminalParsersNewTest.cpp
)

target_include_directories(TerminalParsersNewTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib
)

target_link_libraries(TerminalParsersNewTest PRIVATE
    easy_pc
    CppUTest
    CppUTestExt
)

add_test(
    NAME TerminalParsersNewTest
    COMMAND TerminalParsersNewTest
)

add_executable(ErrorHandlingTest 
    AllTests.cpp
    ErrorHandlingTest.cpp
)

target_include_directories(ErrorHandlingTest  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib
)

target_link_libraries(ErrorHandlingTest  PRIVATE
    easy_pc
    CppUTest
    CppUTestExt
)

add_test(
    NAME ErrorHandlingTest 
    COMMAND ErrorHandlingTest 
)

add_executable(CptPrinterTest 
    AllTests.cpp
    CptPrinterTest.cpp
)

target_include_directories(CptPrinterTest  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib
)

target_link_libraries(CptPrinterTest  PRIVATE
    easy_pc
    CppUTest
    CppUTestExt
)

add_test(
    NAME CptPrinterTest 
    COMMAND CptPrinterTest 
)

add_executable(CptVisitorTest 
    AllTests.cpp
    CptVisitorTest.cpp
)

target_include_directories(CptVisitorTest  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib
)

target_link_libraries(CptVisitorTest  PRIVATE
    easy_pc
    CppUTest
    CppUTestExt
)

add_test(
    NAME CptVisitorTest 
    COMMAND CptVisitorTest 
)

add_executable(CombinatorTest 
    AllTests.cpp
    CombinatorTest.cpp
)

target_include_directories(CombinatorTest  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib
)

target_link_libraries(CombinatorTest  PRIVATE
    easy_pc
    CppUTest
    CppUTestExt
)

add_test(
    NAME CombinatorTest 
    COMMAND CombinatorTest 
)

add_executable(CombinatorNewTest
    AllTests.cpp
    CombinatorNewTest.cpp
)

target_include_directories(CombinatorNewTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib
)

target_link_libraries(CombinatorNewTest PRIVATE
    easy_pc
    CppUTest
    CppUTestExt
)

add_test(
    NAME CombinatorNewTest
    COMMAND CombinatorNewTest
)

add_executable(ParserListTest
    AllTests.cpp
    ParserListTest.cpp
)

target_include_directories(ParserListTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib
)

target_link_libraries(ParserListTest PRIVATE
    easy_pc
    CppUTest
    CppUTestExt
)

add_test(
    NAME ParserListTest
    COMMAND ParserListTest
)

add_executable(GdlParserTest
    AllTests.cpp
    GdlParserTest.cpp
    ../tools/gdl_compiler/gdl_parser.c
)

target_include_directories(GdlParserTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../tools/gdl_compiler
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(GdlParserTest PRIVATE
    easy_pc
    CppUTest
    CppUTestExt
)

add_test(
    NAME GdlParserTest
    COMMAND GdlParserTest
)

add_executable(GdlAstBuilderTest
    AllTests.cpp
    GdlAstBuilderTest.cpp
    ../tools/gdl_compiler/gdl_parser.c
    ../tools/gdl_compiler/gdl_ast_builder.c
)

target_include_directories(GdlAstBuilderTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../tools/gdl_compiler
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(GdlAstBuilderTest PRIVATE
    easy_pc
    CppUTest
    CppUTestExt
)

add_test(
    NAME GdlAstBuilderTest
    COMMAND GdlAstBuilderTest
)

add_executable(GeneratedParserTest
    AllTests.cpp
    GeneratedParserTest.cpp
    ../tools/gdl_compiler/gdl_parser.c
    ../tools/gdl_compiler/gdl_ast_builder.c
    ../tools/gdl_compiler/gdl_code_generator.c # Include the code generator source
)

target_include_directories(GeneratedParserTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../tools/gdl_compiler
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(GeneratedParserTest PRIVATE
    easy_pc
    CppUTest
    CppUTestExt
)

add_test(
    NAME GeneratedParserTest
    COMMAND GeneratedParserTest
)

add_executable(ArithmeticParserGeneratorTest
    AllTests.cpp
    ArithmeticParserGeneratorTest.cpp
    ../tools/gdl_compiler/gdl_parser.c
    ../tools/gdl_compiler/gdl_ast_builder.c
    ../tools/gdl_compiler/gdl_code_generator.c
)

target_include_directories(ArithmeticParserGeneratorTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../tools/gdl_compiler
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(ArithmeticParserGeneratorTest PRIVATE
    easy_pc
    CppUTest
    CppUTestExt
)

add_test(
    NAME ArithmeticParserGeneratorTest
    COMMAND ArithmeticParserGeneratorTest
)

add_executable(AstBuilderTest
    AllTests.cpp
    AstBuilderTest.cpp
)

target_include_directories(AstBuilderTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib
)

target_link_libraries(AstBuilderTest PRIVATE
    easy_pc
    CppUTest
    CppUTestExt
)

add_test(
    NAME AstBuilderTest
    COMMAND AstBuilderTest
)