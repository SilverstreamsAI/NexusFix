/*
    NexusFIX Compile-time String Hashing

    Modern C++ #88: Compile-time String Hashing

    FNV-1a hash algorithm for compile-time string hashing.
    Enables switch statements on string values via hash comparison.

    Benefits:
    - Message type dispatch via jump table instead of strcmp
    - Zero runtime overhead for known strings
    - Collision probability: ~2^-32 for 32-bit, ~2^-64 for 64-bit

    Usage:
        using namespace nfx::util::literals;
        switch (fnv1a_hash(msg_type)) {
            case "D"_hash:  return handle_new_order();
            case "8"_hash:  return handle_exec_report();
            case "F"_hash:  return handle_cancel();
        }
*/

#pragma once

#include <cstdint>
#include <string_view>

namespace nfx::util {

// ============================================================================
// FNV-1a Constants
// ============================================================================

namespace detail {

// FNV-1a 32-bit constants
inline constexpr uint32_t fnv1a_32_offset_basis = 2166136261u;
inline constexpr uint32_t fnv1a_32_prime = 16777619u;

// FNV-1a 64-bit constants
inline constexpr uint64_t fnv1a_64_offset_basis = 14695981039346656037ull;
inline constexpr uint64_t fnv1a_64_prime = 1099511628211ull;

} // namespace detail

// ============================================================================
// FNV-1a Hash Functions (32-bit)
// ============================================================================

/// Compute FNV-1a 32-bit hash at compile time
[[nodiscard]] consteval uint32_t fnv1a_hash32(std::string_view str) noexcept {
    uint32_t hash = detail::fnv1a_32_offset_basis;
    for (char c : str) {
        hash ^= static_cast<uint32_t>(static_cast<unsigned char>(c));
        hash *= detail::fnv1a_32_prime;
    }
    return hash;
}

/// Compute FNV-1a 32-bit hash at runtime
[[nodiscard]] inline uint32_t fnv1a_hash32_runtime(std::string_view str) noexcept {
    uint32_t hash = detail::fnv1a_32_offset_basis;
    for (char c : str) {
        hash ^= static_cast<uint32_t>(static_cast<unsigned char>(c));
        hash *= detail::fnv1a_32_prime;
    }
    return hash;
}

// ============================================================================
// FNV-1a Hash Functions (64-bit)
// ============================================================================

/// Compute FNV-1a 64-bit hash at compile time
[[nodiscard]] consteval uint64_t fnv1a_hash64(std::string_view str) noexcept {
    uint64_t hash = detail::fnv1a_64_offset_basis;
    for (char c : str) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        hash *= detail::fnv1a_64_prime;
    }
    return hash;
}

/// Compute FNV-1a 64-bit hash at runtime
[[nodiscard]] inline uint64_t fnv1a_hash64_runtime(std::string_view str) noexcept {
    uint64_t hash = detail::fnv1a_64_offset_basis;
    for (char c : str) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        hash *= detail::fnv1a_64_prime;
    }
    return hash;
}

// ============================================================================
// Default Hash (64-bit)
// ============================================================================

/// Compute FNV-1a hash (64-bit by default, compile-time)
[[nodiscard]] consteval uint64_t fnv1a_hash(std::string_view str) noexcept {
    return fnv1a_hash64(str);
}

/// Compute FNV-1a hash (64-bit by default, runtime)
[[nodiscard]] inline uint64_t fnv1a_hash_runtime(std::string_view str) noexcept {
    return fnv1a_hash64_runtime(str);
}

// ============================================================================
// User-defined Literals
// ============================================================================

namespace literals {

/// User-defined literal for compile-time 64-bit hash
/// Usage: "FIX.4.4"_hash
[[nodiscard]] consteval uint64_t operator""_hash(const char* str, std::size_t len) noexcept {
    return fnv1a_hash64(std::string_view{str, len});
}

/// User-defined literal for compile-time 32-bit hash
/// Usage: "D"_hash32
[[nodiscard]] consteval uint32_t operator""_hash32(const char* str, std::size_t len) noexcept {
    return fnv1a_hash32(std::string_view{str, len});
}

} // namespace literals

// ============================================================================
// FIX Message Type Hashes (pre-computed)
// ============================================================================

namespace fix_msg_type_hash {

// Admin messages
inline constexpr uint64_t HEARTBEAT       = fnv1a_hash("0");   // 35=0
inline constexpr uint64_t TEST_REQUEST    = fnv1a_hash("1");   // 35=1
inline constexpr uint64_t RESEND_REQUEST  = fnv1a_hash("2");   // 35=2
inline constexpr uint64_t REJECT          = fnv1a_hash("3");   // 35=3
inline constexpr uint64_t SEQUENCE_RESET  = fnv1a_hash("4");   // 35=4
inline constexpr uint64_t LOGOUT          = fnv1a_hash("5");   // 35=5
inline constexpr uint64_t LOGON           = fnv1a_hash("A");   // 35=A

// Order messages
inline constexpr uint64_t NEW_ORDER_SINGLE      = fnv1a_hash("D");   // 35=D
inline constexpr uint64_t EXECUTION_REPORT      = fnv1a_hash("8");   // 35=8
inline constexpr uint64_t ORDER_CANCEL_REQUEST  = fnv1a_hash("F");   // 35=F
inline constexpr uint64_t ORDER_CANCEL_REJECT   = fnv1a_hash("9");   // 35=9
inline constexpr uint64_t ORDER_STATUS_REQUEST  = fnv1a_hash("H");   // 35=H

// Market data messages
inline constexpr uint64_t MARKET_DATA_REQUEST                   = fnv1a_hash("V");   // 35=V
inline constexpr uint64_t MARKET_DATA_SNAPSHOT                  = fnv1a_hash("W");   // 35=W
inline constexpr uint64_t MARKET_DATA_INCREMENTAL_REFRESH       = fnv1a_hash("X");   // 35=X
inline constexpr uint64_t MARKET_DATA_REQUEST_REJECT            = fnv1a_hash("Y");   // 35=Y

} // namespace fix_msg_type_hash

// ============================================================================
// FIX Version Hashes (pre-computed)
// ============================================================================

namespace fix_version_hash {

inline constexpr uint64_t FIX_4_0 = fnv1a_hash("FIX.4.0");
inline constexpr uint64_t FIX_4_1 = fnv1a_hash("FIX.4.1");
inline constexpr uint64_t FIX_4_2 = fnv1a_hash("FIX.4.2");
inline constexpr uint64_t FIX_4_3 = fnv1a_hash("FIX.4.3");
inline constexpr uint64_t FIX_4_4 = fnv1a_hash("FIX.4.4");
inline constexpr uint64_t FIX_5_0 = fnv1a_hash("FIX.5.0");
inline constexpr uint64_t FIXT_1_1 = fnv1a_hash("FIXT.1.1");

} // namespace fix_version_hash

// ============================================================================
// Hash-based String Comparison
// ============================================================================

/// Compare string to compile-time hash (for switch statements)
/// Usage: if (hash_equals(msg_type, "D"_hash)) { ... }
template<uint64_t Hash>
[[nodiscard]] inline bool hash_equals(std::string_view str) noexcept {
    return fnv1a_hash_runtime(str) == Hash;
}

/// Compare two strings via hash (faster for long strings)
[[nodiscard]] inline bool hash_compare(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    return fnv1a_hash_runtime(a) == fnv1a_hash_runtime(b);
}

// ============================================================================
// Compile-time Collision Detection (for testing)
// ============================================================================

namespace detail {

/// Static assert helper for collision detection
template<uint64_t H1, uint64_t H2>
struct no_hash_collision {
    static_assert(H1 != H2, "Hash collision detected!");
    static constexpr bool value = true;
};

} // namespace detail

// Verify no collisions for common FIX message types
static_assert(detail::no_hash_collision<
    fix_msg_type_hash::NEW_ORDER_SINGLE,
    fix_msg_type_hash::EXECUTION_REPORT
>::value);

static_assert(detail::no_hash_collision<
    fix_msg_type_hash::HEARTBEAT,
    fix_msg_type_hash::LOGON
>::value);

} // namespace nfx::util
