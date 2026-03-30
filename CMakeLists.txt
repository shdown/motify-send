cmake_minimum_required (VERSION 3.0...3.10)
project (motify-send C)

if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE Release)
endif ()

set (CMAKE_C_STANDARD 99)

if ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pedantic -Wpedantic -Wall -Wextra")
endif ()

file (GLOB sources "*.c")
add_executable (motify-send ${sources})
target_compile_definitions (motify-send PUBLIC -D_POSIX_C_SOURCE=200809L)

find_package (PkgConfig REQUIRED)
pkg_check_modules (GSTUFF REQUIRED glib-2.0 gio-2.0)

target_include_directories (motify-send SYSTEM PUBLIC ${GSTUFF_INCLUDE_DIRS})
target_compile_options (motify-send PUBLIC ${GSTUFF_CFLAGS_OTHER})
target_link_libraries (motify-send PUBLIC ${GSTUFF_LIBRARIES})

include (GNUInstallDirs)
install (TARGETS motify-send DESTINATION ${CMAKE_INSTALL_BINDIR})
