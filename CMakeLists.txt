cmake_minimum_required(VERSION 3.25)
project(nexusfix VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(NFX_ENABLE_SIMD "Enable SIMD optimizations (AVX2)" ON)
option(NFX_ENABLE_IO_URING "Enable io_uring transport (Linux only)" OFF)
option(NFX_ENABLE_LOGGING "Enable Quill high-performance logging" ON)
option(NFX_BUILD_BENCHMARKS "Build benchmarks" ON)
option(NFX_BUILD_TESTS "Build tests" ON)
option(NFX_BUILD_EXAMPLES "Build examples" ON)

# Dependencies via FetchContent
include(FetchContent)

# Quill - High-performance logging library (~20ns hot path)
if(NFX_ENABLE_LOGGING)
    FetchContent_Declare(
        quill
        GIT_REPOSITORY https://github.com/odygrd/quill.git
        GIT_TAG v7.3.0
        GIT_SHALLOW TRUE
    )
    set(QUILL_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(quill)
endif()

# Compiler flags
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -Wno-unused-parameter
)

# Release optimizations with full LTO support
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    # Compiler optimization flags
    add_compile_options(
        -O3                      # Maximum optimization
        -march=native            # CPU-specific optimizations
        -flto=auto               # Link-Time Optimization (auto-detect parallelism)
        -fno-fat-lto-objects     # Smaller LTO object files
        -fdevirtualize-at-ltrans # Better devirtualization at link time
        -fipa-pta                # Interprocedural pointer analysis
    )

    # Linker flags for LTO
    add_link_options(
        -flto=auto
        -fuse-linker-plugin
    )

    # Set interprocedural optimization policy
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# RelWithDebInfo optimizations (for profiling)
if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_compile_options(
        -O2
        -march=native
        -flto=auto
    )
    add_link_options(-flto=auto)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# Main header-only library (for now)
add_library(nexusfix INTERFACE)
target_include_directories(nexusfix INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(nexusfix INTERFACE cxx_std_23)

# SIMD support
if(NFX_ENABLE_SIMD)
    target_compile_options(nexusfix INTERFACE -mavx2)
    target_compile_definitions(nexusfix INTERFACE NFX_HAS_SIMD=1)
endif()

# Quill logging support
if(NFX_ENABLE_LOGGING)
    target_link_libraries(nexusfix INTERFACE quill::quill)
    target_compile_definitions(nexusfix INTERFACE NFX_HAS_LOGGING=1)
endif()

# io_uring support (Linux only)
if(NFX_ENABLE_IO_URING AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBURING liburing)
    if(LIBURING_FOUND)
        target_link_libraries(nexusfix INTERFACE ${LIBURING_LIBRARIES})
        target_compile_definitions(nexusfix INTERFACE NFX_HAS_IO_URING=1)
    endif()
endif()

# Tests
if(NFX_BUILD_TESTS)
    enable_testing()
    find_package(Catch2 3 QUIET)
    if(Catch2_FOUND)
        add_subdirectory(tests)
    else()
        message(STATUS "Catch2 not found, skipping tests")
    endif()
endif()

# Benchmarks
if(NFX_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# Examples
if(NFX_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
