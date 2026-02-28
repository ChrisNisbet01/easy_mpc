set(EASY_PC_LIB_SOURCES
  easy_pc.c
  cpt_printer.c
  parsers.c
  easy_pc_ast.c
  child_list.c
)

# Shared Library
add_library(easy_pc_shared SHARED ${EASY_PC_LIB_SOURCES})
set_target_properties(easy_pc_shared PROPERTIES OUTPUT_NAME "easy_pc")

# Static Library
add_library(easy_pc_static STATIC ${EASY_PC_LIB_SOURCES})
set_target_properties(easy_pc_static PROPERTIES OUTPUT_NAME "easy_pc")

# List of all library targets
set(EASY_PC_TARGETS easy_pc_shared easy_pc_static)

# Common properties for both libraries
target_include_directories(easy_pc_shared PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> # For parsers.h
)
target_include_directories(easy_pc_static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> # For parsers.h
)

target_compile_options(easy_pc_shared PRIVATE -Wall -Wextra -pedantic)
target_compile_options(easy_pc_static PRIVATE -Wall -Wextra -pedantic)

target_link_libraries(easy_pc_shared PRIVATE Threads::Threads)
target_link_libraries(easy_pc_static PRIVATE Threads::Threads)

# LTO settings
include(CheckIPOSupported)
check_ipo_supported(RESULT lto_supported)

if(lto_supported)
  message(STATUS "LTO enabled for library targets.")
  foreach(target IN LISTS EASY_PC_TARGETS)
    set_target_properties(${target} PROPERTIES
      INTERPROCEDURAL_OPTIMIZATION TRUE
    )
  endforeach()
endif()

# Installation
install(TARGETS ${EASY_PC_TARGETS}
    EXPORT easy_pc_targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../include/easy_pc/
    DESTINATION include/easy_pc
    FILES_MATCHING PATTERN "*.h"
)