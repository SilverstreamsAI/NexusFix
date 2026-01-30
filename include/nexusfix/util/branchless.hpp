/*
    NexusFIX Branch-free Programming Utilities

    Modern C++ #84: Branch-free Programming

    Branch misprediction penalty: ~15-20 cycles on modern CPUs.
    These utilities eliminate branches in hot paths using:
    - Arithmetic/bitwise operations instead of conditionals
    - Branchless min/max/clamp
    - Conditional selection without branches

    All functions are constexpr/noexcept for compile-time optimization.
*/

#pragma once

#include <cstdint>
#include <type_traits>
#include <concepts>

namespace nfx::util {

// ============================================================================
// Branchless Min/Max
// ============================================================================

/// Branchless minimum for signed integers
template<std::signed_integral T>
[[nodiscard]] constexpr T branchless_min(T a, T b) noexcept {
    // Use conditional move semantics
    // Most compilers optimize this to cmov instruction
    T diff = a - b;
    T mask = diff >> (sizeof(T) * 8 - 1);  // Arithmetic shift: -1 if a < b, 0 otherwise
    return b + (diff & mask);
}

/// Branchless maximum for signed integers
template<std::signed_integral T>
[[nodiscard]] constexpr T branchless_max(T a, T b) noexcept {
    T diff = a - b;
    T mask = diff >> (sizeof(T) * 8 - 1);
    return a - (diff & mask);
}

/// Branchless minimum for unsigned integers
template<std::unsigned_integral T>
[[nodiscard]] constexpr T branchless_min(T a, T b) noexcept {
    // For unsigned, we use subtraction borrow
    return b ^ ((a ^ b) & -static_cast<T>(a < b));
}

/// Branchless maximum for unsigned integers
template<std::unsigned_integral T>
[[nodiscard]] constexpr T branchless_max(T a, T b) noexcept {
    return a ^ ((a ^ b) & -static_cast<T>(a < b));
}

// ============================================================================
// Branchless Conditional Selection
// ============================================================================

/// Select a if condition is true, b otherwise (branchless)
template<std::integral T>
[[nodiscard]] constexpr T branchless_select(bool condition, T a, T b) noexcept {
    // condition: true -> -1 (all 1s), false -> 0
    T mask = -static_cast<T>(condition);
    return (a & mask) | (b & ~mask);
}

/// Select a if condition is true, b otherwise (for pointers)
template<typename T>
[[nodiscard]] inline T* branchless_select_ptr(bool condition, T* a, T* b) noexcept {
    auto mask = -static_cast<std::uintptr_t>(condition);
    auto result = (reinterpret_cast<std::uintptr_t>(a) & mask) |
                  (reinterpret_cast<std::uintptr_t>(b) & ~mask);
    return reinterpret_cast<T*>(result);
}

// ============================================================================
// Branchless Clamp
// ============================================================================

/// Clamp value to range [lo, hi] without branches
template<std::signed_integral T>
[[nodiscard]] constexpr T branchless_clamp(T value, T lo, T hi) noexcept {
    return branchless_min(branchless_max(value, lo), hi);
}

/// Clamp value to range [lo, hi] without branches
template<std::unsigned_integral T>
[[nodiscard]] constexpr T branchless_clamp(T value, T lo, T hi) noexcept {
    return branchless_min(branchless_max(value, lo), hi);
}

// ============================================================================
// Branchless Absolute Value
// ============================================================================

/// Absolute value without branches
template<std::signed_integral T>
[[nodiscard]] constexpr T branchless_abs(T value) noexcept {
    T mask = value >> (sizeof(T) * 8 - 1);  // -1 if negative, 0 if positive
    return (value ^ mask) - mask;
}

// ============================================================================
// Branchless Sign Functions
// ============================================================================

/// Return sign: -1, 0, or 1 (branchless)
template<std::signed_integral T>
[[nodiscard]] constexpr int branchless_sign(T value) noexcept {
    return (value > 0) - (value < 0);
}

/// Return 1 if non-negative, -1 if negative (branchless)
template<std::signed_integral T>
[[nodiscard]] constexpr int branchless_signum(T value) noexcept {
    return 1 | (static_cast<int>(value) >> (sizeof(int) * 8 - 1));
}

/// Check if same sign (branchless)
template<std::signed_integral T>
[[nodiscard]] constexpr bool branchless_same_sign(T a, T b) noexcept {
    return (a ^ b) >= 0;
}

// ============================================================================
// Branchless Comparisons
// ============================================================================

/// Convert bool to -1 (true) or 0 (false)
[[nodiscard]] constexpr int32_t bool_to_mask32(bool value) noexcept {
    return -static_cast<int32_t>(value);
}

/// Convert bool to -1 (true) or 0 (false)
[[nodiscard]] constexpr int64_t bool_to_mask64(bool value) noexcept {
    return -static_cast<int64_t>(value);
}

/// Branchless less-than returning 1 or 0
template<std::integral T>
[[nodiscard]] constexpr T branchless_lt(T a, T b) noexcept {
    return static_cast<T>(a < b);
}

/// Branchless greater-than returning 1 or 0
template<std::integral T>
[[nodiscard]] constexpr T branchless_gt(T a, T b) noexcept {
    return static_cast<T>(a > b);
}

/// Branchless equal returning 1 or 0
template<std::integral T>
[[nodiscard]] constexpr T branchless_eq(T a, T b) noexcept {
    return static_cast<T>(a == b);
}

// ============================================================================
// Branchless Range Checks
// ============================================================================

/// Check if value is in range [lo, hi] (inclusive, branchless)
template<std::integral T>
[[nodiscard]] constexpr bool in_range(T value, T lo, T hi) noexcept {
    // Single comparison: (value - lo) <= (hi - lo)
    // Works for both signed and unsigned
    return static_cast<std::make_unsigned_t<T>>(value - lo) <=
           static_cast<std::make_unsigned_t<T>>(hi - lo);
}

/// Check if ASCII character is digit '0'-'9' (branchless)
[[nodiscard]] constexpr bool is_digit(char c) noexcept {
    return in_range(c, '0', '9');
}

/// Check if ASCII character is uppercase 'A'-'Z' (branchless)
[[nodiscard]] constexpr bool is_upper(char c) noexcept {
    return in_range(c, 'A', 'Z');
}

/// Check if ASCII character is lowercase 'a'-'z' (branchless)
[[nodiscard]] constexpr bool is_lower(char c) noexcept {
    return in_range(c, 'a', 'z');
}

/// Check if ASCII character is letter (branchless)
[[nodiscard]] constexpr bool is_alpha(char c) noexcept {
    // Convert to uppercase then check
    return in_range(static_cast<char>(c | 0x20), 'a', 'z');
}

/// Check if ASCII character is alphanumeric (branchless)
[[nodiscard]] constexpr bool is_alnum(char c) noexcept {
    return is_digit(c) | is_alpha(c);
}

// ============================================================================
// Branchless ASCII Conversion
// ============================================================================

/// Convert ASCII digit to integer (assumes valid digit, branchless)
[[nodiscard]] constexpr int digit_to_int(char c) noexcept {
    return c - '0';
}

/// Convert integer [0-9] to ASCII digit (branchless)
[[nodiscard]] constexpr char int_to_digit(int n) noexcept {
    return static_cast<char>('0' + n);
}

/// Convert lowercase to uppercase (branchless, no-op if already upper)
[[nodiscard]] constexpr char to_upper(char c) noexcept {
    // Clear bit 5 if lowercase letter
    return static_cast<char>(c & ~(is_lower(c) << 5));
}

/// Convert uppercase to lowercase (branchless, no-op if already lower)
[[nodiscard]] constexpr char to_lower(char c) noexcept {
    // Set bit 5 if uppercase letter
    return static_cast<char>(c | (is_upper(c) << 5));
}

// ============================================================================
// Branchless Integer Parsing
// ============================================================================

/// Parse single ASCII digit, return -1 if invalid (branchless)
[[nodiscard]] constexpr int parse_digit(char c) noexcept {
    int digit = c - '0';
    int valid_mask = -static_cast<int>(in_range(c, '0', '9'));
    int invalid_mask = ~valid_mask;
    return (digit & valid_mask) | (-1 & invalid_mask);
}

/// Parse up to 2 ASCII digits (00-99), return -1 if invalid
[[nodiscard]] constexpr int parse_2digits(const char* s) noexcept {
    int d0 = parse_digit(s[0]);
    int d1 = parse_digit(s[1]);
    // Both valid if neither is -1
    int both_valid = ~((d0 | d1) >> 31);  // 0 if any is -1, -1 otherwise
    int result = d0 * 10 + d1;
    return (result & both_valid) | (-1 & ~both_valid);
}

/// Parse up to 4 ASCII digits (0000-9999), return -1 if invalid
[[nodiscard]] constexpr int parse_4digits(const char* s) noexcept {
    int d0 = parse_digit(s[0]);
    int d1 = parse_digit(s[1]);
    int d2 = parse_digit(s[2]);
    int d3 = parse_digit(s[3]);
    int all_valid = ~((d0 | d1 | d2 | d3) >> 31);
    int result = d0 * 1000 + d1 * 100 + d2 * 10 + d3;
    return (result & all_valid) | (-1 & ~all_valid);
}

// ============================================================================
// Branchless Conditional Increment/Decrement
// ============================================================================

/// Add delta if condition is true (branchless)
template<std::integral T>
[[nodiscard]] constexpr T conditional_add(T value, T delta, bool condition) noexcept {
    return value + (delta & -static_cast<T>(condition));
}

/// Subtract delta if condition is true (branchless)
template<std::integral T>
[[nodiscard]] constexpr T conditional_sub(T value, T delta, bool condition) noexcept {
    return value - (delta & -static_cast<T>(condition));
}

/// Increment if condition is true (branchless)
template<std::integral T>
[[nodiscard]] constexpr T conditional_inc(T value, bool condition) noexcept {
    return value + static_cast<T>(condition);
}

/// Decrement if condition is true (branchless)
template<std::integral T>
[[nodiscard]] constexpr T conditional_dec(T value, bool condition) noexcept {
    return value - static_cast<T>(condition);
}

// ============================================================================
// Branchless Null Check
// ============================================================================

/// Return ptr if non-null, otherwise return fallback (branchless)
template<typename T>
[[nodiscard]] inline T* null_coalesce(T* ptr, T* fallback) noexcept {
    return branchless_select_ptr(ptr != nullptr, ptr, fallback);
}

} // namespace nfx::util
