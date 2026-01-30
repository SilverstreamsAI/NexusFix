// SPDX-License-Identifier: MIT
// Copyright (c) 2025 SilverstreamsAI

#pragma once

#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>

#include "nexusfix/platform/platform.hpp"

namespace nfx::sbe {

// ============================================================================
// SBE Primitive Type Aliases
// ============================================================================

using SbeChar = char;
using SbeInt8 = std::int8_t;
using SbeInt16 = std::int16_t;
using SbeInt32 = std::int32_t;
using SbeInt64 = std::int64_t;
using SbeUint8 = std::uint8_t;
using SbeUint16 = std::uint16_t;
using SbeUint32 = std::uint32_t;
using SbeUint64 = std::uint64_t;

// ============================================================================
// SBE Null Value Constants (per SBE specification)
// ============================================================================

namespace null_value {

inline constexpr SbeChar CHAR = '\0';
inline constexpr SbeInt8 INT8 = static_cast<SbeInt8>(INT8_MIN);
inline constexpr SbeInt16 INT16 = static_cast<SbeInt16>(INT16_MIN);
inline constexpr SbeInt32 INT32 = INT32_MIN;
inline constexpr SbeInt64 INT64 = INT64_MIN;
inline constexpr SbeUint8 UINT8 = UINT8_MAX;
inline constexpr SbeUint16 UINT16 = UINT16_MAX;
inline constexpr SbeUint32 UINT32 = UINT32_MAX;
inline constexpr SbeUint64 UINT64 = UINT64_MAX;

}  // namespace null_value

// ============================================================================
// Little-Endian Read/Write Utilities
// ============================================================================
// On x86/x64 (little-endian), these compile to simple load/store instructions.
// The compiler optimizes away the byte manipulation for LE architectures.

template <typename T>
    requires std::is_trivially_copyable_v<T>
[[nodiscard]] NFX_FORCE_INLINE constexpr T read_le(
    const char* NFX_RESTRICT buffer) noexcept {
    T value{};
    if (std::is_constant_evaluated()) {
        // Constexpr path: manual byte-by-byte construction
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            if constexpr (sizeof(T) == 1) {
                value = static_cast<T>(static_cast<unsigned char>(buffer[0]));
            } else {
                value |= static_cast<T>(
                    static_cast<unsigned char>(buffer[i])) << (i * 8);
            }
        }
    } else {
        // Runtime path: direct memcpy (optimized to single instruction on x86)
        std::memcpy(&value, buffer, sizeof(T));
    }
    return value;
}

template <typename T>
    requires std::is_trivially_copyable_v<T>
NFX_FORCE_INLINE constexpr void write_le(
    char* NFX_RESTRICT buffer, T value) noexcept {
    if (std::is_constant_evaluated()) {
        // Constexpr path: manual byte-by-byte write
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            buffer[i] = static_cast<char>(
                (static_cast<std::make_unsigned_t<T>>(value) >> (i * 8)) & 0xFF);
        }
    } else {
        // Runtime path: direct memcpy (optimized to single instruction on x86)
        std::memcpy(buffer, &value, sizeof(T));
    }
}

// ============================================================================
// Convenience Read/Write for Specific Types
// ============================================================================

[[nodiscard]] NFX_FORCE_INLINE SbeInt64 read_int64(
    const char* buffer) noexcept {
    return read_le<SbeInt64>(buffer);
}

[[nodiscard]] NFX_FORCE_INLINE SbeUint64 read_uint64(
    const char* buffer) noexcept {
    return read_le<SbeUint64>(buffer);
}

[[nodiscard]] NFX_FORCE_INLINE SbeInt32 read_int32(
    const char* buffer) noexcept {
    return read_le<SbeInt32>(buffer);
}

[[nodiscard]] NFX_FORCE_INLINE SbeUint32 read_uint32(
    const char* buffer) noexcept {
    return read_le<SbeUint32>(buffer);
}

[[nodiscard]] NFX_FORCE_INLINE SbeInt16 read_int16(
    const char* buffer) noexcept {
    return read_le<SbeInt16>(buffer);
}

[[nodiscard]] NFX_FORCE_INLINE SbeUint16 read_uint16(
    const char* buffer) noexcept {
    return read_le<SbeUint16>(buffer);
}

[[nodiscard]] NFX_FORCE_INLINE SbeInt8 read_int8(
    const char* buffer) noexcept {
    return read_le<SbeInt8>(buffer);
}

[[nodiscard]] NFX_FORCE_INLINE SbeUint8 read_uint8(
    const char* buffer) noexcept {
    return read_le<SbeUint8>(buffer);
}

[[nodiscard]] NFX_FORCE_INLINE SbeChar read_char(
    const char* buffer) noexcept {
    return buffer[0];
}

NFX_FORCE_INLINE void write_int64(char* buffer, SbeInt64 value) noexcept {
    write_le(buffer, value);
}

NFX_FORCE_INLINE void write_uint64(char* buffer, SbeUint64 value) noexcept {
    write_le(buffer, value);
}

NFX_FORCE_INLINE void write_int32(char* buffer, SbeInt32 value) noexcept {
    write_le(buffer, value);
}

NFX_FORCE_INLINE void write_uint32(char* buffer, SbeUint32 value) noexcept {
    write_le(buffer, value);
}

NFX_FORCE_INLINE void write_int16(char* buffer, SbeInt16 value) noexcept {
    write_le(buffer, value);
}

NFX_FORCE_INLINE void write_uint16(char* buffer, SbeUint16 value) noexcept {
    write_le(buffer, value);
}

NFX_FORCE_INLINE void write_int8(char* buffer, SbeInt8 value) noexcept {
    write_le(buffer, value);
}

NFX_FORCE_INLINE void write_uint8(char* buffer, SbeUint8 value) noexcept {
    write_le(buffer, value);
}

NFX_FORCE_INLINE void write_char(char* buffer, SbeChar value) noexcept {
    buffer[0] = value;
}

// ============================================================================
// Buffer Bounds Checking Utility
// ============================================================================

[[nodiscard]] NFX_FORCE_INLINE constexpr bool check_bounds(
    std::size_t offset, std::size_t field_size,
    std::size_t buffer_size) noexcept {
    return offset + field_size <= buffer_size;
}

}  // namespace nfx::sbe
