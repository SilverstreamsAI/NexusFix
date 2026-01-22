#pragma once

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

[[nodiscard]] constexpr std::string_view md_entry_type_name(MDEntryType t) noexcept {
    switch (t) {
        case MDEntryType::Bid: return "Bid";
        case MDEntryType::Offer: return "Offer";
        case MDEntryType::Trade: return "Trade";
        case MDEntryType::IndexValue: return "IndexValue";
        case MDEntryType::OpeningPrice: return "OpeningPrice";
        case MDEntryType::ClosingPrice: return "ClosingPrice";
        case MDEntryType::SettlementPrice: return "SettlementPrice";
        case MDEntryType::TradingSessionHighPrice: return "SessionHigh";
        case MDEntryType::TradingSessionLowPrice: return "SessionLow";
        case MDEntryType::TradingSessionVWAPPrice: return "VWAP";
        case MDEntryType::Imbalance: return "Imbalance";
        case MDEntryType::TradeVolume: return "TradeVolume";
        case MDEntryType::OpenInterest: return "OpenInterest";
        default: return "Unknown";
    }
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

[[nodiscard]] constexpr std::string_view md_update_action_name(MDUpdateAction a) noexcept {
    switch (a) {
        case MDUpdateAction::New: return "New";
        case MDUpdateAction::Change: return "Change";
        case MDUpdateAction::Delete: return "Delete";
        case MDUpdateAction::DeleteThru: return "DeleteThru";
        case MDUpdateAction::DeleteFrom: return "DeleteFrom";
        default: return "Unknown";
    }
}

// ============================================================================
// Subscription Request Type (Tag 263)
// ============================================================================

enum class SubscriptionRequestType : char {
    Snapshot              = '0',
    SnapshotPlusUpdates   = '1',
    DisablePreviousSnapshot = '2'
};

[[nodiscard]] constexpr std::string_view subscription_type_name(SubscriptionRequestType t) noexcept {
    switch (t) {
        case SubscriptionRequestType::Snapshot: return "Snapshot";
        case SubscriptionRequestType::SnapshotPlusUpdates: return "Subscribe";
        case SubscriptionRequestType::DisablePreviousSnapshot: return "Unsubscribe";
        default: return "Unknown";
    }
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

[[nodiscard]] constexpr std::string_view md_rej_reason_name(MDReqRejReason r) noexcept {
    switch (r) {
        case MDReqRejReason::UnknownSymbol: return "UnknownSymbol";
        case MDReqRejReason::DuplicateMDReqID: return "DuplicateMDReqID";
        case MDReqRejReason::InsufficientPermissions: return "InsufficientPermissions";
        case MDReqRejReason::UnsupportedSubscriptionType: return "UnsupportedSubscriptionType";
        case MDReqRejReason::UnsupportedMarketDepth: return "UnsupportedMarketDepth";
        case MDReqRejReason::UnsupportedMDUpdateType: return "UnsupportedMDUpdateType";
        case MDReqRejReason::UnsupportedAggregatedBook: return "UnsupportedAggregatedBook";
        case MDReqRejReason::UnsupportedMDEntryType: return "UnsupportedMDEntryType";
        case MDReqRejReason::UnsupportedTradingSessionID: return "UnsupportedTradingSessionID";
        case MDReqRejReason::UnsupportedScope: return "UnsupportedScope";
        case MDReqRejReason::UnsupportedOpenCloseSettleFlag: return "UnsupportedOpenCloseSettleFlag";
        case MDReqRejReason::UnsupportedMDImplicitDelete: return "UnsupportedMDImplicitDelete";
        case MDReqRejReason::InsufficientCredit: return "InsufficientCredit";
        case MDReqRejReason::Other: return "Other";
        default: return "Unknown";
    }
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
