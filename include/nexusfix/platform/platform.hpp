#pragma once

/// @file platform.hpp
/// @brief Platform and compiler detection for cross-platform support

// ============================================================================
// Platform Detection
// ============================================================================

#if defined(_WIN32) || defined(_WIN64)
    #define NFX_PLATFORM_WINDOWS 1
    #define NFX_PLATFORM_LINUX 0
    #define NFX_PLATFORM_MACOS 0
    #define NFX_PLATFORM_NAME "Windows"
#elif defined(__APPLE__) && defined(__MACH__)
    #define NFX_PLATFORM_WINDOWS 0
    #define NFX_PLATFORM_LINUX 0
    #define NFX_PLATFORM_MACOS 1
    #define NFX_PLATFORM_NAME "macOS"
#elif defined(__linux__)
    #define NFX_PLATFORM_WINDOWS 0
    #define NFX_PLATFORM_LINUX 1
    #define NFX_PLATFORM_MACOS 0
    #define NFX_PLATFORM_NAME "Linux"
#else
    #error "Unsupported platform: NexusFIX requires Windows, Linux, or macOS"
#endif

// POSIX-compatible platforms (Linux and macOS share BSD socket API)
#if NFX_PLATFORM_LINUX || NFX_PLATFORM_MACOS
    #define NFX_PLATFORM_POSIX 1
#else
    #define NFX_PLATFORM_POSIX 0
#endif

// ============================================================================
// Compiler Detection
// ============================================================================

#if defined(_MSC_VER)
    #define NFX_COMPILER_MSVC 1
    #define NFX_COMPILER_GCC 0
    #define NFX_COMPILER_CLANG 0
    #define NFX_COMPILER_NAME "MSVC"
    #define NFX_COMPILER_VERSION _MSC_VER
#elif defined(__clang__)
    #define NFX_COMPILER_MSVC 0
    #define NFX_COMPILER_GCC 0
    #define NFX_COMPILER_CLANG 1
    #define NFX_COMPILER_NAME "Clang"
    #define NFX_COMPILER_VERSION (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#elif defined(__GNUC__)
    #define NFX_COMPILER_MSVC 0
    #define NFX_COMPILER_GCC 1
    #define NFX_COMPILER_CLANG 0
    #define NFX_COMPILER_NAME "GCC"
    #define NFX_COMPILER_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#else
    #define NFX_COMPILER_MSVC 0
    #define NFX_COMPILER_GCC 0
    #define NFX_COMPILER_CLANG 0
    #define NFX_COMPILER_NAME "Unknown"
    #define NFX_COMPILER_VERSION 0
#endif

// ============================================================================
// Architecture Detection
// ============================================================================

#if defined(__x86_64__) || defined(_M_X64)
    #define NFX_ARCH_X64 1
    #define NFX_ARCH_X86 0
    #define NFX_ARCH_ARM64 0
    #define NFX_ARCH_NAME "x86_64"
#elif defined(__i386__) || defined(_M_IX86)
    #define NFX_ARCH_X64 0
    #define NFX_ARCH_X86 1
    #define NFX_ARCH_ARM64 0
    #define NFX_ARCH_NAME "x86"
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define NFX_ARCH_X64 0
    #define NFX_ARCH_X86 0
    #define NFX_ARCH_ARM64 1
    #define NFX_ARCH_NAME "ARM64"
#else
    #define NFX_ARCH_X64 0
    #define NFX_ARCH_X86 0
    #define NFX_ARCH_ARM64 0
    #define NFX_ARCH_NAME "Unknown"
#endif

// ============================================================================
// Async I/O Backend Detection
// ============================================================================

// io_uring (Linux 5.1+)
// NFX_HAS_IO_URING should be defined by CMake if liburing is available
#if NFX_PLATFORM_LINUX && defined(NFX_HAS_IO_URING) && NFX_HAS_IO_URING
    #define NFX_ASYNC_IO_IOURING 1
#else
    #define NFX_ASYNC_IO_IOURING 0
#endif

// IOCP (Windows)
#if NFX_PLATFORM_WINDOWS
    #define NFX_ASYNC_IO_IOCP 1
#else
    #define NFX_ASYNC_IO_IOCP 0
#endif

// kqueue (macOS, BSD)
#if NFX_PLATFORM_MACOS
    #define NFX_ASYNC_IO_KQUEUE 1
#else
    #define NFX_ASYNC_IO_KQUEUE 0
#endif

// Generic async I/O availability
#if NFX_ASYNC_IO_IOURING || NFX_ASYNC_IO_IOCP || NFX_ASYNC_IO_KQUEUE
    #define NFX_HAS_ASYNC_IO 1
#else
    #define NFX_HAS_ASYNC_IO 0
#endif

// Async backend name for logging/diagnostics
#if NFX_ASYNC_IO_IOURING
    #define NFX_ASYNC_IO_BACKEND_NAME "io_uring"
#elif NFX_ASYNC_IO_IOCP
    #define NFX_ASYNC_IO_BACKEND_NAME "IOCP"
#elif NFX_ASYNC_IO_KQUEUE
    #define NFX_ASYNC_IO_BACKEND_NAME "kqueue"
#else
    #define NFX_ASYNC_IO_BACKEND_NAME "none"
#endif

// ============================================================================
// SIMD Detection
// ============================================================================

// SSE4.2 (x86/x64)
#if (NFX_ARCH_X64 || NFX_ARCH_X86) && (defined(__SSE4_2__) || (NFX_COMPILER_MSVC && NFX_ARCH_X64))
    #define NFX_HAS_SSE42 1
#else
    #define NFX_HAS_SSE42 0
#endif

// AVX2 (x86/x64)
#if defined(__AVX2__)
    #define NFX_HAS_AVX2 1
#else
    #define NFX_HAS_AVX2 0
#endif

// AVX-512 (x86/x64)
#if defined(__AVX512F__)
    #define NFX_HAS_AVX512 1
#else
    #define NFX_HAS_AVX512 0
#endif

// ARM NEON (ARM64)
#if NFX_ARCH_ARM64 && (defined(__ARM_NEON) || defined(__ARM_NEON__))
    #define NFX_HAS_NEON 1
#else
    #define NFX_HAS_NEON 0
#endif

// ============================================================================
// Compiler-specific Attributes
// ============================================================================

// Force inline
#if NFX_COMPILER_MSVC
    #define NFX_FORCE_INLINE __forceinline
#elif NFX_COMPILER_GCC || NFX_COMPILER_CLANG
    #define NFX_FORCE_INLINE __attribute__((always_inline)) inline
#else
    #define NFX_FORCE_INLINE inline
#endif

// No inline
#if NFX_COMPILER_MSVC
    #define NFX_NO_INLINE __declspec(noinline)
#elif NFX_COMPILER_GCC || NFX_COMPILER_CLANG
    #define NFX_NO_INLINE __attribute__((noinline))
#else
    #define NFX_NO_INLINE
#endif

// Restrict pointer
#if NFX_COMPILER_MSVC
    #define NFX_RESTRICT __restrict
#elif NFX_COMPILER_GCC || NFX_COMPILER_CLANG
    #define NFX_RESTRICT __restrict__
#else
    #define NFX_RESTRICT
#endif

// Hot function (optimize for speed, place in hot section)
#if NFX_COMPILER_GCC || NFX_COMPILER_CLANG
    #define NFX_HOT [[gnu::hot]]
    #define NFX_COLD [[gnu::cold]]
#else
    #define NFX_HOT
    #define NFX_COLD
#endif

// Cache line size (for alignment)
#if defined(__cpp_lib_hardware_interference_size)
    #include <new>
    #define NFX_CACHE_LINE_SIZE std::hardware_destructive_interference_size
#else
    #define NFX_CACHE_LINE_SIZE 64
#endif

// ============================================================================
// Debug/Release Detection
// ============================================================================

#if defined(NDEBUG)
    #define NFX_DEBUG 0
    #define NFX_RELEASE 1
#else
    #define NFX_DEBUG 1
    #define NFX_RELEASE 0
#endif

// ============================================================================
// Platform Info Functions (constexpr)
// ============================================================================

namespace nfx::platform {

/// Get platform name
[[nodiscard]] constexpr const char* name() noexcept {
    return NFX_PLATFORM_NAME;
}

/// Get compiler name
[[nodiscard]] constexpr const char* compiler_name() noexcept {
    return NFX_COMPILER_NAME;
}

/// Get architecture name
[[nodiscard]] constexpr const char* arch_name() noexcept {
    return NFX_ARCH_NAME;
}

/// Get async I/O backend name
[[nodiscard]] constexpr const char* async_io_backend() noexcept {
    return NFX_ASYNC_IO_BACKEND_NAME;
}

/// Check if running on Windows
[[nodiscard]] constexpr bool is_windows() noexcept {
    return NFX_PLATFORM_WINDOWS;
}

/// Check if running on Linux
[[nodiscard]] constexpr bool is_linux() noexcept {
    return NFX_PLATFORM_LINUX;
}

/// Check if running on macOS
[[nodiscard]] constexpr bool is_macos() noexcept {
    return NFX_PLATFORM_MACOS;
}

/// Check if running on POSIX-compatible platform
[[nodiscard]] constexpr bool is_posix() noexcept {
    return NFX_PLATFORM_POSIX;
}

/// Check if async I/O is available
[[nodiscard]] constexpr bool has_async_io() noexcept {
    return NFX_HAS_ASYNC_IO;
}

/// Check if io_uring is available
[[nodiscard]] constexpr bool has_io_uring() noexcept {
    return NFX_ASYNC_IO_IOURING;
}

/// Check if IOCP is available
[[nodiscard]] constexpr bool has_iocp() noexcept {
    return NFX_ASYNC_IO_IOCP;
}

/// Check if kqueue is available
[[nodiscard]] constexpr bool has_kqueue() noexcept {
    return NFX_ASYNC_IO_KQUEUE;
}

} // namespace nfx::platform
