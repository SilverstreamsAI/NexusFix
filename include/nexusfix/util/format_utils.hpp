/*
    NexusFIX Format Utilities

    Type-safe formatting using std::format (C++23).

    C++23 Features Used:
    - std::format (C++20/23)
    - std::print/std::println (C++23) - with fallback for older compilers
    - std::string::resize_and_overwrite (C++23)

    IMPORTANT: For NON-HOT-PATH use only!
    - Configuration formatting
    - Error message formatting
    - Debug output
    - Logging (use with caution)

    DO NOT use in:
    - Parser hot paths
    - Serializer hot paths
    - Message processing loops
*/

#pragma once

#include <format>
#include <string>
#include <string_view>
#include <cstdint>
#include <iostream>
#include <span>
#include <cstdio>
#include <algorithm>

// ============================================================================
// C++23 <print> Header Detection
// ============================================================================
// std::print/std::println (P2093R14) requires:
// - GCC 14+ with libstdc++
// - Clang 17+ with libc++ 17+
// - MSVC 19.37+

#if __has_include(<print>)
    #include <print>
    #if defined(__cpp_lib_print) && __cpp_lib_print >= 202207L
        #define NFX_HAS_STD_PRINT 1
    #else
        #define NFX_HAS_STD_PRINT 0
    #endif
#else
    #define NFX_HAS_STD_PRINT 0
#endif

// ============================================================================
// C++23 resize_and_overwrite Detection
// ============================================================================
#if defined(__cpp_lib_string_resize_and_overwrite) && __cpp_lib_string_resize_and_overwrite >= 202110L
    #define NFX_HAS_RESIZE_AND_OVERWRITE 1
#else
    #define NFX_HAS_RESIZE_AND_OVERWRITE 0
#endif

namespace nfx::util {

// ============================================================================
// Format Functions (non-hot-path)
// ============================================================================

/// Format a string using std::format (allocates)
/// Use for: error messages, debug output, configuration
template<typename... Args>
[[nodiscard]] inline std::string format(std::format_string<Args...> fmt, Args&&... args) {
    return std::format(fmt, std::forward<Args>(args)...);
}

/// Format to a pre-allocated buffer (no allocation if buffer is large enough)
/// Returns: number of characters written (excluding null terminator)
template<typename... Args>
[[nodiscard]] inline size_t format_to_buffer(
    char* buffer,
    size_t buffer_size,
    std::format_string<Args...> fmt,
    Args&&... args) noexcept {

    auto result = std::format_to_n(buffer, buffer_size - 1, fmt, std::forward<Args>(args)...);
    *result.out = '\0';
    return static_cast<size_t>(result.size);
}

// ============================================================================
// C++23 Print Functions (std::print / std::println)
// ============================================================================
// Zero-overhead wrappers when std::print is available.
// Falls back to std::cout << std::format() for older compilers.
//
// Benefits over std::cout:
// - No iostream state (flags, precision, etc.)
// - Better performance (no virtual dispatch)
// - Cleaner syntax
// - Exception-safe

#if NFX_HAS_STD_PRINT

/// Print formatted output to stdout (no newline)
template<typename... Args>
inline void print(std::format_string<Args...> fmt, Args&&... args) {
    std::print(fmt, std::forward<Args>(args)...);
}

/// Print formatted output to stdout with newline
template<typename... Args>
inline void println(std::format_string<Args...> fmt, Args&&... args) {
    std::println(fmt, std::forward<Args>(args)...);
}

/// Print empty line to stdout
inline void println() {
    std::println("");
}

/// Print formatted output to stderr (no newline)
template<typename... Args>
inline void eprint(std::format_string<Args...> fmt, Args&&... args) {
    std::print(stderr, fmt, std::forward<Args>(args)...);
}

/// Print formatted output to stderr with newline
template<typename... Args>
inline void eprintln(std::format_string<Args...> fmt, Args&&... args) {
    std::println(stderr, fmt, std::forward<Args>(args)...);
}

/// Print empty line to stderr
inline void eprintln() {
    std::println(stderr, "");
}

/// Print to a specific FILE* stream
template<typename... Args>
inline void fprint(std::FILE* stream, std::format_string<Args...> fmt, Args&&... args) {
    std::print(stream, fmt, std::forward<Args>(args)...);
}

/// Print to a specific FILE* stream with newline
template<typename... Args>
inline void fprintln(std::FILE* stream, std::format_string<Args...> fmt, Args&&... args) {
    std::println(stream, fmt, std::forward<Args>(args)...);
}

#else  // Fallback for compilers without <print>

/// Print formatted output to stdout (no newline) - fallback
template<typename... Args>
inline void print(std::format_string<Args...> fmt, Args&&... args) {
    std::cout << std::format(fmt, std::forward<Args>(args)...);
}

/// Print formatted output to stdout with newline - fallback
template<typename... Args>
inline void println(std::format_string<Args...> fmt, Args&&... args) {
    std::cout << std::format(fmt, std::forward<Args>(args)...) << '\n';
}

/// Print empty line to stdout - fallback
inline void println() {
    std::cout << '\n';
}

/// Print formatted output to stderr (no newline) - fallback
template<typename... Args>
inline void eprint(std::format_string<Args...> fmt, Args&&... args) {
    std::cerr << std::format(fmt, std::forward<Args>(args)...);
}

/// Print formatted output to stderr with newline - fallback
template<typename... Args>
inline void eprintln(std::format_string<Args...> fmt, Args&&... args) {
    std::cerr << std::format(fmt, std::forward<Args>(args)...) << '\n';
}

/// Print empty line to stderr - fallback
inline void eprintln() {
    std::cerr << '\n';
}

/// Print to a specific FILE* stream - fallback
template<typename... Args>
inline void fprint(std::FILE* stream, std::format_string<Args...> fmt, Args&&... args) {
    std::fputs(std::format(fmt, std::forward<Args>(args)...).c_str(), stream);
}

/// Print to a specific FILE* stream with newline - fallback
template<typename... Args>
inline void fprintln(std::FILE* stream, std::format_string<Args...> fmt, Args&&... args) {
    auto str = std::format(fmt, std::forward<Args>(args)...);
    std::fputs(str.c_str(), stream);
    std::fputc('\n', stream);
}

#endif  // NFX_HAS_STD_PRINT

// ============================================================================
// C++23 resize_and_overwrite Utilities
// ============================================================================
// Avoids double-initialization when building strings.
// Pattern: allocate buffer, fill it, set final size.

#if NFX_HAS_RESIZE_AND_OVERWRITE

/// Format to a string using resize_and_overwrite (zero double-init)
/// @param max_size Maximum expected output size
/// @param fmt Format string
/// @param args Format arguments
/// @return Formatted string
template<typename... Args>
[[nodiscard]] inline std::string format_to_string(
    size_t max_size,
    std::format_string<Args...> fmt,
    Args&&... args)
{
    std::string result;
    result.resize_and_overwrite(max_size, [&](char* buf, size_t n) {
        auto out = std::format_to_n(buf, n, fmt, std::forward<Args>(args)...);
        return static_cast<size_t>(out.out - buf);
    });
    return result;
}

/// Build a string using a callback with resize_and_overwrite
/// @param max_size Maximum expected output size
/// @param builder Callback: size_t(char* buf, size_t capacity) -> bytes written
/// @return Built string
template<typename Builder>
[[nodiscard]] inline std::string build_string(size_t max_size, Builder&& builder) {
    std::string result;
    result.resize_and_overwrite(max_size, std::forward<Builder>(builder));
    return result;
}

#else  // Fallback for compilers without resize_and_overwrite

/// Format to a string - fallback (uses resize + format_to_n)
template<typename... Args>
[[nodiscard]] inline std::string format_to_string(
    size_t max_size,
    std::format_string<Args...> fmt,
    Args&&... args)
{
    std::string result(max_size, '\0');
    auto out = std::format_to_n(result.data(), max_size, fmt, std::forward<Args>(args)...);
    result.resize(static_cast<size_t>(out.out - result.data()));
    return result;
}

/// Build a string using a callback - fallback
template<typename Builder>
[[nodiscard]] inline std::string build_string(size_t max_size, Builder&& builder) {
    std::string result(max_size, '\0');
    size_t written = builder(result.data(), max_size);
    result.resize(written);
    return result;
}

#endif  // NFX_HAS_RESIZE_AND_OVERWRITE

// ============================================================================
// Common Format Patterns
// ============================================================================

/// Format session identifier
[[nodiscard]] inline std::string format_session_id(
    std::string_view sender,
    std::string_view target) {
    return std::format("{}:{}", sender, target);
}

/// Format sequence numbers
[[nodiscard]] inline std::string format_seq_gap(
    uint32_t expected,
    uint32_t received) {
    return std::format("Sequence gap: expected {}, received {}", expected, received);
}

/// Format byte size with units
[[nodiscard]] inline std::string format_bytes(size_t bytes) {
    if (bytes >= 1024 * 1024 * 1024) {
        return std::format("{:.2f} GB", static_cast<double>(bytes) / (1024 * 1024 * 1024));
    } else if (bytes >= 1024 * 1024) {
        return std::format("{:.2f} MB", static_cast<double>(bytes) / (1024 * 1024));
    } else if (bytes >= 1024) {
        return std::format("{:.2f} KB", static_cast<double>(bytes) / 1024);
    } else {
        return std::format("{} bytes", bytes);
    }
}

/// Format latency with appropriate units
[[nodiscard]] inline std::string format_latency_ns(uint64_t nanoseconds) {
    if (nanoseconds >= 1'000'000'000) {
        return std::format("{:.3f} s", static_cast<double>(nanoseconds) / 1'000'000'000);
    } else if (nanoseconds >= 1'000'000) {
        return std::format("{:.3f} ms", static_cast<double>(nanoseconds) / 1'000'000);
    } else if (nanoseconds >= 1'000) {
        return std::format("{:.3f} us", static_cast<double>(nanoseconds) / 1'000);
    } else {
        return std::format("{} ns", nanoseconds);
    }
}

/// Format throughput
[[nodiscard]] inline std::string format_throughput(double msgs_per_sec) {
    if (msgs_per_sec >= 1'000'000) {
        return std::format("{:.2f}M msg/s", msgs_per_sec / 1'000'000);
    } else if (msgs_per_sec >= 1'000) {
        return std::format("{:.2f}K msg/s", msgs_per_sec / 1'000);
    } else {
        return std::format("{:.0f} msg/s", msgs_per_sec);
    }
}

/// Format FIX tag=value pair
[[nodiscard]] inline std::string format_tag_value(int tag, std::string_view value) {
    return std::format("{}={}", tag, value);
}

/// Format connection address
[[nodiscard]] inline std::string format_address(std::string_view host, uint16_t port) {
    return std::format("{}:{}", host, port);
}

// ============================================================================
// Error Formatting
// ============================================================================

/// Format parse error with context
[[nodiscard]] inline std::string format_parse_error(
    std::string_view message,
    int tag,
    size_t offset) {
    if (tag > 0) {
        return std::format("Parse error: {} (tag={}, offset={})", message, tag, offset);
    } else {
        return std::format("Parse error: {} (offset={})", message, offset);
    }
}

/// Format transport error with system errno
[[nodiscard]] inline std::string format_transport_error(
    std::string_view message,
    int system_errno) {
    if (system_errno != 0) {
        return std::format("Transport error: {} (errno={})", message, system_errno);
    } else {
        return std::format("Transport error: {}", message);
    }
}

// ============================================================================
// Compile-time Format String Validation
// ============================================================================

/// Validate format string at compile time (returns true if valid)
template<typename... Args>
consteval bool validate_format_string(const char* fmt) {
    // std::format_string validates at compile time
    // This helper is for documentation purposes
    return true;
}

// ============================================================================
// Debug Formatting Helpers
// ============================================================================
// Utilities for debug output that use C++23 range formatting where available.

/// Format a span of bytes as hex string (for debug output)
[[nodiscard]] inline std::string format_hex(std::span<const std::byte> data, size_t max_bytes = 32) {
    const size_t limit = std::min(data.size(), max_bytes);
    std::string result;
    result.reserve(limit * 3 + 8);  // "XX " per byte + possible "..."

    for (size_t i = 0; i < limit; ++i) {
        if (i > 0) result += ' ';
        result += std::format("{:02X}", static_cast<unsigned char>(data[i]));
    }

    if (data.size() > max_bytes) {
        result += std::format("... ({} more bytes)", data.size() - max_bytes);
    }

    return result;
}

/// Format a span of chars as hex string (for debug output)
[[nodiscard]] inline std::string format_hex(std::span<const char> data, size_t max_bytes = 32) {
    return format_hex(std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(data.data()), data.size()}, max_bytes);
}

/// Format a span of unsigned chars as hex string (for debug output)
[[nodiscard]] inline std::string format_hex(std::span<const unsigned char> data, size_t max_bytes = 32) {
    return format_hex(std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(data.data()), data.size()}, max_bytes);
}

} // namespace nfx::util

// ============================================================================
// Feature Detection Summary
// ============================================================================
// NFX_HAS_STD_PRINT          - std::print/std::println available (<print> header)
// NFX_HAS_RESIZE_AND_OVERWRITE - std::string::resize_and_overwrite available
