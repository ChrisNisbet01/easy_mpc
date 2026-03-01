cmake_minimum_required(VERSION 3.16)
project(easy_pc C)

include(FetchContent)
include(CTest)
include(cmake/EasyPcMacros.cmake)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(WITH_INPUT_STREAM_SUPPORT "Enable streaming input support" ON)

if(WITH_INPUT_STREAM_SUPPORT)
    find_package(Threads REQUIRED)
endif()

add_subdirectory(lib)

option(BUILD_EXAMPLES "Build example applications" OFF)
if(BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

# --- Unit Tests ---
option(WITH_UNIT_TESTS "Build unit tests" OFF)
add_custom_target(all_unit_tests)

if(WITH_UNIT_TESTS)

  enable_language(CXX) # Tests are in C++

  # CppUTest
  set(CPPUTEST_USE_GCOV OFF)
  set(CPPUTEST_USE_MEM_LEAK_DETECTION OFF)
  set(CPPUTEST_USE_VALGRIND OFF)
  set(CPPUTEST_USE_VLD OFF)
  set(CPPUTEST_USE_SANITIZERS OFF)
  set(CPPUTEST_USE_COVERAGE OFF)
  set(CPPUTEST_USE_PCH OFF)
  set(CPPUTEST_USE_MODULES OFF)
  set(CPPUTEST_USE_TDD_MACROS OFF)
  set(CPPUTEST_USE_REAL_GCC_THR OFF)
  set(CPPUTEST_USE_REAL_MINGW_THR OFF)
  set(CPPUTEST_USE_REAL_PTHREAD OFF)
  set(CPPUTEST_USE_REAL_STD_THR OFF)
  set(CPPUTEST_USE_EXTENSIONS ON)
  set(CPPUTEST_MEM_LEAK_DETECTION_DISABLED OFF CACHE BOOL "" FORCE)

  # Prevent CppUTest's own tests from being added to our test suite
  set(TESTS OFF CACHE BOOL "Disable CppUTest's own tests")

  FetchContent_Declare(
    cpputest
    GIT_REPOSITORY https://github.com/cpputest/cpputest.git
    GIT_TAG        v4.0
    CMAKE_ARGS BUILD_INSTALL OFF # Attempt to disable CppUTest's own installation
  )
  FetchContent_MakeAvailable(cpputest)
  add_subdirectory(tests)

endif()

# --- Doxygen Documentation ---
find_package(Doxygen)

if (DOXYGEN_FOUND)
    message(STATUS "Doxygen found, documentation will be generated.")
    file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/docs/help/html)

    # Add a custom target to run Doxygen
    add_custom_target(
        docs ALL
        COMMAND ${DOXYGEN_EXECUTABLE} Doxyfile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen"
    )

    # Make the documentation target dependent on the header file
    # This ensures Doxygen is re-run if the header file changes.
    add_custom_command(
        OUTPUT ${CMAKE_SOURCE_DIR}/docs/help/html/index.html
        COMMAND ${DOXYGEN_EXECUTABLE} Doxyfile
        MAIN_DEPENDENCY include/easy_pc/easy_pc.h
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Re-generating Doxygen documentation due to header change"
        VERBATIM
    )
    add_custom_target(doxygen_docs ALL DEPENDS ${CMAKE_SOURCE_DIR}/docs/help/html/index.html)

else ()
    message(STATUS "Doxygen not found, skipping documentation generation.")
endif ()

add_subdirectory(tools/gdl_compiler)