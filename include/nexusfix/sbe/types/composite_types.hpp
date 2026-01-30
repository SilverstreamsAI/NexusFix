// SPDX-License-Identifier: MIT
// Copyright (c) 2025 SilverstreamsAI

#pragma once

#include <algorithm>
#include <cstring>
#include <string_view>

#include "nexusfix/platform/platform.hpp"
#include "nexusfix/sbe/types/sbe_types.hpp"
#include "nexusfix/types/field_types.hpp"

namespace nfx::sbe {

// ============================================================================
// FixedString<N>: Space-Padded Fixed-Length String
// ============================================================================
// SBE fixed-length strings are right-padded with spaces (ASCII 0x20).
// Provides zero-copy decode and efficient encode operations.

template <std::size_t N>
class FixedString {
public:
    static constexpr std::size_t SIZE = N;
    static constexpr char PADDING_CHAR = ' ';

    // Decode: Extract string_view from buffer (strips trailing spaces)
    [[nodiscard]] NFX_FORCE_INLINE static std::string_view decode(
        const char* NFX_RESTRICT buffer) noexcept {
        // Find last non-space character
        std::size_t len = N;
        while (len > 0 && buffer[len - 1] == PADDING_CHAR) {
            --len;
        }
        return std::string_view{buffer, len};
    }

    // Encode: Write string to buffer with space padding
    NFX_FORCE_INLINE static void encode(
        char* NFX_RESTRICT buffer, std::string_view value) noexcept {
        const std::size_t copy_len = std::min(value.size(), N);
        std::memcpy(buffer, value.data(), copy_len);
        // Pad remaining bytes with spaces
        if (copy_len < N) {
            std::memset(buffer + copy_len, PADDING_CHAR, N - copy_len);
        }
    }

    // Clear: Fill buffer with spaces
    NFX_FORCE_INLINE static void clear(char* NFX_RESTRICT buffer) noexcept {
        std::memset(buffer, PADDING_CHAR, N);
    }

    // Check if buffer contains a null/empty string
    [[nodiscard]] NFX_FORCE_INLINE static bool is_null(
        const char* NFX_RESTRICT buffer) noexcept {
        // All spaces or all nulls indicates null value
        for (std::size_t i = 0; i < N; ++i) {
            if (buffer[i] != PADDING_CHAR && buffer[i] != '\0') {
                return false;
            }
        }
        return true;
    }
};

// Common fixed string sizes
using FixedString8 = FixedString<8>;
using FixedString20 = FixedString<20>;

// ============================================================================
// DecimalPrice: Wraps FixedPrice for SBE encoding
// ============================================================================
// Uses existing nfx::FixedPrice (int64, 8 decimals) for internal representation.
// SBE storage: raw int64 value directly (no conversion needed).

class DecimalPrice {
public:
    static constexpr std::size_t SIZE = sizeof(SbeInt64);

    // Decode: Read int64 and wrap as FixedPrice
    [[nodiscard]] NFX_FORCE_INLINE static FixedPrice decode(
        const char* NFX_RESTRICT buffer) noexcept {
        FixedPrice price;
        price.raw = read_int64(buffer);
        return price;
    }

    // Encode: Write FixedPrice raw value as int64
    NFX_FORCE_INLINE static void encode(
        char* NFX_RESTRICT buffer, FixedPrice value) noexcept {
        write_int64(buffer, value.raw);
    }

    // Check if value is null (uses SBE null sentinel)
    [[nodiscard]] NFX_FORCE_INLINE static bool is_null(
        const char* NFX_RESTRICT buffer) noexcept {
        return read_int64(buffer) == null_value::INT64;
    }

    // Write null value
    NFX_FORCE_INLINE static void write_null(
        char* NFX_RESTRICT buffer) noexcept {
        write_int64(buffer, null_value::INT64);
    }
};

// ============================================================================
// DecimalQty: Wraps Qty for SBE encoding
// ============================================================================
// Uses existing nfx::Qty (int64, 4 decimals) for internal representation.

class DecimalQty {
public:
    static constexpr std::size_t SIZE = sizeof(SbeInt64);

    // Decode: Read int64 and wrap as Qty
    [[nodiscard]] NFX_FORCE_INLINE static Qty decode(
        const char* NFX_RESTRICT buffer) noexcept {
        Qty qty;
        qty.raw = read_int64(buffer);
        return qty;
    }

    // Encode: Write Qty raw value as int64
    NFX_FORCE_INLINE static void encode(
        char* NFX_RESTRICT buffer, Qty value) noexcept {
        write_int64(buffer, value.raw);
    }

    // Check if value is null
    [[nodiscard]] NFX_FORCE_INLINE static bool is_null(
        const char* NFX_RESTRICT buffer) noexcept {
        return read_int64(buffer) == null_value::INT64;
    }

    // Write null value
    NFX_FORCE_INLINE static void write_null(
        char* NFX_RESTRICT buffer) noexcept {
        write_int64(buffer, null_value::INT64);
    }
};

// ============================================================================
// SbeTimestamp: Wraps Timestamp for SBE encoding
// ============================================================================
// Uses existing nfx::Timestamp (int64 nanoseconds since epoch).

class SbeTimestamp {
public:
    static constexpr std::size_t SIZE = sizeof(SbeInt64);

    // Decode: Read int64 as Timestamp
    [[nodiscard]] NFX_FORCE_INLINE static Timestamp decode(
        const char* NFX_RESTRICT buffer) noexcept {
        return Timestamp{read_int64(buffer)};
    }

    // Encode: Write Timestamp nanos as int64
    NFX_FORCE_INLINE static void encode(
        char* NFX_RESTRICT buffer, Timestamp value) noexcept {
        write_int64(buffer, value.nanos);
    }

    // Check if value is null
    [[nodiscard]] NFX_FORCE_INLINE static bool is_null(
        const char* NFX_RESTRICT buffer) noexcept {
        return read_int64(buffer) == null_value::INT64;
    }

    // Write null value
    NFX_FORCE_INLINE static void write_null(
        char* NFX_RESTRICT buffer) noexcept {
        write_int64(buffer, null_value::INT64);
    }
};

// ============================================================================
// SBE Enum Wrappers
// ============================================================================
// Wrap existing single-byte enums for SBE encoding.
// These use the same char representation as FIX protocol.

class SbeSide {
public:
    static constexpr std::size_t SIZE = sizeof(SbeChar);

    [[nodiscard]] NFX_FORCE_INLINE static Side decode(
        const char* NFX_RESTRICT buffer) noexcept {
        return static_cast<Side>(buffer[0]);
    }

    NFX_FORCE_INLINE static void encode(
        char* NFX_RESTRICT buffer, Side value) noexcept {
        buffer[0] = static_cast<char>(value);
    }

    [[nodiscard]] NFX_FORCE_INLINE static bool is_null(
        const char* NFX_RESTRICT buffer) noexcept {
        return buffer[0] == null_value::CHAR;
    }

    NFX_FORCE_INLINE static void write_null(
        char* NFX_RESTRICT buffer) noexcept {
        buffer[0] = null_value::CHAR;
    }
};

class SbeOrdType {
public:
    static constexpr std::size_t SIZE = sizeof(SbeChar);

    [[nodiscard]] NFX_FORCE_INLINE static OrdType decode(
        const char* NFX_RESTRICT buffer) noexcept {
        return static_cast<OrdType>(buffer[0]);
    }

    NFX_FORCE_INLINE static void encode(
        char* NFX_RESTRICT buffer, OrdType value) noexcept {
        buffer[0] = static_cast<char>(value);
    }

    [[nodiscard]] NFX_FORCE_INLINE static bool is_null(
        const char* NFX_RESTRICT buffer) noexcept {
        return buffer[0] == null_value::CHAR;
    }

    NFX_FORCE_INLINE static void write_null(
        char* NFX_RESTRICT buffer) noexcept {
        buffer[0] = null_value::CHAR;
    }
};

class SbeExecType {
public:
    static constexpr std::size_t SIZE = sizeof(SbeChar);

    [[nodiscard]] NFX_FORCE_INLINE static ExecType decode(
        const char* NFX_RESTRICT buffer) noexcept {
        return static_cast<ExecType>(buffer[0]);
    }

    NFX_FORCE_INLINE static void encode(
        char* NFX_RESTRICT buffer, ExecType value) noexcept {
        buffer[0] = static_cast<char>(value);
    }

    [[nodiscard]] NFX_FORCE_INLINE static bool is_null(
        const char* NFX_RESTRICT buffer) noexcept {
        return buffer[0] == null_value::CHAR;
    }

    NFX_FORCE_INLINE static void write_null(
        char* NFX_RESTRICT buffer) noexcept {
        buffer[0] = null_value::CHAR;
    }
};

class SbeOrdStatus {
public:
    static constexpr std::size_t SIZE = sizeof(SbeChar);

    [[nodiscard]] NFX_FORCE_INLINE static OrdStatus decode(
        const char* NFX_RESTRICT buffer) noexcept {
        return static_cast<OrdStatus>(buffer[0]);
    }

    NFX_FORCE_INLINE static void encode(
        char* NFX_RESTRICT buffer, OrdStatus value) noexcept {
        buffer[0] = static_cast<char>(value);
    }

    [[nodiscard]] NFX_FORCE_INLINE static bool is_null(
        const char* NFX_RESTRICT buffer) noexcept {
        return buffer[0] == null_value::CHAR;
    }

    NFX_FORCE_INLINE static void write_null(
        char* NFX_RESTRICT buffer) noexcept {
        buffer[0] = null_value::CHAR;
    }
};

class SbeTimeInForce {
public:
    static constexpr std::size_t SIZE = sizeof(SbeChar);

    [[nodiscard]] NFX_FORCE_INLINE static TimeInForce decode(
        const char* NFX_RESTRICT buffer) noexcept {
        return static_cast<TimeInForce>(buffer[0]);
    }

    NFX_FORCE_INLINE static void encode(
        char* NFX_RESTRICT buffer, TimeInForce value) noexcept {
        buffer[0] = static_cast<char>(value);
    }

    [[nodiscard]] NFX_FORCE_INLINE static bool is_null(
        const char* NFX_RESTRICT buffer) noexcept {
        return buffer[0] == null_value::CHAR;
    }

    NFX_FORCE_INLINE static void write_null(
        char* NFX_RESTRICT buffer) noexcept {
        buffer[0] = null_value::CHAR;
    }
};

}  // namespace nfx::sbe
