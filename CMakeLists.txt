cmake_minimum_required(VERSION 3.25)
project(nexusfix VERSION 0.1.13 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(NFX_ENABLE_SIMD "Enable SIMD optimizations (AVX2)" ON)
option(NFX_ENABLE_AVX512 "Enable AVX-512 optimizations (requires CPU support)" OFF)
option(NFX_ENABLE_IO_URING "Enable io_uring transport (Linux only)" OFF)
option(NFX_ENABLE_LOGGING "Enable Quill high-performance logging" ON)
option(NFX_ENABLE_ABSEIL "Enable Abseil for Swiss Table hash maps (~3x faster)" ON)
option(NFX_ENABLE_MIMALLOC "Enable mimalloc allocator for per-session heaps" OFF)
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

# Abseil - Google's C++ library (Swiss Table hash maps ~3x faster lookups)
if(NFX_ENABLE_ABSEIL)
    FetchContent_Declare(
        abseil-cpp
        GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
        GIT_TAG 20240722.0
        GIT_SHALLOW TRUE
    )
    set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
    set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(abseil-cpp)
endif()

# mimalloc - Microsoft's high-performance allocator (per-session heaps, O(1) cleanup)
if(NFX_ENABLE_MIMALLOC)
    FetchContent_Declare(
        mimalloc
        GIT_REPOSITORY https://github.com/microsoft/mimalloc.git
        GIT_TAG v2.1.7
        GIT_SHALLOW TRUE
    )
    set(MI_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(MI_BUILD_OBJECT OFF CACHE BOOL "" FORCE)
    set(MI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(mimalloc)
endif()

# Platform-specific compiler flags
if(MSVC)
    # MSVC flags
    add_compile_options(
        /W4                      # High warning level
        /WX                      # Warnings as errors
        /permissive-             # Strict conformance
        /Zc:__cplusplus          # Correct __cplusplus macro
        /utf-8                   # UTF-8 source and execution charset
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(
            /O2                  # Maximum optimization
            /GL                  # Whole program optimization
        )
        add_link_options(/LTCG)  # Link-time code generation
    endif()
else()
    # Clang on Linux: use libc++ for full C++23 support (std::expected, etc.)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_compile_options(-stdlib=libc++)
        add_link_options(-stdlib=libc++ -lc++abi)
    endif()

    # GCC/Clang flags
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wno-unused-parameter
    )

    # Release optimizations with full LTO support
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(
            -O3                      # Maximum optimization
        )

        # Native arch only for local builds, not CI
        if(NOT DEFINED ENV{CI})
            add_compile_options(-march=native)
        endif()

        # LTO support (GCC/Clang)
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            add_compile_options(
                -flto=auto
                -fno-fat-lto-objects
                -fdevirtualize-at-ltrans
                -fipa-pta
            )
            add_link_options(
                -flto=auto
                -fuse-linker-plugin
            )
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            add_compile_options(-flto=thin)
            add_link_options(-flto=thin)
        endif()

        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()

    # RelWithDebInfo optimizations (for profiling)
    if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        add_compile_options(-O2)
        if(NOT DEFINED ENV{CI})
            add_compile_options(-march=native)
        endif()
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            add_compile_options(-flto=auto)
            add_link_options(-flto=auto)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            add_compile_options(-flto=thin)
            add_link_options(-flto=thin)
        endif()
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
endif()

# Main header-only library (for now)
add_library(nexusfix INTERFACE)
target_include_directories(nexusfix INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(nexusfix INTERFACE cxx_std_23)

# Windows socket library
if(WIN32)
    target_link_libraries(nexusfix INTERFACE ws2_32)
endif()

# SIMD support
if(NFX_ENABLE_SIMD)
    if(MSVC)
        target_compile_options(nexusfix INTERFACE /arch:AVX2)
    else()
        target_compile_options(nexusfix INTERFACE -mavx2)
    endif()
    target_compile_definitions(nexusfix INTERFACE NFX_HAS_SIMD=1)

    # AVX-512 support (optional, requires CPU support)
    if(NFX_ENABLE_AVX512)
        if(MSVC)
            target_compile_options(nexusfix INTERFACE /arch:AVX512)
        else()
            target_compile_options(nexusfix INTERFACE -mavx512f -mavx512bw)
        endif()
        message(STATUS "AVX-512 optimizations enabled")
    endif()
endif()

# Quill logging support
if(NFX_ENABLE_LOGGING)
    target_link_libraries(nexusfix INTERFACE quill::quill)
    target_compile_definitions(nexusfix INTERFACE NFX_HAS_LOGGING=1)
endif()

# Abseil support (Swiss Table hash maps)
if(NFX_ENABLE_ABSEIL)
    target_link_libraries(nexusfix INTERFACE
        absl::flat_hash_map
        absl::flat_hash_set
    )
    target_compile_definitions(nexusfix INTERFACE NFX_HAS_ABSEIL=1)
    message(STATUS "Abseil Swiss Tables enabled (~3x faster hash map lookups)")
endif()

# mimalloc allocator support (per-session heaps)
if(NFX_ENABLE_MIMALLOC)
    target_link_libraries(nexusfix INTERFACE mimalloc-static)
    target_compile_definitions(nexusfix INTERFACE NFX_HAS_MIMALLOC=1)
    message(STATUS "mimalloc allocator enabled (per-session heaps, O(1) cleanup)")
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
    if(NOT Catch2_FOUND)
        message(STATUS "Catch2 not found, fetching via FetchContent...")
        FetchContent_Declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.5.2
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(Catch2)
        list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    endif()
    add_subdirectory(tests)
endif()

# Benchmarks
if(NFX_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# Examples
if(NFX_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Install targets (only when no FetchContent dependencies are linked)
# Quill and Abseil don't provide export sets, so we can only export
# the pure header-only library without these dependencies
if(NOT NFX_ENABLE_LOGGING AND NOT NFX_ENABLE_ABSEIL AND NOT NFX_ENABLE_MIMALLOC)
    include(GNUInstallDirs)
    install(TARGETS nexusfix
        EXPORT nexusfixTargets
    )
    install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    install(EXPORT nexusfixTargets
        FILE nexusfixTargets.cmake
        NAMESPACE nexusfix::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/nexusfix
    )

    # Package configuration
    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/nexusfixConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )
    configure_file(cmake/nexusfixConfig.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/nexusfixConfig.cmake"
        @ONLY
    )
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/nexusfixConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/nexusfixConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/nexusfix
    )
endif()
