# Copyright 2024 Google LLC
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.
#
# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.14)

cmake_policy(SET CMP0135 NEW) # https://cmake.org/cmake/help/latest/policy/CMP0135.html

project(minja VERSION 1.0.0 LANGUAGES CXX)

add_library(minja INTERFACE)
target_sources(minja INTERFACE
  # Add sources for minja library if it were not INTERFACE only.
  # For INTERFACE, sources are typically headers or consumed by targets linking to it.
  # If minja becomes a static/shared library, add .cpp files here.
  # For now, we add the serialization source to a separate static library
  # that minja will link against if minja itself is not compiled.
  # However, since minja is INTERFACE, it doesn't get "linked" in the traditional sense.
  # The generated FlatBuffers header is handled by target_include_directories.
  # The serialization .cpp file needs to be compiled into *something*.
  # Let's create a helper static library for minja's compiled parts.
)

# Create a static library for minja's compiled components (like serialization)
add_library(minja_compiled STATIC
    src/minja_serialization.cpp
)

# Link minja_compiled to flatbuffers (runtime)
target_link_libraries(minja_compiled PRIVATE flatbuffers::flatbuffers)

# The main 'minja' INTERFACE library will bring in includes and link dependencies.
# If applications link to 'minja', they should also get 'minja_compiled'.
# We can make minja_compiled a PUBLIC dependency of minja.
target_link_libraries(minja INTERFACE minja_compiled)


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Test if clang-tidy is available
find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
if (CLANG_TIDY_EXE)
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
    set(CMAKE_CXX_CLANG_TIDY
        clang-tidy;
        -header-filter=include/minja/.*;
        # https://clang.llvm.org/extra/clang-tidy/checks/list.html
        # TODO: enable more / disable less checks: google-*,misc-*,modernize-*,performance-*
        -checks=-*,clang-analyzer-*,clang-diagnostic-*,cppcoreguideline-*,bugprone-*,-bugprone-suspicious-include,-bugprone-assignment-in-if-condition,-bugprone-narrowing-conversions,-bugprone-easily-swappable-parameters,-bugprone-inc-dec-in-conditions,-bugprone-exception-escape,-clang-analyzer-cplusplus.StringChecker;
        -warnings-as-errors=*;
    )
else()
    message(STATUS "clang-tidy not found")
endif()

if (MSVC)
    set(MINJA_FUZZTEST_ENABLED_DEFAULT OFF)
    set(MINJA_USE_VENV_DEFAULT OFF)
else()
    set(MINJA_FUZZTEST_ENABLED_DEFAULT ON)
    set(MINJA_USE_VENV_DEFAULT ON)
endif()
option(MINJA_TEST_ENABLED           "minja: Build with test(python interpreter required)"   ON)
option(MINJA_EXAMPLE_ENABLED        "minja: Build with example"                             ON)
option(MINJA_FUZZTEST_ENABLED       "minja: fuzztests enabled"                              MINJA_FUZZTEST_ENABLED_DEFAULT)
option(MINJA_FUZZTEST_FUZZING_MODE  "minja: run fuzztests (if enabled) in fuzzing mode"     OFF)
option(MINJA_USE_VENV               "minja: use Python venv for build"                      MINJA_USE_VENV_DEFAULT)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
if (NOT MSVC)
    add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

include(FetchContent)

# Fetch nlohmann/json
FetchContent_Declare(json URL https://github.com/nlohmann/json/archive/refs/heads/develop.zip)
FetchContent_MakeAvailable(json)
target_link_libraries(minja INTERFACE nlohmann_json::nlohmann_json)

# Fetch FlatBuffers
set(FLATBUFFERS_BUILD_TESTS OFF CACHE BOOL "Build FlatBuffers tests" FORCE)
FetchContent_Declare(
  flatbuffers
  GIT_REPOSITORY https://github.com/google/flatbuffers.git
  GIT_TAG        v24.3.25
)
FetchContent_MakeAvailable(flatbuffers)
# flatc should be available as flatbuffers::flatc after FetchContent_MakeAvailable

set(MINJA_SCHEMA_FILE ${CMAKE_CURRENT_SOURCE_DIR}/include/minja/minja_schema.fbs)
set(FLATBUFFERS_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated_flatbuffers)
set(MINJA_GENERATED_HEADER ${FLATBUFFERS_GENERATED_DIR}/minja_schema_generated.h)

# Command to generate the C++ header from the schema
add_custom_command(
  OUTPUT ${MINJA_GENERATED_HEADER}
  COMMAND $<TARGET_FILE:flatbuffers::flatc> -c -o ${FLATBUFFERS_GENERATED_DIR} --gen-object-api ${MINJA_SCHEMA_FILE}
  DEPENDS ${MINJA_SCHEMA_FILE} flatbuffers::flatc # Depending on flatbuffers::flatc ensures flatc is built first
  COMMENT "Generating C++ from FlatBuffers schema ${MINJA_SCHEMA_FILE}"
  VERBATIM
)

# Custom target for the generated header
add_custom_target(
  MinjaFlatbuffersGenerated
  DEPENDS ${MINJA_GENERATED_HEADER}
)

# Add the generated files directory to minja's include path
target_include_directories(minja INTERFACE
  $<BUILD_INTERFACE:${FLATBUFFERS_GENERATED_DIR}>
)

# Add dependency from minja_compiled to the custom target to ensure generation before compilation
add_dependencies(minja_compiled MinjaFlatbuffersGenerated)

# minja INTERFACE library already links to flatbuffers::flatbuffers
# target_link_libraries(minja INTERFACE flatbuffers::flatbuffers) # This is already present below, good.

if(MINJA_TEST_ENABLED)
    if (MINJA_FUZZTEST_ENABLED)
        # Fetch google/fuzztest (and indirectly, gtest)
        FetchContent_Declare(fuzztest URL https://github.com/google/fuzztest/archive/refs/heads/main.zip)
        FetchContent_MakeAvailable(fuzztest)
        message(STATUS "${fuzztest_BINARY_DIR}: ${${fuzztest_BINARY_DIR}}")
    else()
        # Fetch gtest
        set(INSTALL_GTEST OFF)
        FetchContent_Declare(googletest URL https://github.com/google/googletest/archive/refs/heads/main.zip)
        FetchContent_MakeAvailable(googletest)
    endif()
endif()

# Use ccache if installed
find_program(CCACHE_PATH ccache)
if (CCACHE_PATH)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE_PATH})
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ${CCACHE_PATH})
endif()

# Release build by default
if (NOT XCODE AND NOT MSVC AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

if(MINJA_TEST_ENABLED)
    set(Python_FIND_STRATEGY LOCATION CACHE STRING "Python find strategy" FORCE)
    find_package(Python COMPONENTS Interpreter REQUIRED)
    if(MINJA_USE_VENV)
        # Create a python venv w/ the required dependencies
        set(VENV_DIR "${CMAKE_BINARY_DIR}/venv")
        if(WIN32)
            set(VENV_PYTHON "${VENV_DIR}/Scripts/python.exe")
        else()
            set(VENV_PYTHON "${VENV_DIR}/bin/python")
        endif()
        execute_process(
            COMMAND ${Python_EXECUTABLE} -m venv "${VENV_DIR}"
            COMMAND_ERROR_IS_FATAL ANY)
        execute_process(
            COMMAND ${VENV_PYTHON} -m pip install -r "${CMAKE_SOURCE_DIR}/requirements.txt"
            COMMAND_ERROR_IS_FATAL ANY)
        set(Python_EXECUTABLE "${VENV_PYTHON}" CACHE FILEPATH "Path to Python executable in venv" FORCE)
    endif()
    message(STATUS "Python executable: ${Python_EXECUTABLE}")
endif()

find_program(CPPCHECK cppcheck)
if(CPPCHECK)
  set(CMAKE_CXX_CPPCHECK "${CPPCHECK}" -i ${json_SOURCE_DIR}/include/nlohmann/json.hpp)
  message(STATUS "cppcheck found: ${CPPCHECK}")
endif()

include(GNUInstallDirs)
target_include_directories(minja INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    $<BUILD_INTERFACE:${FLATBUFFERS_GENERATED_DIR}> # Also add to install interface if headers are installed
)

# Ensure the generated header is also installed
install(FILES ${MINJA_GENERATED_HEADER} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/minja/generated)

install(FILES
  ${PROJECT_SOURCE_DIR}/include/minja/minja.hpp
  ${PROJECT_SOURCE_DIR}/include/minja/chat-template.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/minja
)
install(
  TARGETS minja minja_compiled # Also install the compiled part if needed, or handle via linking
  EXPORT "${TARGETS_EXPORT_NAME}"
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/minja  # for downstream projects
)

if(MINJA_EXAMPLE_ENABLED)
    add_subdirectory(examples)
endif()

if(MINJA_TEST_ENABLED)
    enable_testing()
    include(GoogleTest)
    add_subdirectory(tests)
endif()
