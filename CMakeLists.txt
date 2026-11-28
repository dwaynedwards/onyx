cmake_minimum_required(VERSION 3.25...4.4)
project(onyx
    LANGUAGES CXX
    VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

set(ONYX ${CMAKE_PROJECT_NAME})

option(ONYX_BUILD_TESTS "Build onyx unit tests" ON)
option(ONYX_ENABLE_ASAN "Enable AddressSanitizer and UndefinedBehaviorSanitizer in Debug builds" ON)
option(ONYX_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

if (ONYX_ENABLE_ASAN)
  if (MSVC)
    string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    add_compile_options($<$<CONFIG:Debug>:/fsanitize=address>)
  else ()
    # Apple Clang, mainline Clang, and GCC
    add_compile_options(
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
        $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
    )
    add_link_options(
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )
  endif ()
endif ()

function(onyx_configure_target target)
  if (MSVC)
    target_compile_options(${target} PRIVATE
        /W4
        /permissive-
        $<$<BOOL:${ONYX_WARNINGS_AS_ERRORS}>:/WX>
    )
  else ()
    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wno-empty-translation-unit
        -Wshadow
        -Wconversion
        -Wsign-conversion
        -Wnon-virtual-dtor
        -Wnull-dereference
        $<$<BOOL:${ONYX_WARNINGS_AS_ERRORS}>:-Werror>
    )
  endif ()
endfunction()

file(GLOB_RECURSE ONYX_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(${ONYX} OBJECT)
target_sources(${ONYX} PRIVATE
    ${ONYX_SOURCES})
target_include_directories(${ONYX} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(${ONYX} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src")

onyx_configure_target(${ONYX})

add_subdirectory(deps)

if (ONYX_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif ()
