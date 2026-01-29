/*
    NexusFIX Format Utilities

    Type-safe formatting using std::format (C++23).

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

} // namespace nfx::util
