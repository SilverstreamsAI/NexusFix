#pragma once

#include <cstdint>
#include <concepts>
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
