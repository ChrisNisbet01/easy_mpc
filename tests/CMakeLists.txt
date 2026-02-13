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

