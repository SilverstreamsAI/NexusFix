#pragma once

#include <cstdint>
#include <compare>
#include <concepts>
#include <string_view>
#include <charconv>
#include <limits>

#include "nexusfix/platform/platform.hpp"

namespace nfx {

// ============================================================================
// Strong Type Wrapper Template
// ============================================================================

/// CRTP base for strong types with zero overhead
template <typename Derived, typename T>
struct StrongType {
    T value;

    constexpr StrongType() noexcept : value{} {}
    constexpr explicit StrongType(T v) noexcept : value{v} {}

    [[nodiscard]] constexpr T get() const noexcept { return value; }
    [[nodiscard]] constexpr explicit operator T() const noexcept { return value; }

    constexpr auto operator<=>(const StrongType&) const noexcept = default;
};

// ============================================================================
// Sequence Number (32-bit unsigned, wraps at 2^31-1 per FIX spec)
// ============================================================================

struct SeqNum : StrongType<SeqNum, uint32_t> {
    using StrongType::StrongType;

    static constexpr uint32_t MAX_VALUE = 2147483647u; // 2^31 - 1

    [[nodiscard]] constexpr SeqNum next() const noexcept {
        return SeqNum{value >= MAX_VALUE ? 1u : value + 1u};
    }

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return value > 0 && value <= MAX_VALUE;
    }
};

// ============================================================================
// Fixed-Point Price (8 decimal places, stored as int64_t)
// ============================================================================

struct FixedPrice {
    static constexpr int64_t SCALE = 100000000LL;  // 10^8
    static constexpr int DECIMAL_PLACES = 8;

    int64_t raw;  // Scaled integer value

    constexpr FixedPrice() noexcept : raw{0} {}
    constexpr explicit FixedPrice(int64_t scaled) noexcept : raw{scaled} {}

    /// Construct from double (only for initialization, not hot path)
    static constexpr FixedPrice from_double(double d) noexcept {
        return FixedPrice{static_cast<int64_t>(d * SCALE)};
    }

    /// Convert to double (for display, not hot path)
    [[nodiscard]] constexpr double to_double() const noexcept {
        return static_cast<double>(raw) / SCALE;
    }

    [[nodiscard]] constexpr int64_t scaled() const noexcept { return raw; }

    // Arithmetic operations (all in fixed-point)
    constexpr FixedPrice operator+(FixedPrice other) const noexcept {
        return FixedPrice{raw + other.raw};
    }

    constexpr FixedPrice operator-(FixedPrice other) const noexcept {
        return FixedPrice{raw - other.raw};
    }

    constexpr FixedPrice operator*(int64_t multiplier) const noexcept {
        return FixedPrice{raw * multiplier};
    }

    constexpr FixedPrice operator/(int64_t divisor) const noexcept {
        return FixedPrice{raw / divisor};
    }

    constexpr auto operator<=>(const FixedPrice&) const noexcept = default;

    /// Parse from string view (zero-copy)
    [[nodiscard]] NFX_HOT
    static constexpr FixedPrice from_string(std::string_view sv) noexcept {
        if (sv.empty()) [[unlikely]] return FixedPrice{0};

        bool negative = false;
        size_t pos = 0;

        if (sv[0] == '-') [[unlikely]] {
            negative = true;
            pos = 1;
        }

        int64_t integer_part = 0;
        int64_t fractional_part = 0;
        int fractional_digits = 0;
        bool in_fraction = false;

        for (; pos < sv.size(); ++pos) [[likely]] {
            char c = sv[pos];
            if (c == '.') [[unlikely]] {
                in_fraction = true;
                continue;
            }
            if (c < '0' || c > '9') [[unlikely]] break;

            if (in_fraction) [[unlikely]] {
                if (fractional_digits < DECIMAL_PLACES) [[likely]] {
                    fractional_part = fractional_part * 10 + (c - '0');
                    ++fractional_digits;
                }
            } else {
                integer_part = integer_part * 10 + (c - '0');
            }
        }

        // Scale fractional part to full precision (branch-free multiplication)
        // Use lookup table for power of 10
        static constexpr int64_t POW10[] = {
            100000000LL, 10000000LL, 1000000LL, 100000LL,
            10000LL, 1000LL, 100LL, 10LL, 1LL
        };
        fractional_part *= POW10[fractional_digits];

        int64_t result = integer_part * SCALE + fractional_part;
        return FixedPrice{negative ? -result : result};
    }
};

// ============================================================================
// Quantity (fixed-point, 4 decimal places for fractional shares)
// ============================================================================

struct Qty {
    static constexpr int64_t SCALE = 10000LL;  // 10^4
    static constexpr int DECIMAL_PLACES = 4;

    int64_t raw;

    constexpr Qty() noexcept : raw{0} {}
    constexpr explicit Qty(int64_t scaled) noexcept : raw{scaled} {}

    static constexpr Qty from_int(int64_t whole) noexcept {
        return Qty{whole * SCALE};
    }

    static constexpr Qty from_double(double d) noexcept {
        return Qty{static_cast<int64_t>(d * SCALE)};
    }

    [[nodiscard]] constexpr int64_t whole() const noexcept {
        return raw / SCALE;
    }

    [[nodiscard]] constexpr double to_double() const noexcept {
        return static_cast<double>(raw) / SCALE;
    }

    constexpr Qty operator+(Qty other) const noexcept {
        return Qty{raw + other.raw};
    }

    constexpr Qty operator-(Qty other) const noexcept {
        return Qty{raw - other.raw};
    }

    constexpr auto operator<=>(const Qty&) const noexcept = default;

    [[nodiscard]] NFX_HOT
    static constexpr Qty from_string(std::string_view sv) noexcept {
        if (sv.empty()) [[unlikely]] return Qty{0};

        bool negative = false;
        size_t pos = 0;

        if (sv[0] == '-') [[unlikely]] {
            negative = true;
            pos = 1;
        }

        int64_t integer_part = 0;
        int64_t fractional_part = 0;
        int fractional_digits = 0;
        bool in_fraction = false;

        for (; pos < sv.size(); ++pos) [[likely]] {
            char c = sv[pos];
            if (c == '.') [[unlikely]] {
                in_fraction = true;
                continue;
            }
            if (c < '0' || c > '9') [[unlikely]] break;

            if (in_fraction) [[unlikely]] {
                if (fractional_digits < DECIMAL_PLACES) [[likely]] {
                    fractional_part = fractional_part * 10 + (c - '0');
                    ++fractional_digits;
                }
            } else {
                integer_part = integer_part * 10 + (c - '0');
            }
        }

        // Scale fractional part to full precision (branch-free with lookup table)
        static constexpr int64_t POW10[] = {10000LL, 1000LL, 100LL, 10LL, 1LL};
        fractional_part *= POW10[fractional_digits];

        int64_t result = integer_part * SCALE + fractional_part;
        return Qty{negative ? -result : result};
    }
};

// ============================================================================
// FIX Timestamp (nanoseconds since epoch)
// ============================================================================

struct Timestamp {
    int64_t nanos;  // Nanoseconds since Unix epoch

    constexpr Timestamp() noexcept : nanos{0} {}
    constexpr explicit Timestamp(int64_t ns) noexcept : nanos{ns} {}

    [[nodiscard]] constexpr int64_t as_nanos() const noexcept { return nanos; }
    [[nodiscard]] constexpr int64_t as_micros() const noexcept { return nanos / 1000; }
    [[nodiscard]] constexpr int64_t as_millis() const noexcept { return nanos / 1000000; }
    [[nodiscard]] constexpr int64_t as_seconds() const noexcept { return nanos / 1000000000; }

    constexpr auto operator<=>(const Timestamp&) const noexcept = default;
};

// ============================================================================
// Order/Execution Side
// ============================================================================

enum class Side : char {
    Buy  = '1',
    Sell = '2',
    BuyMinus = '3',
    SellPlus = '4',
    SellShort = '5',
    SellShortExempt = '6',
    Undisclosed = '7',
    Cross = '8',
    CrossShort = '9'
};

[[nodiscard]] constexpr bool is_buy_side(Side s) noexcept {
    return s == Side::Buy || s == Side::BuyMinus;
}

[[nodiscard]] constexpr bool is_sell_side(Side s) noexcept {
    return s == Side::Sell || s == Side::SellPlus ||
           s == Side::SellShort || s == Side::SellShortExempt;
}

// ============================================================================
// Order Type
// ============================================================================

enum class OrdType : char {
    Market          = '1',
    Limit           = '2',
    Stop            = '3',
    StopLimit       = '4',
    MarketOnClose   = '5',
    WithOrWithout   = '6',
    LimitOrBetter   = '7',
    LimitWithOrWithout = '8',
    OnBasis         = '9',
    PreviouslyQuoted = 'D',
    PreviouslyIndicated = 'E',
    Pegged          = 'P'
};

// ============================================================================
// Order Status
// ============================================================================

enum class OrdStatus : char {
    New             = '0',
    PartiallyFilled = '1',
    Filled          = '2',
    DoneForDay      = '3',
    Canceled        = '4',
    Replaced        = '5',
    PendingCancel   = '6',
    Stopped         = '7',
    Rejected        = '8',
    Suspended       = '9',
    PendingNew      = 'A',
    Calculated      = 'B',
    Expired         = 'C',
    AcceptedForBidding = 'D',
    PendingReplace  = 'E'
};

[[nodiscard]] constexpr bool is_terminal_status(OrdStatus s) noexcept {
    return s == OrdStatus::Filled ||
           s == OrdStatus::Canceled ||
           s == OrdStatus::Rejected ||
           s == OrdStatus::Expired ||
           s == OrdStatus::DoneForDay;
}

// ============================================================================
// Execution Type
// ============================================================================

enum class ExecType : char {
    New             = '0',
    PartialFill     = '1',
    Fill            = '2',
    DoneForDay      = '3',
    Canceled        = '4',
    Replaced        = '5',
    PendingCancel   = '6',
    Stopped         = '7',
    Rejected        = '8',
    Suspended       = '9',
    PendingNew      = 'A',
    Calculated      = 'B',
    Expired         = 'C',
    Restated        = 'D',
    PendingReplace  = 'E',
    Trade           = 'F',
    TradeCorrect    = 'G',
    TradeCancel     = 'H',
    OrderStatus     = 'I'
};

// ============================================================================
// Time In Force
// ============================================================================

enum class TimeInForce : char {
    Day             = '0',
    GoodTillCancel  = '1',
    AtTheOpening    = '2',
    ImmediateOrCancel = '3',
    FillOrKill      = '4',
    GoodTillCrossing = '5',
    GoodTillDate    = '6',
    AtTheClose      = '7'
};

// ============================================================================
// User-defined Literals
// ============================================================================

namespace literals {

/// Price literal: 100.50_price
consteval FixedPrice operator""_price(long double d) {
    return FixedPrice::from_double(static_cast<double>(d));
}

/// Quantity literal: 100_qty
consteval Qty operator""_qty(unsigned long long q) {
    return Qty::from_int(static_cast<int64_t>(q));
}

/// Sequence number literal: 1_seq
consteval SeqNum operator""_seq(unsigned long long s) {
    return SeqNum{static_cast<uint32_t>(s)};
}

} // namespace literals

// ============================================================================
// Static Assertions for Type Sizes
// ============================================================================

// Core types should be compact for cache efficiency
static_assert(sizeof(FixedPrice) == 8, "FixedPrice should be 8 bytes (single int64_t)");
static_assert(sizeof(Qty) == 8, "Qty should be 8 bytes (single int64_t)");
static_assert(sizeof(SeqNum) == 4, "SeqNum should be 4 bytes (single uint32_t)");
static_assert(sizeof(Timestamp) == 8, "Timestamp should be 8 bytes (single int64_t)");
static_assert(sizeof(Side) == 1, "Side enum should be 1 byte");
static_assert(sizeof(OrdType) == 1, "OrdType enum should be 1 byte");
static_assert(sizeof(OrdStatus) == 1, "OrdStatus enum should be 1 byte");
static_assert(sizeof(ExecType) == 1, "ExecType enum should be 1 byte");
static_assert(sizeof(TimeInForce) == 1, "TimeInForce enum should be 1 byte");

} // namespace nfx
