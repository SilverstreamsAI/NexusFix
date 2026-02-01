#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace nfx {

// ============================================================================
// Market Data Entry Type (Tag 269)
// ============================================================================

enum class MDEntryType : char {
    Bid                     = '0',
    Offer                   = '1',
    Trade                   = '2',
    IndexValue              = '3',
    OpeningPrice            = '4',
    ClosingPrice            = '5',
    SettlementPrice         = '6',
    TradingSessionHighPrice = '7',
    TradingSessionLowPrice  = '8',
    TradingSessionVWAPPrice = '9',
    Imbalance               = 'A',
    TradeVolume             = 'B',
    OpenInterest            = 'C'
};

// Compile-time MDEntryType metadata
namespace detail {

template<MDEntryType T>
struct MDEntryTypeInfo {
    static constexpr std::string_view name = "Unknown";
};

template<> struct MDEntryTypeInfo<MDEntryType::Bid> {
    static constexpr std::string_view name = "Bid";
};
template<> struct MDEntryTypeInfo<MDEntryType::Offer> {
    static constexpr std::string_view name = "Offer";
};
template<> struct MDEntryTypeInfo<MDEntryType::Trade> {
    static constexpr std::string_view name = "Trade";
};
template<> struct MDEntryTypeInfo<MDEntryType::IndexValue> {
    static constexpr std::string_view name = "IndexValue";
};
template<> struct MDEntryTypeInfo<MDEntryType::OpeningPrice> {
    static constexpr std::string_view name = "OpeningPrice";
};
template<> struct MDEntryTypeInfo<MDEntryType::ClosingPrice> {
    static constexpr std::string_view name = "ClosingPrice";
};
template<> struct MDEntryTypeInfo<MDEntryType::SettlementPrice> {
    static constexpr std::string_view name = "SettlementPrice";
};
template<> struct MDEntryTypeInfo<MDEntryType::TradingSessionHighPrice> {
    static constexpr std::string_view name = "SessionHigh";
};
template<> struct MDEntryTypeInfo<MDEntryType::TradingSessionLowPrice> {
    static constexpr std::string_view name = "SessionLow";
};
template<> struct MDEntryTypeInfo<MDEntryType::TradingSessionVWAPPrice> {
    static constexpr std::string_view name = "VWAP";
};
template<> struct MDEntryTypeInfo<MDEntryType::Imbalance> {
    static constexpr std::string_view name = "Imbalance";
};
template<> struct MDEntryTypeInfo<MDEntryType::TradeVolume> {
    static constexpr std::string_view name = "TradeVolume";
};
template<> struct MDEntryTypeInfo<MDEntryType::OpenInterest> {
    static constexpr std::string_view name = "OpenInterest";
};

// Lookup table: values span '0'-'9' (48-57) and 'A'-'C' (65-67)
// Use 128 entries for O(1) lookup
consteval std::array<std::string_view, 128> create_md_entry_type_table() {
    std::array<std::string_view, 128> table{};
    for (auto& e : table) e = "Unknown";
    table['0'] = MDEntryTypeInfo<MDEntryType::Bid>::name;
    table['1'] = MDEntryTypeInfo<MDEntryType::Offer>::name;
    table['2'] = MDEntryTypeInfo<MDEntryType::Trade>::name;
    table['3'] = MDEntryTypeInfo<MDEntryType::IndexValue>::name;
    table['4'] = MDEntryTypeInfo<MDEntryType::OpeningPrice>::name;
    table['5'] = MDEntryTypeInfo<MDEntryType::ClosingPrice>::name;
    table['6'] = MDEntryTypeInfo<MDEntryType::SettlementPrice>::name;
    table['7'] = MDEntryTypeInfo<MDEntryType::TradingSessionHighPrice>::name;
    table['8'] = MDEntryTypeInfo<MDEntryType::TradingSessionLowPrice>::name;
    table['9'] = MDEntryTypeInfo<MDEntryType::TradingSessionVWAPPrice>::name;
    table['A'] = MDEntryTypeInfo<MDEntryType::Imbalance>::name;
    table['B'] = MDEntryTypeInfo<MDEntryType::TradeVolume>::name;
    table['C'] = MDEntryTypeInfo<MDEntryType::OpenInterest>::name;
    return table;
}

inline constexpr auto MD_ENTRY_TYPE_TABLE = create_md_entry_type_table();

} // namespace detail

// Compile-time lookup
template<MDEntryType T>
[[nodiscard]] consteval std::string_view md_entry_type_name() noexcept {
    return detail::MDEntryTypeInfo<T>::name;
}

// Runtime O(1) lookup
[[nodiscard]] inline constexpr std::string_view md_entry_type_name(MDEntryType t) noexcept {
    const auto idx = static_cast<unsigned char>(static_cast<char>(t));
    if (idx < 128) [[likely]] {
        return detail::MD_ENTRY_TYPE_TABLE[idx];
    }
    return "Unknown";
}

[[nodiscard]] constexpr bool is_quote_type(MDEntryType t) noexcept {
    return t == MDEntryType::Bid || t == MDEntryType::Offer;
}

[[nodiscard]] constexpr bool is_trade_type(MDEntryType t) noexcept {
    return t == MDEntryType::Trade || t == MDEntryType::TradeVolume;
}

// ============================================================================
// Market Data Update Action (Tag 279)
// ============================================================================

enum class MDUpdateAction : char {
    New        = '0',
    Change     = '1',
    Delete     = '2',
    DeleteThru = '3',
    DeleteFrom = '4'
};

// Compile-time MDUpdateAction metadata
namespace detail {

template<MDUpdateAction A>
struct MDUpdateActionInfo {
    static constexpr std::string_view name = "Unknown";
};

template<> struct MDUpdateActionInfo<MDUpdateAction::New> {
    static constexpr std::string_view name = "New";
};
template<> struct MDUpdateActionInfo<MDUpdateAction::Change> {
    static constexpr std::string_view name = "Change";
};
template<> struct MDUpdateActionInfo<MDUpdateAction::Delete> {
    static constexpr std::string_view name = "Delete";
};
template<> struct MDUpdateActionInfo<MDUpdateAction::DeleteThru> {
    static constexpr std::string_view name = "DeleteThru";
};
template<> struct MDUpdateActionInfo<MDUpdateAction::DeleteFrom> {
    static constexpr std::string_view name = "DeleteFrom";
};

// Compact lookup table: values '0'-'4' (5 entries)
consteval std::array<std::string_view, 5> create_md_update_action_table() {
    std::array<std::string_view, 5> table{};
    table[0] = MDUpdateActionInfo<MDUpdateAction::New>::name;
    table[1] = MDUpdateActionInfo<MDUpdateAction::Change>::name;
    table[2] = MDUpdateActionInfo<MDUpdateAction::Delete>::name;
    table[3] = MDUpdateActionInfo<MDUpdateAction::DeleteThru>::name;
    table[4] = MDUpdateActionInfo<MDUpdateAction::DeleteFrom>::name;
    return table;
}

inline constexpr auto MD_UPDATE_ACTION_TABLE = create_md_update_action_table();

} // namespace detail

// Compile-time lookup
template<MDUpdateAction A>
[[nodiscard]] consteval std::string_view md_update_action_name() noexcept {
    return detail::MDUpdateActionInfo<A>::name;
}

// Runtime O(1) lookup
[[nodiscard]] inline constexpr std::string_view md_update_action_name(MDUpdateAction a) noexcept {
    const auto idx = static_cast<char>(a) - '0';
    if (idx >= 0 && idx < 5) [[likely]] {
        return detail::MD_UPDATE_ACTION_TABLE[static_cast<size_t>(idx)];
    }
    return "Unknown";
}

// ============================================================================
// Subscription Request Type (Tag 263)
// ============================================================================

enum class SubscriptionRequestType : char {
    Snapshot              = '0',
    SnapshotPlusUpdates   = '1',
    DisablePreviousSnapshot = '2'
};

// Compile-time SubscriptionRequestType metadata
namespace detail {

template<SubscriptionRequestType T>
struct SubscriptionRequestTypeInfo {
    static constexpr std::string_view name = "Unknown";
};

template<> struct SubscriptionRequestTypeInfo<SubscriptionRequestType::Snapshot> {
    static constexpr std::string_view name = "Snapshot";
};
template<> struct SubscriptionRequestTypeInfo<SubscriptionRequestType::SnapshotPlusUpdates> {
    static constexpr std::string_view name = "Subscribe";
};
template<> struct SubscriptionRequestTypeInfo<SubscriptionRequestType::DisablePreviousSnapshot> {
    static constexpr std::string_view name = "Unsubscribe";
};

// Compact lookup table: values '0'-'2' (3 entries)
consteval std::array<std::string_view, 3> create_subscription_type_table() {
    std::array<std::string_view, 3> table{};
    table[0] = SubscriptionRequestTypeInfo<SubscriptionRequestType::Snapshot>::name;
    table[1] = SubscriptionRequestTypeInfo<SubscriptionRequestType::SnapshotPlusUpdates>::name;
    table[2] = SubscriptionRequestTypeInfo<SubscriptionRequestType::DisablePreviousSnapshot>::name;
    return table;
}

inline constexpr auto SUBSCRIPTION_TYPE_TABLE = create_subscription_type_table();

} // namespace detail

// Compile-time lookup
template<SubscriptionRequestType T>
[[nodiscard]] consteval std::string_view subscription_type_name() noexcept {
    return detail::SubscriptionRequestTypeInfo<T>::name;
}

// Runtime O(1) lookup
[[nodiscard]] inline constexpr std::string_view subscription_type_name(SubscriptionRequestType t) noexcept {
    const auto idx = static_cast<char>(t) - '0';
    if (idx >= 0 && idx < 3) [[likely]] {
        return detail::SUBSCRIPTION_TYPE_TABLE[static_cast<size_t>(idx)];
    }
    return "Unknown";
}

// ============================================================================
// Market Data Update Type (Tag 265)
// ============================================================================

enum class MDUpdateType : int {
    FullRefresh       = 0,
    IncrementalRefresh = 1
};

// ============================================================================
// Market Data Request Reject Reason (Tag 281)
// ============================================================================

enum class MDReqRejReason : char {
    UnknownSymbol                 = '0',
    DuplicateMDReqID              = '1',
    InsufficientPermissions       = '2',
    UnsupportedSubscriptionType   = '3',
    UnsupportedMarketDepth        = '4',
    UnsupportedMDUpdateType       = '5',
    UnsupportedAggregatedBook     = '6',
    UnsupportedMDEntryType        = '7',
    UnsupportedTradingSessionID   = '8',
    UnsupportedScope              = '9',
    UnsupportedOpenCloseSettleFlag = 'A',
    UnsupportedMDImplicitDelete   = 'B',
    InsufficientCredit            = 'C',
    Other                         = 'D'
};

// Compile-time MDReqRejReason metadata
namespace detail {

template<MDReqRejReason R>
struct MDReqRejReasonInfo {
    static constexpr std::string_view name = "Unknown";
};

template<> struct MDReqRejReasonInfo<MDReqRejReason::UnknownSymbol> {
    static constexpr std::string_view name = "UnknownSymbol";
};
template<> struct MDReqRejReasonInfo<MDReqRejReason::DuplicateMDReqID> {
    static constexpr std::string_view name = "DuplicateMDReqID";
};
template<> struct MDReqRejReasonInfo<MDReqRejReason::InsufficientPermissions> {
    static constexpr std::string_view name = "InsufficientPermissions";
};
template<> struct MDReqRejReasonInfo<MDReqRejReason::UnsupportedSubscriptionType> {
    static constexpr std::string_view name = "UnsupportedSubscriptionType";
};
template<> struct MDReqRejReasonInfo<MDReqRejReason::UnsupportedMarketDepth> {
    static constexpr std::string_view name = "UnsupportedMarketDepth";
};
template<> struct MDReqRejReasonInfo<MDReqRejReason::UnsupportedMDUpdateType> {
    static constexpr std::string_view name = "UnsupportedMDUpdateType";
};
template<> struct MDReqRejReasonInfo<MDReqRejReason::UnsupportedAggregatedBook> {
    static constexpr std::string_view name = "UnsupportedAggregatedBook";
};
template<> struct MDReqRejReasonInfo<MDReqRejReason::UnsupportedMDEntryType> {
    static constexpr std::string_view name = "UnsupportedMDEntryType";
};
template<> struct MDReqRejReasonInfo<MDReqRejReason::UnsupportedTradingSessionID> {
    static constexpr std::string_view name = "UnsupportedTradingSessionID";
};
template<> struct MDReqRejReasonInfo<MDReqRejReason::UnsupportedScope> {
    static constexpr std::string_view name = "UnsupportedScope";
};
template<> struct MDReqRejReasonInfo<MDReqRejReason::UnsupportedOpenCloseSettleFlag> {
    static constexpr std::string_view name = "UnsupportedOpenCloseSettleFlag";
};
template<> struct MDReqRejReasonInfo<MDReqRejReason::UnsupportedMDImplicitDelete> {
    static constexpr std::string_view name = "UnsupportedMDImplicitDelete";
};
template<> struct MDReqRejReasonInfo<MDReqRejReason::InsufficientCredit> {
    static constexpr std::string_view name = "InsufficientCredit";
};
template<> struct MDReqRejReasonInfo<MDReqRejReason::Other> {
    static constexpr std::string_view name = "Other";
};

// Lookup table: values span '0'-'9' (48-57) and 'A'-'D' (65-68)
// Use 128 entries for O(1) lookup
consteval std::array<std::string_view, 128> create_md_rej_reason_table() {
    std::array<std::string_view, 128> table{};
    for (auto& e : table) e = "Unknown";
    table['0'] = MDReqRejReasonInfo<MDReqRejReason::UnknownSymbol>::name;
    table['1'] = MDReqRejReasonInfo<MDReqRejReason::DuplicateMDReqID>::name;
    table['2'] = MDReqRejReasonInfo<MDReqRejReason::InsufficientPermissions>::name;
    table['3'] = MDReqRejReasonInfo<MDReqRejReason::UnsupportedSubscriptionType>::name;
    table['4'] = MDReqRejReasonInfo<MDReqRejReason::UnsupportedMarketDepth>::name;
    table['5'] = MDReqRejReasonInfo<MDReqRejReason::UnsupportedMDUpdateType>::name;
    table['6'] = MDReqRejReasonInfo<MDReqRejReason::UnsupportedAggregatedBook>::name;
    table['7'] = MDReqRejReasonInfo<MDReqRejReason::UnsupportedMDEntryType>::name;
    table['8'] = MDReqRejReasonInfo<MDReqRejReason::UnsupportedTradingSessionID>::name;
    table['9'] = MDReqRejReasonInfo<MDReqRejReason::UnsupportedScope>::name;
    table['A'] = MDReqRejReasonInfo<MDReqRejReason::UnsupportedOpenCloseSettleFlag>::name;
    table['B'] = MDReqRejReasonInfo<MDReqRejReason::UnsupportedMDImplicitDelete>::name;
    table['C'] = MDReqRejReasonInfo<MDReqRejReason::InsufficientCredit>::name;
    table['D'] = MDReqRejReasonInfo<MDReqRejReason::Other>::name;
    return table;
}

inline constexpr auto MD_REJ_REASON_TABLE = create_md_rej_reason_table();

} // namespace detail

// Compile-time lookup
template<MDReqRejReason R>
[[nodiscard]] consteval std::string_view md_rej_reason_name() noexcept {
    return detail::MDReqRejReasonInfo<R>::name;
}

// Runtime O(1) lookup
[[nodiscard]] inline constexpr std::string_view md_rej_reason_name(MDReqRejReason r) noexcept {
    const auto idx = static_cast<unsigned char>(static_cast<char>(r));
    if (idx < 128) [[likely]] {
        return detail::MD_REJ_REASON_TABLE[idx];
    }
    return "Unknown";
}

// ============================================================================
// Market Data Entry (parsed from repeating group)
// ============================================================================

struct MDEntry {
    MDEntryType entry_type{MDEntryType::Bid};
    int64_t price_raw{0};           // FixedPrice raw value
    int64_t size_raw{0};            // Qty raw value
    MDUpdateAction update_action{MDUpdateAction::New};
    std::string_view entry_id{};
    std::string_view symbol{};
    std::string_view entry_date{};
    std::string_view entry_time{};
    int position_no{0};             // Price level (1 = best)
    int number_of_orders{0};

    [[nodiscard]] constexpr bool is_bid() const noexcept {
        return entry_type == MDEntryType::Bid;
    }

    [[nodiscard]] constexpr bool is_offer() const noexcept {
        return entry_type == MDEntryType::Offer;
    }

    [[nodiscard]] constexpr bool is_trade() const noexcept {
        return entry_type == MDEntryType::Trade;
    }

    [[nodiscard]] constexpr bool has_price() const noexcept {
        return price_raw != 0;
    }

    [[nodiscard]] constexpr bool has_size() const noexcept {
        return size_raw != 0;
    }
};

// ============================================================================
// Related Symbol Entry (for subscription requests)
// ============================================================================

struct RelatedSymbol {
    std::string_view symbol{};
    std::string_view security_id{};
    std::string_view security_exchange{};
};

// ============================================================================
// Static Assertions
// ============================================================================

static_assert(sizeof(MDEntryType) == 1, "MDEntryType should be 1 byte");
static_assert(sizeof(MDUpdateAction) == 1, "MDUpdateAction should be 1 byte");
static_assert(sizeof(SubscriptionRequestType) == 1, "SubscriptionRequestType should be 1 byte");
static_assert(sizeof(MDReqRejReason) == 1, "MDReqRejReason should be 1 byte");

} // namespace nfx
