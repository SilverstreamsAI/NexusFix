#pragma once

#include <array>
#include <cstdint>
#include <concepts>
#include <string_view>
#include <type_traits>

namespace nfx::tag {

/// Compile-time FIX tag wrapper using NTTP
template <int N>
struct Tag {
    static constexpr int value = N;

    consteval operator int() const noexcept { return N; }
};

/// Concept to identify Tag types
template <typename T>
concept IsTag = requires {
    { T::value } -> std::convertible_to<int>;
};

// ============================================================================
// FIX 4.4 Standard Header Tags
// ============================================================================

using BeginString   = Tag<8>;    // FIX.4.4
using BodyLength    = Tag<9>;    // Message body length
using MsgType       = Tag<35>;   // Message type
using SenderCompID  = Tag<49>;   // Sender identifier
using TargetCompID  = Tag<56>;   // Target identifier
using MsgSeqNum     = Tag<34>;   // Message sequence number
using SendingTime   = Tag<52>;   // Time of message transmission
using PossDupFlag   = Tag<43>;   // Possible duplicate
using PossResend    = Tag<97>;   // Possible resend
using OrigSendingTime = Tag<122>; // Original sending time

// ============================================================================
// FIX 4.4 Standard Trailer Tags
// ============================================================================

using CheckSum = Tag<10>;  // Three-byte checksum

// ============================================================================
// Session-level Tags
// ============================================================================

using EncryptMethod    = Tag<98>;   // Encryption method
using HeartBtInt       = Tag<108>;  // Heartbeat interval
using ResetSeqNumFlag  = Tag<141>;  // Reset sequence numbers
using TestReqID        = Tag<112>;  // Test request ID
using RefSeqNum        = Tag<45>;   // Reference sequence number
using Text             = Tag<58>;   // Free format text

// ============================================================================
// Order Tags (NewOrderSingle 35=D)
// ============================================================================

using ClOrdID          = Tag<11>;   // Client order ID
using Symbol           = Tag<55>;   // Instrument symbol
using Side             = Tag<54>;   // Buy/Sell
using OrderQty         = Tag<38>;   // Order quantity
using OrdType          = Tag<40>;   // Order type (Market, Limit, etc.)
using Price            = Tag<44>;   // Limit price
using StopPx           = Tag<99>;   // Stop price
using TimeInForce      = Tag<59>;   // Order validity
using TransactTime     = Tag<60>;   // Transaction time
using Account          = Tag<1>;    // Account ID
using HandlInst        = Tag<21>;   // Handling instructions
using ExDestination    = Tag<100>;  // Exchange destination
using SecurityType     = Tag<167>;  // Security type
using MaturityMonthYear = Tag<200>; // Maturity month-year
using SecurityExchange = Tag<207>;  // Security exchange

// ============================================================================
// Execution Report Tags (35=8)
// ============================================================================

using OrderID          = Tag<37>;   // Order ID
using ExecID           = Tag<17>;   // Execution ID
using ExecType         = Tag<150>;  // Execution type
using OrdStatus        = Tag<39>;   // Order status
using LeavesQty        = Tag<151>;  // Remaining quantity
using CumQty           = Tag<14>;   // Cumulative filled quantity
using AvgPx            = Tag<6>;    // Average fill price
using LastPx           = Tag<31>;   // Last fill price
using LastQty          = Tag<32>;   // Last fill quantity
using OrdRejReason     = Tag<103>;  // Order reject reason
using ExecRestatementReason = Tag<378>; // Exec restatement reason

// ============================================================================
// Order Cancel Tags (35=F, 35=G)
// ============================================================================

using OrigClOrdID      = Tag<41>;   // Original client order ID
using CxlRejReason     = Tag<102>;  // Cancel reject reason
using CxlRejResponseTo = Tag<434>;  // Cancel reject response to

// ============================================================================
// Market Data Tags
// ============================================================================

using MDReqID          = Tag<262>;  // Market data request ID
using SubscriptionRequestType = Tag<263>; // Subscription type
using MarketDepth      = Tag<264>;  // Market depth
using MDUpdateType     = Tag<265>;  // Update type
using AggregatedBook   = Tag<266>;  // Aggregated book flag
using NoMDEntryTypes   = Tag<267>;  // Number of MD entry types
using NoMDEntries      = Tag<268>;  // Number of MD entries
using MDEntryType      = Tag<269>;  // Entry type (Bid, Offer, Trade)
using MDEntryPx        = Tag<270>;  // Entry price
using MDEntrySize      = Tag<271>;  // Entry size
using MDEntryDate      = Tag<272>;  // Entry date
using MDEntryTime      = Tag<273>;  // Entry time
using MDUpdateAction   = Tag<279>;  // Update action (New/Change/Delete)
using MDEntryID        = Tag<278>;  // Entry identifier
using MDReqRejReason   = Tag<281>;  // Request rejection reason
using MDEntryPositionNo = Tag<290>; // Price level position
using NoRelatedSym     = Tag<146>;  // Number of related symbols
using SecurityID       = Tag<48>;   // Security identifier
using TradingSessionID = Tag<336>;  // Trading session ID
using QuoteCondition   = Tag<276>;  // Quote condition
using TradeCondition   = Tag<277>;  // Trade condition
using NumberOfOrders   = Tag<346>;  // Number of orders at price level
using TotalVolumeTraded = Tag<387>; // Total volume traded

// ============================================================================
// Compile-time Tag Metadata (TICKET_023)
// ============================================================================

namespace detail {

/// Tag metadata entry
struct TagEntry {
    std::string_view name;
    bool is_header;
    bool is_required;
};

/// Default tag info template
template<int TagNum>
struct TagInfo {
    static constexpr std::string_view name = "";
    static constexpr bool is_header = false;
    static constexpr bool is_required = false;
};

// Header tags
template<> struct TagInfo<8> {
    static constexpr std::string_view name = "BeginString";
    static constexpr bool is_header = true;
    static constexpr bool is_required = true;
};

template<> struct TagInfo<9> {
    static constexpr std::string_view name = "BodyLength";
    static constexpr bool is_header = true;
    static constexpr bool is_required = true;
};

template<> struct TagInfo<35> {
    static constexpr std::string_view name = "MsgType";
    static constexpr bool is_header = true;
    static constexpr bool is_required = true;
};

template<> struct TagInfo<49> {
    static constexpr std::string_view name = "SenderCompID";
    static constexpr bool is_header = true;
    static constexpr bool is_required = true;
};

template<> struct TagInfo<56> {
    static constexpr std::string_view name = "TargetCompID";
    static constexpr bool is_header = true;
    static constexpr bool is_required = true;
};

template<> struct TagInfo<34> {
    static constexpr std::string_view name = "MsgSeqNum";
    static constexpr bool is_header = true;
    static constexpr bool is_required = true;
};

template<> struct TagInfo<52> {
    static constexpr std::string_view name = "SendingTime";
    static constexpr bool is_header = true;
    static constexpr bool is_required = true;
};

template<> struct TagInfo<43> {
    static constexpr std::string_view name = "PossDupFlag";
    static constexpr bool is_header = true;
    static constexpr bool is_required = false;
};

template<> struct TagInfo<97> {
    static constexpr std::string_view name = "PossResend";
    static constexpr bool is_header = true;
    static constexpr bool is_required = false;
};

template<> struct TagInfo<122> {
    static constexpr std::string_view name = "OrigSendingTime";
    static constexpr bool is_header = true;
    static constexpr bool is_required = false;
};

// Trailer tag
template<> struct TagInfo<10> {
    static constexpr std::string_view name = "CheckSum";
    static constexpr bool is_header = false;
    static constexpr bool is_required = true;
};

// Session tags
template<> struct TagInfo<98> {
    static constexpr std::string_view name = "EncryptMethod";
    static constexpr bool is_header = false;
    static constexpr bool is_required = false;
};

template<> struct TagInfo<108> {
    static constexpr std::string_view name = "HeartBtInt";
    static constexpr bool is_header = false;
    static constexpr bool is_required = false;
};

template<> struct TagInfo<141> {
    static constexpr std::string_view name = "ResetSeqNumFlag";
    static constexpr bool is_header = false;
    static constexpr bool is_required = false;
};

template<> struct TagInfo<112> {
    static constexpr std::string_view name = "TestReqID";
    static constexpr bool is_header = false;
    static constexpr bool is_required = false;
};

template<> struct TagInfo<45> {
    static constexpr std::string_view name = "RefSeqNum";
    static constexpr bool is_header = false;
    static constexpr bool is_required = false;
};

template<> struct TagInfo<58> {
    static constexpr std::string_view name = "Text";
    static constexpr bool is_header = false;
    static constexpr bool is_required = false;
};

// Order tags
template<> struct TagInfo<11> {
    static constexpr std::string_view name = "ClOrdID";
    static constexpr bool is_header = false;
    static constexpr bool is_required = false;
};

template<> struct TagInfo<55> {
    static constexpr std::string_view name = "Symbol";
    static constexpr bool is_header = false;
    static constexpr bool is_required = false;
};

template<> struct TagInfo<54> {
    static constexpr std::string_view name = "Side";
    static constexpr bool is_header = false;
    static constexpr bool is_required = false;
};

template<> struct TagInfo<38> {
    static constexpr std::string_view name = "OrderQty";
    static constexpr bool is_header = false;
    static constexpr bool is_required = false;
};

template<> struct TagInfo<40> {
    static constexpr std::string_view name = "OrdType";
    static constexpr bool is_header = false;
    static constexpr bool is_required = false;
};

template<> struct TagInfo<44> {
    static constexpr std::string_view name = "Price";
    static constexpr bool is_header = false;
    static constexpr bool is_required = false;
};

template<> struct TagInfo<99> {
    static constexpr std::string_view name = "StopPx";
    static constexpr bool is_header = false;
    static constexpr bool is_required = false;
};

template<> struct TagInfo<59> {
    static constexpr std::string_view name = "TimeInForce";
    static constexpr bool is_header = false;
    static constexpr bool is_required = false;
};

template<> struct TagInfo<60> {
    static constexpr std::string_view name = "TransactTime";
    static constexpr bool is_header = false;
    static constexpr bool is_required = false;
};

template<> struct TagInfo<1> {
    static constexpr std::string_view name = "Account";
    static constexpr bool is_header = false;
    static constexpr bool is_required = false;
};

template<> struct TagInfo<21> {
    static constexpr std::string_view name = "HandlInst";
    static constexpr bool is_header = false;
    static constexpr bool is_required = false;
};

// Execution report tags
template<> struct TagInfo<37> {
    static constexpr std::string_view name = "OrderID";
    static constexpr bool is_header = false;
    static constexpr bool is_required = false;
};

template<> struct TagInfo<17> {
    static constexpr std::string_view name = "ExecID";
    static constexpr bool is_header = false;
    static constexpr bool is_required = false;
};

template<> struct TagInfo<150> {
    static constexpr std::string_view name = "ExecType";
    static constexpr bool is_header = false;
    static constexpr bool is_required = false;
};

template<> struct TagInfo<39> {
    static constexpr std::string_view name = "OrdStatus";
    static constexpr bool is_header = false;
    static constexpr bool is_required = false;
};

template<> struct TagInfo<151> {
    static constexpr std::string_view name = "LeavesQty";
    static constexpr bool is_header = false;
    static constexpr bool is_required = false;
};

template<> struct TagInfo<14> {
    static constexpr std::string_view name = "CumQty";
    static constexpr bool is_header = false;
    static constexpr bool is_required = false;
};

template<> struct TagInfo<6> {
    static constexpr std::string_view name = "AvgPx";
    static constexpr bool is_header = false;
    static constexpr bool is_required = false;
};

template<> struct TagInfo<31> {
    static constexpr std::string_view name = "LastPx";
    static constexpr bool is_header = false;
    static constexpr bool is_required = false;
};

template<> struct TagInfo<32> {
    static constexpr std::string_view name = "LastQty";
    static constexpr bool is_header = false;
    static constexpr bool is_required = false;
};

template<> struct TagInfo<41> {
    static constexpr std::string_view name = "OrigClOrdID";
    static constexpr bool is_header = false;
    static constexpr bool is_required = false;
};

// Maximum tag number for sparse lookup table (covers common tags)
inline constexpr int MAX_COMMON_TAG = 200;

/// Generate sparse lookup table for common tags (0-199)
consteval std::array<TagEntry, MAX_COMMON_TAG> create_tag_table() {
    std::array<TagEntry, MAX_COMMON_TAG> table{};

    // Initialize all entries as unknown
    for (auto& entry : table) {
        entry = {"", false, false};
    }

    // Populate known tags
    table[1]  = {TagInfo<1>::name, TagInfo<1>::is_header, TagInfo<1>::is_required};
    table[6]  = {TagInfo<6>::name, TagInfo<6>::is_header, TagInfo<6>::is_required};
    table[8]  = {TagInfo<8>::name, TagInfo<8>::is_header, TagInfo<8>::is_required};
    table[9]  = {TagInfo<9>::name, TagInfo<9>::is_header, TagInfo<9>::is_required};
    table[10] = {TagInfo<10>::name, TagInfo<10>::is_header, TagInfo<10>::is_required};
    table[11] = {TagInfo<11>::name, TagInfo<11>::is_header, TagInfo<11>::is_required};
    table[14] = {TagInfo<14>::name, TagInfo<14>::is_header, TagInfo<14>::is_required};
    table[17] = {TagInfo<17>::name, TagInfo<17>::is_header, TagInfo<17>::is_required};
    table[21] = {TagInfo<21>::name, TagInfo<21>::is_header, TagInfo<21>::is_required};
    table[31] = {TagInfo<31>::name, TagInfo<31>::is_header, TagInfo<31>::is_required};
    table[32] = {TagInfo<32>::name, TagInfo<32>::is_header, TagInfo<32>::is_required};
    table[34] = {TagInfo<34>::name, TagInfo<34>::is_header, TagInfo<34>::is_required};
    table[35] = {TagInfo<35>::name, TagInfo<35>::is_header, TagInfo<35>::is_required};
    table[37] = {TagInfo<37>::name, TagInfo<37>::is_header, TagInfo<37>::is_required};
    table[38] = {TagInfo<38>::name, TagInfo<38>::is_header, TagInfo<38>::is_required};
    table[39] = {TagInfo<39>::name, TagInfo<39>::is_header, TagInfo<39>::is_required};
    table[40] = {TagInfo<40>::name, TagInfo<40>::is_header, TagInfo<40>::is_required};
    table[41] = {TagInfo<41>::name, TagInfo<41>::is_header, TagInfo<41>::is_required};
    table[43] = {TagInfo<43>::name, TagInfo<43>::is_header, TagInfo<43>::is_required};
    table[44] = {TagInfo<44>::name, TagInfo<44>::is_header, TagInfo<44>::is_required};
    table[45] = {TagInfo<45>::name, TagInfo<45>::is_header, TagInfo<45>::is_required};
    table[49] = {TagInfo<49>::name, TagInfo<49>::is_header, TagInfo<49>::is_required};
    table[52] = {TagInfo<52>::name, TagInfo<52>::is_header, TagInfo<52>::is_required};
    table[54] = {TagInfo<54>::name, TagInfo<54>::is_header, TagInfo<54>::is_required};
    table[55] = {TagInfo<55>::name, TagInfo<55>::is_header, TagInfo<55>::is_required};
    table[56] = {TagInfo<56>::name, TagInfo<56>::is_header, TagInfo<56>::is_required};
    table[58] = {TagInfo<58>::name, TagInfo<58>::is_header, TagInfo<58>::is_required};
    table[59] = {TagInfo<59>::name, TagInfo<59>::is_header, TagInfo<59>::is_required};
    table[60] = {TagInfo<60>::name, TagInfo<60>::is_header, TagInfo<60>::is_required};
    table[97] = {TagInfo<97>::name, TagInfo<97>::is_header, TagInfo<97>::is_required};
    table[98] = {TagInfo<98>::name, TagInfo<98>::is_header, TagInfo<98>::is_required};
    table[99] = {TagInfo<99>::name, TagInfo<99>::is_header, TagInfo<99>::is_required};
    table[108] = {TagInfo<108>::name, TagInfo<108>::is_header, TagInfo<108>::is_required};
    table[112] = {TagInfo<112>::name, TagInfo<112>::is_header, TagInfo<112>::is_required};
    table[122] = {TagInfo<122>::name, TagInfo<122>::is_header, TagInfo<122>::is_required};
    table[141] = {TagInfo<141>::name, TagInfo<141>::is_header, TagInfo<141>::is_required};
    table[150] = {TagInfo<150>::name, TagInfo<150>::is_header, TagInfo<150>::is_required};
    table[151] = {TagInfo<151>::name, TagInfo<151>::is_header, TagInfo<151>::is_required};

    return table;
}

inline constexpr auto TAG_TABLE = create_tag_table();

} // namespace detail

// ============================================================================
// Compile-time Tag Queries
// ============================================================================

/// Compile-time tag name query
template<int TagNum>
[[nodiscard]] consteval std::string_view tag_name() noexcept {
    return detail::TagInfo<TagNum>::name;
}

/// Compile-time is_header query
template<int TagNum>
[[nodiscard]] consteval bool is_header_tag() noexcept {
    return detail::TagInfo<TagNum>::is_header;
}

/// Compile-time is_required query
template<int TagNum>
[[nodiscard]] consteval bool is_required_tag() noexcept {
    return detail::TagInfo<TagNum>::is_required;
}

// ============================================================================
// Runtime Tag Queries (O(1) lookup for common tags)
// ============================================================================

/// Runtime tag name query
[[nodiscard]] inline constexpr std::string_view tag_name(int tag_num) noexcept {
    if (tag_num >= 0 && tag_num < detail::MAX_COMMON_TAG) [[likely]] {
        return detail::TAG_TABLE[tag_num].name;
    }
    return "";
}

/// Runtime is_header query
[[nodiscard]] inline constexpr bool is_header_tag(int tag_num) noexcept {
    if (tag_num >= 0 && tag_num < detail::MAX_COMMON_TAG) [[likely]] {
        return detail::TAG_TABLE[tag_num].is_header;
    }
    return false;
}

/// Runtime is_required query
[[nodiscard]] inline constexpr bool is_required_tag(int tag_num) noexcept {
    if (tag_num >= 0 && tag_num < detail::MAX_COMMON_TAG) [[likely]] {
        return detail::TAG_TABLE[tag_num].is_required;
    }
    return false;
}

// Static assertions for tag metadata
static_assert(detail::TagInfo<8>::name == "BeginString");
static_assert(detail::TagInfo<8>::is_header == true);
static_assert(detail::TagInfo<8>::is_required == true);
static_assert(detail::TagInfo<35>::name == "MsgType");
static_assert(detail::TAG_TABLE[8].name == "BeginString");
static_assert(detail::TAG_TABLE[35].is_header == true);

// ============================================================================
// Compile-time Tag Utilities
// ============================================================================

/// Get tag value at compile time
template <IsTag T>
consteval int tag_value() noexcept {
    return T::value;
}

/// Check if two tags are the same
template <IsTag T1, IsTag T2>
consteval bool same_tag() noexcept {
    return T1::value == T2::value;
}

/// Compile-time tag comparison
template <IsTag T>
consteval bool operator==(Tag<T::value>, int v) noexcept {
    return T::value == v;
}

} // namespace nfx::tag
