#pragma once

// =============================================================================
// NexusFIX Compiler Feature Detection
// TICKET_026: Cross-platform compiler feature macros
// =============================================================================

// Note: NFX_COMPILER_GCC, NFX_COMPILER_CLANG, NFX_COMPILER_MSVC are defined
// in nexusfix/platform/platform.hpp. This file provides additional feature
// detection macros that depend on specific compiler quirks.

// -----------------------------------------------------------------------------
// Apple Clang Detection
// Apple Clang is detected as NFX_COMPILER_CLANG, but has different behavior
// -----------------------------------------------------------------------------
#if defined(__clang__) && defined(__apple_build_version__)
    #define NFX_COMPILER_APPLE_CLANG 1
#else
    #define NFX_COMPILER_APPLE_CLANG 0
#endif

// -----------------------------------------------------------------------------
// NFX_ASSUME - C++23 [[assume]] with cross-platform support
// Requirements:
//   - GCC 13+ supports [[assume]]
//   - Clang 19+ supports [[assume]] (Clang 18 does NOT)
//   - Apple Clang: disabled due to -Wassume warnings on function calls
//   - MSVC 19.29+ supports __assume()
// -----------------------------------------------------------------------------
#if NFX_COMPILER_APPLE_CLANG
    #define NFX_ASSUME(expr) /* disabled: Apple Clang -Wassume */
#elif defined(__clang__) && __clang_major__ >= 19
    #define NFX_ASSUME(expr) [[assume(expr)]]
#elif defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 13
    #define NFX_ASSUME(expr) [[assume(expr)]]
#elif defined(_MSC_VER) && _MSC_VER >= 1929
    #define NFX_ASSUME(expr) __assume(expr)
#else
    #define NFX_ASSUME(expr) /* unsupported compiler/version */
#endif

// -----------------------------------------------------------------------------
// NFX_UNREACHABLE - Mark unreachable code paths
// Prefer std::unreachable() on C++23, fallback to compiler intrinsics
// -----------------------------------------------------------------------------
#if __cplusplus >= 202302L && !NFX_COMPILER_APPLE_CLANG
    #include <utility>
    #define NFX_UNREACHABLE() std::unreachable()
#elif defined(__GNUC__) || defined(__clang__)
    #define NFX_UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER)
    #define NFX_UNREACHABLE() __assume(0)
#else
    #define NFX_UNREACHABLE() ((void)0)
#endif
