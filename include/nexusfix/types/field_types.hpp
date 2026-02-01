#pragma once

#include <array>
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

// Compile-time Side lookup (TICKET_023)
namespace detail {
    template<Side S> struct SideInfo { static constexpr std::string_view name = "Unknown"; };
    template<> struct SideInfo<Side::Buy> { static constexpr std::string_view name = "Buy"; };
    template<> struct SideInfo<Side::Sell> { static constexpr std::string_view name = "Sell"; };
    template<> struct SideInfo<Side::BuyMinus> { static constexpr std::string_view name = "BuyMinus"; };
    template<> struct SideInfo<Side::SellPlus> { static constexpr std::string_view name = "SellPlus"; };
    template<> struct SideInfo<Side::SellShort> { static constexpr std::string_view name = "SellShort"; };
    template<> struct SideInfo<Side::SellShortExempt> { static constexpr std::string_view name = "SellShortExempt"; };
    template<> struct SideInfo<Side::Undisclosed> { static constexpr std::string_view name = "Undisclosed"; };
    template<> struct SideInfo<Side::Cross> { static constexpr std::string_view name = "Cross"; };
    template<> struct SideInfo<Side::CrossShort> { static constexpr std::string_view name = "CrossShort"; };

    consteval std::array<std::string_view, 10> create_side_table() {
        std::array<std::string_view, 10> table{};
        for (auto& e : table) e = "Unknown";
        table['1' - '0'] = SideInfo<Side::Buy>::name;
        table['2' - '0'] = SideInfo<Side::Sell>::name;
        table['3' - '0'] = SideInfo<Side::BuyMinus>::name;
        table['4' - '0'] = SideInfo<Side::SellPlus>::name;
        table['5' - '0'] = SideInfo<Side::SellShort>::name;
        table['6' - '0'] = SideInfo<Side::SellShortExempt>::name;
        table['7' - '0'] = SideInfo<Side::Undisclosed>::name;
        table['8' - '0'] = SideInfo<Side::Cross>::name;
        table['9' - '0'] = SideInfo<Side::CrossShort>::name;
        return table;
    }
    inline constexpr auto SIDE_TABLE = create_side_table();
}

template<Side S>
[[nodiscard]] consteval std::string_view side_name() noexcept {
    return detail::SideInfo<S>::name;
}

[[nodiscard]] inline constexpr std::string_view side_name(Side s) noexcept {
    const auto idx = static_cast<char>(s) - '0';
    if (idx >= 0 && idx < 10) [[likely]] {
        return detail::SIDE_TABLE[idx];
    }
    return "Unknown";
}

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

// Compile-time OrdType lookup (TICKET_023)
namespace detail {
    template<OrdType T> struct OrdTypeInfo { static constexpr std::string_view name = "Unknown"; };
    template<> struct OrdTypeInfo<OrdType::Market> { static constexpr std::string_view name = "Market"; };
    template<> struct OrdTypeInfo<OrdType::Limit> { static constexpr std::string_view name = "Limit"; };
    template<> struct OrdTypeInfo<OrdType::Stop> { static constexpr std::string_view name = "Stop"; };
    template<> struct OrdTypeInfo<OrdType::StopLimit> { static constexpr std::string_view name = "StopLimit"; };
    template<> struct OrdTypeInfo<OrdType::MarketOnClose> { static constexpr std::string_view name = "MarketOnClose"; };
    template<> struct OrdTypeInfo<OrdType::WithOrWithout> { static constexpr std::string_view name = "WithOrWithout"; };
    template<> struct OrdTypeInfo<OrdType::LimitOrBetter> { static constexpr std::string_view name = "LimitOrBetter"; };
    template<> struct OrdTypeInfo<OrdType::LimitWithOrWithout> { static constexpr std::string_view name = "LimitWithOrWithout"; };
    template<> struct OrdTypeInfo<OrdType::OnBasis> { static constexpr std::string_view name = "OnBasis"; };
    template<> struct OrdTypeInfo<OrdType::PreviouslyQuoted> { static constexpr std::string_view name = "PreviouslyQuoted"; };
    template<> struct OrdTypeInfo<OrdType::PreviouslyIndicated> { static constexpr std::string_view name = "PreviouslyIndicated"; };
    template<> struct OrdTypeInfo<OrdType::Pegged> { static constexpr std::string_view name = "Pegged"; };

    // Sparse table for OrdType (covers '1'-'9', 'D', 'E', 'P')
    consteval std::array<std::string_view, 128> create_ord_type_table() {
        std::array<std::string_view, 128> table{};
        for (auto& e : table) e = "Unknown";
        table['1'] = OrdTypeInfo<OrdType::Market>::name;
        table['2'] = OrdTypeInfo<OrdType::Limit>::name;
        table['3'] = OrdTypeInfo<OrdType::Stop>::name;
        table['4'] = OrdTypeInfo<OrdType::StopLimit>::name;
        table['5'] = OrdTypeInfo<OrdType::MarketOnClose>::name;
        table['6'] = OrdTypeInfo<OrdType::WithOrWithout>::name;
        table['7'] = OrdTypeInfo<OrdType::LimitOrBetter>::name;
        table['8'] = OrdTypeInfo<OrdType::LimitWithOrWithout>::name;
        table['9'] = OrdTypeInfo<OrdType::OnBasis>::name;
        table['D'] = OrdTypeInfo<OrdType::PreviouslyQuoted>::name;
        table['E'] = OrdTypeInfo<OrdType::PreviouslyIndicated>::name;
        table['P'] = OrdTypeInfo<OrdType::Pegged>::name;
        return table;
    }
    inline constexpr auto ORD_TYPE_TABLE = create_ord_type_table();
}

template<OrdType T>
[[nodiscard]] consteval std::string_view ord_type_name() noexcept {
    return detail::OrdTypeInfo<T>::name;
}

[[nodiscard]] inline constexpr std::string_view ord_type_name(OrdType t) noexcept {
    const auto idx = static_cast<unsigned char>(static_cast<char>(t));
    if (idx < 128) [[likely]] {
        return detail::ORD_TYPE_TABLE[idx];
    }
    return "Unknown";
}

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

// Compile-time OrdStatus lookup (TICKET_023)
namespace detail {
    template<OrdStatus S> struct OrdStatusInfo { static constexpr std::string_view name = "Unknown"; };
    template<> struct OrdStatusInfo<OrdStatus::New> { static constexpr std::string_view name = "New"; };
    template<> struct OrdStatusInfo<OrdStatus::PartiallyFilled> { static constexpr std::string_view name = "PartiallyFilled"; };
    template<> struct OrdStatusInfo<OrdStatus::Filled> { static constexpr std::string_view name = "Filled"; };
    template<> struct OrdStatusInfo<OrdStatus::DoneForDay> { static constexpr std::string_view name = "DoneForDay"; };
    template<> struct OrdStatusInfo<OrdStatus::Canceled> { static constexpr std::string_view name = "Canceled"; };
    template<> struct OrdStatusInfo<OrdStatus::Replaced> { static constexpr std::string_view name = "Replaced"; };
    template<> struct OrdStatusInfo<OrdStatus::PendingCancel> { static constexpr std::string_view name = "PendingCancel"; };
    template<> struct OrdStatusInfo<OrdStatus::Stopped> { static constexpr std::string_view name = "Stopped"; };
    template<> struct OrdStatusInfo<OrdStatus::Rejected> { static constexpr std::string_view name = "Rejected"; };
    template<> struct OrdStatusInfo<OrdStatus::Suspended> { static constexpr std::string_view name = "Suspended"; };
    template<> struct OrdStatusInfo<OrdStatus::PendingNew> { static constexpr std::string_view name = "PendingNew"; };
    template<> struct OrdStatusInfo<OrdStatus::Calculated> { static constexpr std::string_view name = "Calculated"; };
    template<> struct OrdStatusInfo<OrdStatus::Expired> { static constexpr std::string_view name = "Expired"; };
    template<> struct OrdStatusInfo<OrdStatus::AcceptedForBidding> { static constexpr std::string_view name = "AcceptedForBidding"; };
    template<> struct OrdStatusInfo<OrdStatus::PendingReplace> { static constexpr std::string_view name = "PendingReplace"; };

    consteval std::array<std::string_view, 128> create_ord_status_table() {
        std::array<std::string_view, 128> table{};
        for (auto& e : table) e = "Unknown";
        table['0'] = OrdStatusInfo<OrdStatus::New>::name;
        table['1'] = OrdStatusInfo<OrdStatus::PartiallyFilled>::name;
        table['2'] = OrdStatusInfo<OrdStatus::Filled>::name;
        table['3'] = OrdStatusInfo<OrdStatus::DoneForDay>::name;
        table['4'] = OrdStatusInfo<OrdStatus::Canceled>::name;
        table['5'] = OrdStatusInfo<OrdStatus::Replaced>::name;
        table['6'] = OrdStatusInfo<OrdStatus::PendingCancel>::name;
        table['7'] = OrdStatusInfo<OrdStatus::Stopped>::name;
        table['8'] = OrdStatusInfo<OrdStatus::Rejected>::name;
        table['9'] = OrdStatusInfo<OrdStatus::Suspended>::name;
        table['A'] = OrdStatusInfo<OrdStatus::PendingNew>::name;
        table['B'] = OrdStatusInfo<OrdStatus::Calculated>::name;
        table['C'] = OrdStatusInfo<OrdStatus::Expired>::name;
        table['D'] = OrdStatusInfo<OrdStatus::AcceptedForBidding>::name;
        table['E'] = OrdStatusInfo<OrdStatus::PendingReplace>::name;
        return table;
    }
    inline constexpr auto ORD_STATUS_TABLE = create_ord_status_table();
}

template<OrdStatus S>
[[nodiscard]] consteval std::string_view ord_status_name() noexcept {
    return detail::OrdStatusInfo<S>::name;
}

[[nodiscard]] inline constexpr std::string_view ord_status_name(OrdStatus s) noexcept {
    const auto idx = static_cast<unsigned char>(static_cast<char>(s));
    if (idx < 128) [[likely]] {
        return detail::ORD_STATUS_TABLE[idx];
    }
    return "Unknown";
}

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

// Compile-time ExecType lookup (TICKET_023)
namespace detail {
    template<ExecType E> struct ExecTypeInfo { static constexpr std::string_view name = "Unknown"; };
    template<> struct ExecTypeInfo<ExecType::New> { static constexpr std::string_view name = "New"; };
    template<> struct ExecTypeInfo<ExecType::PartialFill> { static constexpr std::string_view name = "PartialFill"; };
    template<> struct ExecTypeInfo<ExecType::Fill> { static constexpr std::string_view name = "Fill"; };
    template<> struct ExecTypeInfo<ExecType::DoneForDay> { static constexpr std::string_view name = "DoneForDay"; };
    template<> struct ExecTypeInfo<ExecType::Canceled> { static constexpr std::string_view name = "Canceled"; };
    template<> struct ExecTypeInfo<ExecType::Replaced> { static constexpr std::string_view name = "Replaced"; };
    template<> struct ExecTypeInfo<ExecType::PendingCancel> { static constexpr std::string_view name = "PendingCancel"; };
    template<> struct ExecTypeInfo<ExecType::Stopped> { static constexpr std::string_view name = "Stopped"; };
    template<> struct ExecTypeInfo<ExecType::Rejected> { static constexpr std::string_view name = "Rejected"; };
    template<> struct ExecTypeInfo<ExecType::Suspended> { static constexpr std::string_view name = "Suspended"; };
    template<> struct ExecTypeInfo<ExecType::PendingNew> { static constexpr std::string_view name = "PendingNew"; };
    template<> struct ExecTypeInfo<ExecType::Calculated> { static constexpr std::string_view name = "Calculated"; };
    template<> struct ExecTypeInfo<ExecType::Expired> { static constexpr std::string_view name = "Expired"; };
    template<> struct ExecTypeInfo<ExecType::Restated> { static constexpr std::string_view name = "Restated"; };
    template<> struct ExecTypeInfo<ExecType::PendingReplace> { static constexpr std::string_view name = "PendingReplace"; };
    template<> struct ExecTypeInfo<ExecType::Trade> { static constexpr std::string_view name = "Trade"; };
    template<> struct ExecTypeInfo<ExecType::TradeCorrect> { static constexpr std::string_view name = "TradeCorrect"; };
    template<> struct ExecTypeInfo<ExecType::TradeCancel> { static constexpr std::string_view name = "TradeCancel"; };
    template<> struct ExecTypeInfo<ExecType::OrderStatus> { static constexpr std::string_view name = "OrderStatus"; };

    consteval std::array<std::string_view, 128> create_exec_type_table() {
        std::array<std::string_view, 128> table{};
        for (auto& e : table) e = "Unknown";
        table['0'] = ExecTypeInfo<ExecType::New>::name;
        table['1'] = ExecTypeInfo<ExecType::PartialFill>::name;
        table['2'] = ExecTypeInfo<ExecType::Fill>::name;
        table['3'] = ExecTypeInfo<ExecType::DoneForDay>::name;
        table['4'] = ExecTypeInfo<ExecType::Canceled>::name;
        table['5'] = ExecTypeInfo<ExecType::Replaced>::name;
        table['6'] = ExecTypeInfo<ExecType::PendingCancel>::name;
        table['7'] = ExecTypeInfo<ExecType::Stopped>::name;
        table['8'] = ExecTypeInfo<ExecType::Rejected>::name;
        table['9'] = ExecTypeInfo<ExecType::Suspended>::name;
        table['A'] = ExecTypeInfo<ExecType::PendingNew>::name;
        table['B'] = ExecTypeInfo<ExecType::Calculated>::name;
        table['C'] = ExecTypeInfo<ExecType::Expired>::name;
        table['D'] = ExecTypeInfo<ExecType::Restated>::name;
        table['E'] = ExecTypeInfo<ExecType::PendingReplace>::name;
        table['F'] = ExecTypeInfo<ExecType::Trade>::name;
        table['G'] = ExecTypeInfo<ExecType::TradeCorrect>::name;
        table['H'] = ExecTypeInfo<ExecType::TradeCancel>::name;
        table['I'] = ExecTypeInfo<ExecType::OrderStatus>::name;
        return table;
    }
    inline constexpr auto EXEC_TYPE_TABLE = create_exec_type_table();
}

template<ExecType E>
[[nodiscard]] consteval std::string_view exec_type_name() noexcept {
    return detail::ExecTypeInfo<E>::name;
}

[[nodiscard]] inline constexpr std::string_view exec_type_name(ExecType e) noexcept {
    const auto idx = static_cast<unsigned char>(static_cast<char>(e));
    if (idx < 128) [[likely]] {
        return detail::EXEC_TYPE_TABLE[idx];
    }
    return "Unknown";
}

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

// Compile-time TimeInForce lookup (TICKET_023)
namespace detail {
    template<TimeInForce T> struct TimeInForceInfo { static constexpr std::string_view name = "Unknown"; };
    template<> struct TimeInForceInfo<TimeInForce::Day> { static constexpr std::string_view name = "Day"; };
    template<> struct TimeInForceInfo<TimeInForce::GoodTillCancel> { static constexpr std::string_view name = "GoodTillCancel"; };
    template<> struct TimeInForceInfo<TimeInForce::AtTheOpening> { static constexpr std::string_view name = "AtTheOpening"; };
    template<> struct TimeInForceInfo<TimeInForce::ImmediateOrCancel> { static constexpr std::string_view name = "ImmediateOrCancel"; };
    template<> struct TimeInForceInfo<TimeInForce::FillOrKill> { static constexpr std::string_view name = "FillOrKill"; };
    template<> struct TimeInForceInfo<TimeInForce::GoodTillCrossing> { static constexpr std::string_view name = "GoodTillCrossing"; };
    template<> struct TimeInForceInfo<TimeInForce::GoodTillDate> { static constexpr std::string_view name = "GoodTillDate"; };
    template<> struct TimeInForceInfo<TimeInForce::AtTheClose> { static constexpr std::string_view name = "AtTheClose"; };

    consteval std::array<std::string_view, 8> create_time_in_force_table() {
        std::array<std::string_view, 8> table{};
        table[0] = TimeInForceInfo<TimeInForce::Day>::name;
        table[1] = TimeInForceInfo<TimeInForce::GoodTillCancel>::name;
        table[2] = TimeInForceInfo<TimeInForce::AtTheOpening>::name;
        table[3] = TimeInForceInfo<TimeInForce::ImmediateOrCancel>::name;
        table[4] = TimeInForceInfo<TimeInForce::FillOrKill>::name;
        table[5] = TimeInForceInfo<TimeInForce::GoodTillCrossing>::name;
        table[6] = TimeInForceInfo<TimeInForce::GoodTillDate>::name;
        table[7] = TimeInForceInfo<TimeInForce::AtTheClose>::name;
        return table;
    }
    inline constexpr auto TIME_IN_FORCE_TABLE = create_time_in_force_table();
}

template<TimeInForce T>
[[nodiscard]] consteval std::string_view time_in_force_name() noexcept {
    return detail::TimeInForceInfo<T>::name;
}

[[nodiscard]] inline constexpr std::string_view time_in_force_name(TimeInForce t) noexcept {
    const auto idx = static_cast<char>(t) - '0';
    if (idx >= 0 && idx < 8) [[likely]] {
        return detail::TIME_IN_FORCE_TABLE[idx];
    }
    return "Unknown";
}

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
