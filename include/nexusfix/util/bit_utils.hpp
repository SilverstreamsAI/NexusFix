/*
    NexusFIX Bit-level Utilities

    Modern C++ #83: std::bit_cast for zero-overhead type punning
    Modern C++ #85: Bit manipulation intrinsics

    These utilities provide:
    - Safe type punning with std::bit_cast
    - C++23 bit manipulation intrinsics
    - Fixed-point arithmetic helpers

    All functions are constexpr/noexcept for compile-time evaluation.
*/

#pragma once

#include <bit>
#include <cstdint>
#include <array>
#include <span>
#include <type_traits>

namespace nfx::util {

// ============================================================================
// Safe Type Punning with std::bit_cast
// ============================================================================

/// Parse fixed-size type from byte span (aligned)
/// Requirement: sizeof(T) == N
template<typename T, std::size_t N>
    requires (sizeof(T) == N && std::is_trivially_copyable_v<T>)
[[nodiscard]] constexpr T parse_bytes(std::span<const std::byte, N> bytes) noexcept {
    alignas(T) std::array<std::byte, N> aligned;
    for (std::size_t i = 0; i < N; ++i) {
        aligned[i] = bytes[i];
    }
    return std::bit_cast<T>(aligned);
}

/// Parse uint16_t from 2 bytes (little-endian)
[[nodiscard]] constexpr uint16_t parse_u16_le(const std::byte* data) noexcept {
    return static_cast<uint16_t>(data[0]) |
           (static_cast<uint16_t>(data[1]) << 8);
}

/// Parse uint16_t from 2 bytes (big-endian / network order)
[[nodiscard]] constexpr uint16_t parse_u16_be(const std::byte* data) noexcept {
    return (static_cast<uint16_t>(data[0]) << 8) |
           static_cast<uint16_t>(data[1]);
}

/// Parse uint32_t from 4 bytes (little-endian)
[[nodiscard]] constexpr uint32_t parse_u32_le(const std::byte* data) noexcept {
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

/// Parse uint32_t from 4 bytes (big-endian / network order)
[[nodiscard]] constexpr uint32_t parse_u32_be(const std::byte* data) noexcept {
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           static_cast<uint32_t>(data[3]);
}

/// Parse uint64_t from 8 bytes (little-endian)
[[nodiscard]] constexpr uint64_t parse_u64_le(const std::byte* data) noexcept {
    return static_cast<uint64_t>(data[0]) |
           (static_cast<uint64_t>(data[1]) << 8) |
           (static_cast<uint64_t>(data[2]) << 16) |
           (static_cast<uint64_t>(data[3]) << 24) |
           (static_cast<uint64_t>(data[4]) << 32) |
           (static_cast<uint64_t>(data[5]) << 40) |
           (static_cast<uint64_t>(data[6]) << 48) |
           (static_cast<uint64_t>(data[7]) << 56);
}

/// Parse uint64_t from 8 bytes (big-endian / network order)
[[nodiscard]] constexpr uint64_t parse_u64_be(const std::byte* data) noexcept {
    return (static_cast<uint64_t>(data[0]) << 56) |
           (static_cast<uint64_t>(data[1]) << 48) |
           (static_cast<uint64_t>(data[2]) << 40) |
           (static_cast<uint64_t>(data[3]) << 32) |
           (static_cast<uint64_t>(data[4]) << 24) |
           (static_cast<uint64_t>(data[5]) << 16) |
           (static_cast<uint64_t>(data[6]) << 8) |
           static_cast<uint64_t>(data[7]);
}

/// Parse double from 8 bytes (IEEE 754, little-endian)
[[nodiscard]] inline double parse_double_le(const std::byte* data) noexcept {
    uint64_t bits = parse_u64_le(data);
    return std::bit_cast<double>(bits);
}

/// Parse double from 8 bytes (IEEE 754, big-endian)
[[nodiscard]] inline double parse_double_be(const std::byte* data) noexcept {
    uint64_t bits = parse_u64_be(data);
    return std::bit_cast<double>(bits);
}

// ============================================================================
// Byte Swap (Endianness Conversion)
// ============================================================================

/// Swap bytes of 16-bit value
[[nodiscard]] constexpr uint16_t byteswap16(uint16_t value) noexcept {
#if __cpp_lib_byteswap >= 202110L
    return std::byteswap(value);
#else
    return static_cast<uint16_t>((value >> 8) | (value << 8));
#endif
}

/// Swap bytes of 32-bit value
[[nodiscard]] constexpr uint32_t byteswap32(uint32_t value) noexcept {
#if __cpp_lib_byteswap >= 202110L
    return std::byteswap(value);
#else
    return ((value >> 24) & 0x000000FF) |
           ((value >> 8)  & 0x0000FF00) |
           ((value << 8)  & 0x00FF0000) |
           ((value << 24) & 0xFF000000);
#endif
}

/// Swap bytes of 64-bit value
[[nodiscard]] constexpr uint64_t byteswap64(uint64_t value) noexcept {
#if __cpp_lib_byteswap >= 202110L
    return std::byteswap(value);
#else
    return ((value >> 56) & 0x00000000000000FFULL) |
           ((value >> 40) & 0x000000000000FF00ULL) |
           ((value >> 24) & 0x0000000000FF0000ULL) |
           ((value >> 8)  & 0x00000000FF000000ULL) |
           ((value << 8)  & 0x000000FF00000000ULL) |
           ((value << 24) & 0x0000FF0000000000ULL) |
           ((value << 40) & 0x00FF000000000000ULL) |
           ((value << 56) & 0xFF00000000000000ULL);
#endif
}

// ============================================================================
// Bit Manipulation Intrinsics (C++20)
// ============================================================================

/// Count leading zeros
template<std::unsigned_integral T>
[[nodiscard]] constexpr int countl_zero(T value) noexcept {
    return std::countl_zero(value);
}

/// Count trailing zeros
template<std::unsigned_integral T>
[[nodiscard]] constexpr int countr_zero(T value) noexcept {
    return std::countr_zero(value);
}

/// Count number of 1 bits (population count)
template<std::unsigned_integral T>
[[nodiscard]] constexpr int popcount(T value) noexcept {
    return std::popcount(value);
}

/// Check if value is power of 2
template<std::unsigned_integral T>
[[nodiscard]] constexpr bool is_power_of_two(T value) noexcept {
    return std::has_single_bit(value);
}

/// Round up to next power of 2
template<std::unsigned_integral T>
[[nodiscard]] constexpr T next_power_of_two(T value) noexcept {
    return std::bit_ceil(value);
}

/// Round down to previous power of 2
template<std::unsigned_integral T>
[[nodiscard]] constexpr T prev_power_of_two(T value) noexcept {
    return std::bit_floor(value);
}

/// Get bit width (minimum bits to represent value)
template<std::unsigned_integral T>
[[nodiscard]] constexpr int bit_width(T value) noexcept {
    return std::bit_width(value);
}

// ============================================================================
// Bit Field Extraction
// ============================================================================

/// Extract bits [start, start+width) from value
template<std::unsigned_integral T>
[[nodiscard]] constexpr T extract_bits(T value, int start, int width) noexcept {
    T mask = (static_cast<T>(1) << width) - 1;
    return (value >> start) & mask;
}

/// Insert bits into value at position [start, start+width)
template<std::unsigned_integral T>
[[nodiscard]] constexpr T insert_bits(T value, T bits, int start, int width) noexcept {
    T mask = (static_cast<T>(1) << width) - 1;
    value &= ~(mask << start);          // Clear target bits
    value |= (bits & mask) << start;    // Insert new bits
    return value;
}

/// Set bit at position
template<std::unsigned_integral T>
[[nodiscard]] constexpr T set_bit(T value, int pos) noexcept {
    return value | (static_cast<T>(1) << pos);
}

/// Clear bit at position
template<std::unsigned_integral T>
[[nodiscard]] constexpr T clear_bit(T value, int pos) noexcept {
    return value & ~(static_cast<T>(1) << pos);
}

/// Toggle bit at position
template<std::unsigned_integral T>
[[nodiscard]] constexpr T toggle_bit(T value, int pos) noexcept {
    return value ^ (static_cast<T>(1) << pos);
}

/// Test if bit at position is set
template<std::unsigned_integral T>
[[nodiscard]] constexpr bool test_bit(T value, int pos) noexcept {
    return (value >> pos) & 1;
}

// ============================================================================
// Alignment Utilities
// ============================================================================

/// Check if value is aligned to alignment
template<std::unsigned_integral T>
[[nodiscard]] constexpr bool is_aligned(T value, T alignment) noexcept {
    return (value & (alignment - 1)) == 0;
}

/// Round up value to alignment
template<std::unsigned_integral T>
[[nodiscard]] constexpr T align_up(T value, T alignment) noexcept {
    return (value + alignment - 1) & ~(alignment - 1);
}

/// Round down value to alignment
template<std::unsigned_integral T>
[[nodiscard]] constexpr T align_down(T value, T alignment) noexcept {
    return value & ~(alignment - 1);
}

/// Check if pointer is aligned
template<typename T>
[[nodiscard]] inline bool is_pointer_aligned(const T* ptr, std::size_t alignment) noexcept {
    return (reinterpret_cast<std::uintptr_t>(ptr) & (alignment - 1)) == 0;
}

// ============================================================================
// Fixed-Point Arithmetic (for prices)
// ============================================================================

/// Convert double to fixed-point with N decimal places
template<int Decimals = 8>
[[nodiscard]] constexpr int64_t to_fixed(double value) noexcept {
    constexpr double scale = []() {
        double s = 1.0;
        for (int i = 0; i < Decimals; ++i) s *= 10.0;
        return s;
    }();
    return static_cast<int64_t>(value * scale + (value >= 0 ? 0.5 : -0.5));
}

/// Convert fixed-point to double
template<int Decimals = 8>
[[nodiscard]] constexpr double from_fixed(int64_t value) noexcept {
    constexpr double scale = []() {
        double s = 1.0;
        for (int i = 0; i < Decimals; ++i) s *= 10.0;
        return s;
    }();
    return static_cast<double>(value) / scale;
}

} // namespace nfx::util
