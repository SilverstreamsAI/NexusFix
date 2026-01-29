/*
    NexusFIX Diagnostic Utilities

    Zero-overhead debugging with std::source_location (C++20/23).

    Features:
    - Compile-time source location capture
    - Zero runtime overhead when disabled
    - NFX_DEBUG_ASSERT: Debug-only assertions with location info
    - NFX_TRACE_POINT: Diagnostic trace points for debugging

    Usage:
    - Define NFX_DEBUG_DIAGNOSTICS to enable diagnostic output
    - In release builds, all diagnostics compile to no-ops
*/

#pragma once

#include <source_location>
#include <cstdio>
#include <cstdlib>

namespace nfx::util {

// ============================================================================
// Source Location Wrapper
// ============================================================================

/// Lightweight source location info for diagnostics
struct SourceLoc {
    const char* file;
    const char* function;
    unsigned int line;
    unsigned int column;

    /// Capture current source location at call site
    [[nodiscard]] static constexpr SourceLoc current(
        std::source_location loc = std::source_location::current()) noexcept {
        return SourceLoc{
            loc.file_name(),
            loc.function_name(),
            loc.line(),
            loc.column()
        };
    }

    /// Get file name without path
    [[nodiscard]] constexpr const char* file_basename() const noexcept {
        const char* p = file;
        const char* last = file;
        while (*p) {
            if (*p == '/' || *p == '\\') {
                last = p + 1;
            }
            ++p;
        }
        return last;
    }
};

// ============================================================================
// Debug Assertion with Source Location
// ============================================================================

#ifdef NFX_DEBUG_DIAGNOSTICS

/// Debug assertion that prints location on failure (debug builds only)
/// Zero overhead in release builds
#define NFX_DEBUG_ASSERT(condition, message) \
    do { \
        if (!(condition)) [[unlikely]] { \
            ::nfx::util::debug_assert_failed( \
                #condition, \
                message, \
                ::std::source_location::current()); \
        } \
    } while (0)

/// Trace point for debugging (prints location and optional message)
#define NFX_TRACE_POINT(message) \
    ::nfx::util::trace_point(message, ::std::source_location::current())

/// Trace value for debugging (prints variable name and value)
#define NFX_TRACE_VALUE(value) \
    ::nfx::util::trace_value(#value, value, ::std::source_location::current())

#else // NFX_DEBUG_DIAGNOSTICS not defined

#define NFX_DEBUG_ASSERT(condition, message) ((void)0)
#define NFX_TRACE_POINT(message) ((void)0)
#define NFX_TRACE_VALUE(value) ((void)0)

#endif // NFX_DEBUG_DIAGNOSTICS

// ============================================================================
// Implementation Functions (cold path, never inlined)
// ============================================================================

/// Called when debug assertion fails - prints diagnostic and aborts
[[noreturn]] inline void debug_assert_failed(
    const char* condition,
    const char* message,
    std::source_location loc) noexcept {

    std::fprintf(stderr,
        "\n[NFX ASSERTION FAILED]\n"
        "  Condition: %s\n"
        "  Message:   %s\n"
        "  Location:  %s:%u\n"
        "  Function:  %s\n\n",
        condition,
        message,
        loc.file_name(),
        loc.line(),
        loc.function_name());

    std::fflush(stderr);
    std::abort();
}

/// Trace point output for debugging
inline void trace_point(
    const char* message,
    std::source_location loc) noexcept {

    std::fprintf(stderr,
        "[NFX TRACE] %s:%u %s() - %s\n",
        SourceLoc::current(loc).file_basename(),
        loc.line(),
        loc.function_name(),
        message);
    std::fflush(stderr);
}

/// Trace value output for debugging
template<typename T>
inline void trace_value(
    const char* name,
    const T& value,
    std::source_location loc) noexcept {

    if constexpr (std::is_integral_v<T>) {
        std::fprintf(stderr,
            "[NFX TRACE] %s:%u %s = %lld\n",
            SourceLoc::current(loc).file_basename(),
            loc.line(),
            name,
            static_cast<long long>(value));
    } else if constexpr (std::is_floating_point_v<T>) {
        std::fprintf(stderr,
            "[NFX TRACE] %s:%u %s = %f\n",
            SourceLoc::current(loc).file_basename(),
            loc.line(),
            name,
            static_cast<double>(value));
    } else if constexpr (std::is_pointer_v<T>) {
        std::fprintf(stderr,
            "[NFX TRACE] %s:%u %s = %p\n",
            SourceLoc::current(loc).file_basename(),
            loc.line(),
            name,
            static_cast<const void*>(value));
    } else {
        std::fprintf(stderr,
            "[NFX TRACE] %s:%u %s = <non-printable>\n",
            SourceLoc::current(loc).file_basename(),
            loc.line(),
            name);
    }
    std::fflush(stderr);
}

// ============================================================================
// Production Assertion (always enabled, used for invariants)
// ============================================================================

/// Production assertion - always enabled, includes source location
/// Use for invariants that should never be violated
#define NFX_INVARIANT(condition, message) \
    do { \
        if (!(condition)) [[unlikely]] { \
            ::nfx::util::invariant_failed( \
                #condition, \
                message, \
                ::std::source_location::current()); \
        } \
    } while (0)

/// Called when invariant fails - prints diagnostic and aborts
[[noreturn]] inline void invariant_failed(
    const char* condition,
    const char* message,
    std::source_location loc) noexcept {

    std::fprintf(stderr,
        "\n[NFX INVARIANT VIOLATED]\n"
        "  Condition: %s\n"
        "  Message:   %s\n"
        "  Location:  %s:%u\n"
        "  Function:  %s\n\n",
        condition,
        message,
        loc.file_name(),
        loc.line(),
        loc.function_name());

    std::fflush(stderr);
    std::abort();
}

// ============================================================================
// Scoped Trace (RAII-based scope tracking)
// ============================================================================

#ifdef NFX_DEBUG_DIAGNOSTICS

/// RAII scope tracer - prints entry/exit with timing
class ScopedTrace {
public:
    explicit ScopedTrace(
        const char* scope_name,
        std::source_location loc = std::source_location::current()) noexcept
        : name_(scope_name), loc_(loc) {
        std::fprintf(stderr,
            "[NFX SCOPE ENTER] %s:%u %s()\n",
            SourceLoc::current(loc_).file_basename(),
            loc_.line(),
            name_);
        std::fflush(stderr);
    }

    ~ScopedTrace() noexcept {
        std::fprintf(stderr,
            "[NFX SCOPE EXIT]  %s:%u %s()\n",
            SourceLoc::current(loc_).file_basename(),
            loc_.line(),
            name_);
        std::fflush(stderr);
    }

    ScopedTrace(const ScopedTrace&) = delete;
    ScopedTrace& operator=(const ScopedTrace&) = delete;

private:
    const char* name_;
    std::source_location loc_;
};

#define NFX_SCOPED_TRACE(name) \
    ::nfx::util::ScopedTrace _nfx_scoped_trace_##__LINE__(name)

#else // NFX_DEBUG_DIAGNOSTICS not defined

#define NFX_SCOPED_TRACE(name) ((void)0)

#endif // NFX_DEBUG_DIAGNOSTICS

} // namespace nfx::util
