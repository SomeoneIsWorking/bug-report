cmake_minimum_required(VERSION 3.20)
project(bug_report VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# RmlUi: the consumer's own RmlUi target is used as-is when it already exists (a product that
# shows this form inside its UI must share one RmlUi with it). Otherwise — this repository built on
# its own, for its tests — RmlUi 6.3 is taken from BUG_REPORT_RMLUI_DIR or a sibling checkout and
# built static with no font engine, which the tests do not need.
if(NOT TARGET RmlUi::Core)
    set(BUG_REPORT_RMLUI_DEFAULT "")
    foreach(candidate "$ENV{BUG_REPORT_RMLUI_DIR}"
                      "${CMAKE_CURRENT_SOURCE_DIR}/../RmlUi"
                      "${CMAKE_CURRENT_SOURCE_DIR}/../../RmlUi")
        if(NOT "${candidate}" STREQUAL "" AND IS_DIRECTORY "${candidate}")
            set(BUG_REPORT_RMLUI_DEFAULT "${candidate}")
            break()
        endif()
    endforeach()
    set(BUG_REPORT_RMLUI_DIR "${BUG_REPORT_RMLUI_DEFAULT}" CACHE PATH "RmlUi 6.3 checkout")
    if(NOT IS_DIRECTORY "${BUG_REPORT_RMLUI_DIR}")
        message(FATAL_ERROR
            "bug_report needs RmlUi: add an RmlUi::Core target before this directory, or provide "
            "an RmlUi 6.3 checkout with -DBUG_REPORT_RMLUI_DIR=<checkout> (tried "
            "$ENV{BUG_REPORT_RMLUI_DIR}, ../RmlUi and ../../RmlUi beside this repository)")
    endif()
    set(BUILD_SHARED_LIBS OFF)
    set(RMLUI_SAMPLES OFF CACHE BOOL "" FORCE)
    set(RMLUI_FONT_ENGINE "none" CACHE STRING "" FORCE)
    set(RMLUI_PRECOMPILED_HEADERS OFF CACHE BOOL "" FORCE)
    set(RMLUI_COMPILER_OPTIONS OFF CACHE BOOL "" FORCE)
    add_subdirectory("${BUG_REPORT_RMLUI_DIR}" "${CMAKE_BINARY_DIR}/rmlui" EXCLUDE_FROM_ALL)
endif()

add_library(bug_report STATIC
    src/report.cpp
    src/draft.cpp
    src/form.cpp)
add_library(bug_report::bug_report ALIAS bug_report)
target_include_directories(bug_report PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(bug_report PUBLIC RmlUi::Core)
if(MSVC)
    target_compile_options(bug_report PRIVATE /W4)
else()
    target_compile_options(bug_report PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(PROJECT_IS_TOP_LEVEL AND NOT CMAKE_CROSSCOMPILING)
    enable_testing()
    add_executable(bug_report_tests tests/test_bug_report.cpp)
    target_link_libraries(bug_report_tests PRIVATE bug_report)
    add_test(NAME bug_report_tests COMMAND bug_report_tests)
endif()
