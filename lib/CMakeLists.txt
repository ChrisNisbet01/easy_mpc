add_library(easy_pc STATIC easy_pc.c cpt_printer.c parsers.c)

target_include_directories(easy_pc PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> # For parsers.h
)

target_compile_options(easy_pc PRIVATE -Wall -Wextra -pedantic)

install(TARGETS easy_pc
    EXPORT easy_pc_targets
    ARCHIVE DESTINATION lib
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../include/easy_pc/
    DESTINATION include/easy_pc
    FILES_MATCHING PATTERN "*.h"
)