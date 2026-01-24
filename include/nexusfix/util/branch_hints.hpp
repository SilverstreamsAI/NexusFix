/*
    NexusFIX Branch Prediction Hints

    Macros to help CPU branch predictor optimize hot paths.
    Mispredict penalty: ~15-20 cycles on modern CPUs.

    Usage:
    - NFX_LIKELY: Condition is almost always true
    - NFX_UNLIKELY: Condition is almost always false (error paths)
    - NFX_ASSUME: Tell compiler a condition is always true
    - NFX_UNREACHABLE: Mark code path as unreachable

    Guidelines:
    - Use sparingly, only in hot paths
    - Profile to verify branch is actually biased (>90%)
    - Wrong hints are worse than no hints
*/

#pragma once

#include <cstdlib>

namespace nfx::util {

// ============================================================================
// Branch Prediction Macros
// ============================================================================

/// Hint that condition is likely true (hot path)
/// Use for: normal operation, success cases, common values
#if defined(__GNUC__) || defined(__clang__)
    #define NFX_LIKELY(x)   __builtin_expect(!!(x), 1)
#else
    #define NFX_LIKELY(x)   (x)
#endif

/// Hint that condition is unlikely true (cold path)
/// Use for: error handling, edge cases, rare conditions
#if defined(__GNUC__) || defined(__clang__)
    #define NFX_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define NFX_UNLIKELY(x) (x)
#endif

// ============================================================================
// Compiler Assumptions
// ============================================================================

/// Tell compiler to assume condition is always true
/// WARNING: Undefined behavior if condition is false!
#if defined(__clang__)
    #define NFX_ASSUME(x) __builtin_assume(x)
#elif defined(__GNUC__) && __GNUC__ >= 13
    #define NFX_ASSUME(x) __attribute__((assume(x)))
#elif defined(_MSC_VER)
    #define NFX_ASSUME(x) __assume(x)
#else
    #define NFX_ASSUME(x) do { if (!(x)) __builtin_unreachable(); } while(0)
#endif

/// Mark code as unreachable (allows compiler to optimize)
#if defined(__GNUC__) || defined(__clang__)
    #define NFX_UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER)
    #define NFX_UNREACHABLE() __assume(0)
#else
    #define NFX_UNREACHABLE() std::abort()
#endif

// ============================================================================
// Function Attributes
// ============================================================================

/// Mark function as hot (frequently called, optimize aggressively)
#if defined(__GNUC__) || defined(__clang__)
    #define NFX_HOT __attribute__((hot))
#else
    #define NFX_HOT
#endif

/// Mark function as cold (rarely called, optimize for size)
#if defined(__GNUC__) || defined(__clang__)
    #define NFX_COLD __attribute__((cold))
#else
    #define NFX_COLD
#endif

/// Force function to be inlined
#if defined(__GNUC__) || defined(__clang__)
    #define NFX_FORCE_INLINE __attribute__((always_inline)) inline
#elif defined(_MSC_VER)
    #define NFX_FORCE_INLINE __forceinline
#else
    #define NFX_FORCE_INLINE inline
#endif

/// Prevent function from being inlined
#if defined(__GNUC__) || defined(__clang__)
    #define NFX_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
    #define NFX_NOINLINE __declspec(noinline)
#else
    #define NFX_NOINLINE
#endif

/// Mark function as pure (no side effects, only depends on args)
#if defined(__GNUC__) || defined(__clang__)
    #define NFX_PURE __attribute__((pure))
#else
    #define NFX_PURE
#endif

/// Mark function as const (pure + doesn't read global memory)
#if defined(__GNUC__) || defined(__clang__)
    #define NFX_CONST __attribute__((const))
#else
    #define NFX_CONST
#endif

// ============================================================================
// Prefetch Hints
// ============================================================================

/// Prefetch for read with high locality
#define NFX_PREFETCH_READ(addr) __builtin_prefetch((addr), 0, 3)

/// Prefetch for read with low locality (streaming)
#define NFX_PREFETCH_READ_NTA(addr) __builtin_prefetch((addr), 0, 0)

/// Prefetch for write
#define NFX_PREFETCH_WRITE(addr) __builtin_prefetch((addr), 1, 3)

// ============================================================================
// Loop Optimization Hints
// ============================================================================

/// Hint that loop will iterate many times
#if defined(__clang__)
    #define NFX_LOOP_VECTORIZE _Pragma("clang loop vectorize(enable)")
#elif defined(__GNUC__)
    #define NFX_LOOP_VECTORIZE _Pragma("GCC ivdep")
#else
    #define NFX_LOOP_VECTORIZE
#endif

/// Hint that loop iterations are independent
#if defined(__clang__)
    #define NFX_LOOP_INDEPENDENT _Pragma("clang loop vectorize(assume_safety)")
#elif defined(__GNUC__)
    #define NFX_LOOP_INDEPENDENT _Pragma("GCC ivdep")
#else
    #define NFX_LOOP_INDEPENDENT
#endif

/// Unroll loop N times
#if defined(__clang__)
    #define NFX_LOOP_UNROLL(n) _Pragma("clang loop unroll_count(" #n ")")
#elif defined(__GNUC__) && __GNUC__ >= 8
    #define NFX_LOOP_UNROLL(n) _Pragma("GCC unroll " #n)
#else
    #define NFX_LOOP_UNROLL(n)
#endif

// ============================================================================
// Memory Alignment Hints
// ============================================================================

/// Hint that pointer is aligned to N bytes
#if defined(__GNUC__) || defined(__clang__)
    #define NFX_ASSUME_ALIGNED(ptr, n) __builtin_assume_aligned((ptr), (n))
#else
    #define NFX_ASSUME_ALIGNED(ptr, n) (ptr)
#endif

// ============================================================================
// Restrict Pointer (no aliasing)
// ============================================================================

#if defined(__GNUC__) || defined(__clang__)
    #define NFX_RESTRICT __restrict__
#elif defined(_MSC_VER)
    #define NFX_RESTRICT __restrict
#else
    #define NFX_RESTRICT
#endif

// ============================================================================
// Common Patterns with Hints
// ============================================================================

/// Check for null pointer (cold path)
#define NFX_CHECK_NULL(ptr) \
    if (NFX_UNLIKELY((ptr) == nullptr))

/// Check for error (cold path)
#define NFX_CHECK_ERROR(cond) \
    if (NFX_UNLIKELY(cond))

/// Check for success (hot path)
#define NFX_CHECK_SUCCESS(cond) \
    if (NFX_LIKELY(cond))

/// Assert that should be optimized away in release
#define NFX_ASSERT(cond) \
    do { \
        if (NFX_UNLIKELY(!(cond))) { \
            NFX_UNREACHABLE(); \
        } \
    } while(0)

// ============================================================================
// Example Usage Patterns
// ============================================================================

/*
Example 1: Error handling (unlikely path)
-----------------------------------------
if (NFX_UNLIKELY(result < 0)) {
    return handle_error(result);  // Cold path
}
process_success(result);  // Hot path

Example 2: Common case optimization
-----------------------------------
if (NFX_LIKELY(msg_type == 'D')) {
    handle_new_order();  // 95% of messages
} else if (NFX_UNLIKELY(msg_type == '5')) {
    handle_logout();     // Rare
}

Example 3: Loop with prefetch
-----------------------------
for (size_t i = 0; i < n; ++i) {
    NFX_PREFETCH_READ(&data[i + 8]);  // Prefetch ahead
    process(data[i]);
}

Example 4: Hot function
-----------------------
NFX_HOT NFX_FORCE_INLINE
void process_message(const char* data, size_t len) {
    // Critical path code
}

Example 5: Vectorizable loop
----------------------------
NFX_LOOP_VECTORIZE
for (size_t i = 0; i < len; ++i) {
    sum += data[i];
}
*/

} // namespace nfx::util
