#include <catch2/catch_test_macros.hpp>
#include <string>
#include <cstring>

#include "nexusfix/messages/fix44/market_data.hpp"
#include "nexusfix/messages/common/trailer.hpp"

using namespace nfx;
using namespace nfx::fix44;

// Helper to replace | with SOH for test data
static std::string make_fix_message(std::string_view msg) {
    std::string result{msg};
    for (char& c : result) {
        if (c == '|') c = fix::SOH;
    }
    return result;
}

// ============================================================================
// MarketDataRequest Tests
// ============================================================================

TEST_CASE("MarketDataRequest Builder - Basic subscription", "[market_data][request]") {
    MessageAssembler asm_;
    MarketDataRequest::Builder builder;

    auto msg = builder
        .sender_comp_id("SENDER")
        .target_comp_id("TARGET")
        .msg_seq_num(1)
        .sending_time("20260122-10:00:00.000")
        .md_req_id("MD001")
        .subscription_type(SubscriptionRequestType::SnapshotPlusUpdates)
        .market_depth(5)
        .md_update_type(MDUpdateType::IncrementalRefresh)
        .add_entry_type(MDEntryType::Bid)
        .add_entry_type(MDEntryType::Offer)
        .add_symbol("AAPL")
        .add_symbol("GOOGL")
        .build(asm_);

    std::string msg_str{msg.data(), msg.size()};

    // Verify message type
    REQUIRE(msg_str.find("35=V") != std::string::npos);

    // Verify MDReqID
    REQUIRE(msg_str.find("262=MD001") != std::string::npos);

    // Verify subscription type (1 = SnapshotPlusUpdates)
    REQUIRE(msg_str.find("263=1") != std::string::npos);

    // Verify market depth
    REQUIRE(msg_str.find("264=5") != std::string::npos);

    // Verify entry types count
    REQUIRE(msg_str.find("267=2") != std::string::npos);

    // Verify symbols count
    REQUIRE(msg_str.find("146=2") != std::string::npos);

    // Verify symbols
    REQUIRE(msg_str.find("55=AAPL") != std::string::npos);
    REQUIRE(msg_str.find("55=GOOGL") != std::string::npos);
}

TEST_CASE("MarketDataRequest Builder - Snapshot only", "[market_data][request]") {
    MessageAssembler asm_;
    MarketDataRequest::Builder builder;

    auto msg = builder
        .sender_comp_id("SENDER")
        .target_comp_id("TARGET")
        .msg_seq_num(1)
        .sending_time("20260122-10:00:00.000")
        .md_req_id("SNAP001")
        .subscription_type(SubscriptionRequestType::Snapshot)
        .market_depth(0)  // Full book
        .add_entry_type(MDEntryType::Bid)
        .add_entry_type(MDEntryType::Offer)
        .add_entry_type(MDEntryType::Trade)
        .add_symbol("MSFT")
        .build(asm_);

    std::string msg_str{msg.data(), msg.size()};

    // Verify subscription type (0 = Snapshot)
    REQUIRE(msg_str.find("263=0") != std::string::npos);

    // Verify 3 entry types
    REQUIRE(msg_str.find("267=3") != std::string::npos);
}

// ============================================================================
// MarketDataSnapshotFullRefresh Tests
// ============================================================================

TEST_CASE("MarketDataSnapshotFullRefresh Parser - Basic snapshot", "[market_data][snapshot]") {
    std::string raw_msg = make_fix_message(
        "8=FIX.4.4|9=150|35=W|49=SERVER|56=CLIENT|34=1|52=20260122-10:00:00.000|"
        "262=MD001|55=AAPL|268=3|"
        "269=0|270=150.25|271=1000|"
        "269=1|270=150.30|271=500|"
        "269=2|270=150.27|271=100|"
        "10=000|"
    );

    auto result = MarketDataSnapshotFullRefresh::from_buffer(
        std::span<const char>{raw_msg.data(), raw_msg.size()});

    REQUIRE(result.has_value());

    auto& msg = *result;

    REQUIRE(msg.symbol == "AAPL");
    REQUIRE(msg.md_req_id == "MD001");
    REQUIRE(msg.no_md_entries == 3);
    REQUIRE(msg.entry_count() == 3);
}

TEST_CASE("MarketDataSnapshotFullRefresh Parser - Iterate entries", "[market_data][snapshot][iterator]") {
    std::string raw_msg = make_fix_message(
        "8=FIX.4.4|9=180|35=W|49=SERVER|56=CLIENT|34=1|52=20260122-10:00:00.000|"
        "262=MD002|55=GOOGL|268=2|"
        "269=0|270=2800.50|271=200|290=1|"
        "269=1|270=2801.00|271=150|290=1|"
        "10=000|"
    );

    auto result = MarketDataSnapshotFullRefresh::from_buffer(
        std::span<const char>{raw_msg.data(), raw_msg.size()});

    REQUIRE(result.has_value());

    auto& msg = *result;

    REQUIRE(msg.symbol == "GOOGL");

    auto iter = msg.entries();
    REQUIRE(iter.count() == 2);

    // First entry (Bid)
    REQUIRE(iter.has_next());
    MDEntry entry1 = iter.next();
    REQUIRE(entry1.entry_type == MDEntryType::Bid);
    REQUIRE(entry1.has_price());
    REQUIRE(entry1.has_size());

    // Second entry (Offer)
    REQUIRE(iter.has_next());
    MDEntry entry2 = iter.next();
    REQUIRE(entry2.entry_type == MDEntryType::Offer);

    // No more entries
    REQUIRE_FALSE(iter.has_next());
}

// ============================================================================
// MarketDataIncrementalRefresh Tests
// ============================================================================

TEST_CASE("MarketDataIncrementalRefresh Parser - Basic update", "[market_data][incremental]") {
    std::string raw_msg = make_fix_message(
        "8=FIX.4.4|9=120|35=X|49=SERVER|56=CLIENT|34=2|52=20260122-10:00:01.000|"
        "262=MD001|268=2|"
        "279=0|269=0|55=AAPL|270=150.30|271=1200|"
        "279=1|269=1|55=AAPL|270=150.35|271=400|"
        "10=000|"
    );

    auto result = MarketDataIncrementalRefresh::from_buffer(
        std::span<const char>{raw_msg.data(), raw_msg.size()});

    REQUIRE(result.has_value());

    auto& msg = *result;

    REQUIRE(msg.md_req_id == "MD001");
    REQUIRE(msg.no_md_entries == 2);
    REQUIRE(msg.entry_count() == 2);
}

TEST_CASE("MarketDataIncrementalRefresh Parser - Update actions", "[market_data][incremental][actions]") {
    std::string raw_msg = make_fix_message(
        "8=FIX.4.4|9=150|35=X|49=SERVER|56=CLIENT|34=3|52=20260122-10:00:02.000|"
        "268=3|"
        "279=0|269=0|55=MSFT|270=400.00|271=100|"
        "279=1|269=0|55=MSFT|270=399.95|271=150|"
        "279=2|269=1|55=MSFT|270=400.10|"
        "10=000|"
    );

    auto result = MarketDataIncrementalRefresh::from_buffer(
        std::span<const char>{raw_msg.data(), raw_msg.size()});

    REQUIRE(result.has_value());

    auto& msg = *result;

    REQUIRE(msg.entry_count() == 3);

    auto iter = msg.entries();

    // First: New bid
    REQUIRE(iter.has_next());
    MDEntry e1 = iter.next();
    REQUIRE(e1.update_action == MDUpdateAction::New);
    REQUIRE(e1.entry_type == MDEntryType::Bid);

    // Second: Change bid
    REQUIRE(iter.has_next());
    MDEntry e2 = iter.next();
    REQUIRE(e2.update_action == MDUpdateAction::Change);

    // Third: Delete offer
    REQUIRE(iter.has_next());
    MDEntry e3 = iter.next();
    REQUIRE(e3.update_action == MDUpdateAction::Delete);
    REQUIRE(e3.entry_type == MDEntryType::Offer);
}

// ============================================================================
// MarketDataRequestReject Tests
// ============================================================================

TEST_CASE("MarketDataRequestReject Parser - Unknown symbol", "[market_data][reject]") {
    std::string raw_msg = make_fix_message(
        "8=FIX.4.4|9=100|35=Y|49=SERVER|56=CLIENT|34=1|52=20260122-10:00:00.000|"
        "262=MD001|281=0|58=Symbol not found|"
        "10=000|"
    );

    auto result = MarketDataRequestReject::from_buffer(
        std::span<const char>{raw_msg.data(), raw_msg.size()});

    REQUIRE(result.has_value());

    auto& msg = *result;

    REQUIRE(msg.md_req_id == "MD001");
    REQUIRE(msg.md_req_rej_reason == MDReqRejReason::UnknownSymbol);
    REQUIRE(msg.text == "Symbol not found");
    REQUIRE(msg.rejection_reason_name() == "UnknownSymbol");
}

TEST_CASE("MarketDataRequestReject Parser - Insufficient permissions", "[market_data][reject]") {
    std::string raw_msg = make_fix_message(
        "8=FIX.4.4|9=90|35=Y|49=SERVER|56=CLIENT|34=2|52=20260122-10:00:00.000|"
        "262=MD002|281=2|58=Not authorized for this symbol|"
        "10=000|"
    );

    auto result = MarketDataRequestReject::from_buffer(
        std::span<const char>{raw_msg.data(), raw_msg.size()});

    REQUIRE(result.has_value());

    auto& msg = *result;

    REQUIRE(msg.md_req_rej_reason == MDReqRejReason::InsufficientPermissions);
    REQUIRE(msg.rejection_reason_name() == "InsufficientPermissions");
}

// ============================================================================
// Market Data Types Tests
// ============================================================================

TEST_CASE("MDEntryType - Name conversion", "[market_data][types]") {
    REQUIRE(md_entry_type_name(MDEntryType::Bid) == "Bid");
    REQUIRE(md_entry_type_name(MDEntryType::Offer) == "Offer");
    REQUIRE(md_entry_type_name(MDEntryType::Trade) == "Trade");
    REQUIRE(md_entry_type_name(MDEntryType::SettlementPrice) == "SettlementPrice");
}

TEST_CASE("MDEntryType - Classification", "[market_data][types]") {
    REQUIRE(is_quote_type(MDEntryType::Bid));
    REQUIRE(is_quote_type(MDEntryType::Offer));
    REQUIRE_FALSE(is_quote_type(MDEntryType::Trade));

    REQUIRE(is_trade_type(MDEntryType::Trade));
    REQUIRE(is_trade_type(MDEntryType::TradeVolume));
    REQUIRE_FALSE(is_trade_type(MDEntryType::Bid));
}

TEST_CASE("MDUpdateAction - Name conversion", "[market_data][types]") {
    REQUIRE(md_update_action_name(MDUpdateAction::New) == "New");
    REQUIRE(md_update_action_name(MDUpdateAction::Change) == "Change");
    REQUIRE(md_update_action_name(MDUpdateAction::Delete) == "Delete");
}

TEST_CASE("SubscriptionRequestType - Name conversion", "[market_data][types]") {
    REQUIRE(subscription_type_name(SubscriptionRequestType::Snapshot) == "Snapshot");
    REQUIRE(subscription_type_name(SubscriptionRequestType::SnapshotPlusUpdates) == "Subscribe");
    REQUIRE(subscription_type_name(SubscriptionRequestType::DisablePreviousSnapshot) == "Unsubscribe");
}

TEST_CASE("MDReqRejReason - Name conversion", "[market_data][types]") {
    REQUIRE(md_rej_reason_name(MDReqRejReason::UnknownSymbol) == "UnknownSymbol");
    REQUIRE(md_rej_reason_name(MDReqRejReason::DuplicateMDReqID) == "DuplicateMDReqID");
    REQUIRE(md_rej_reason_name(MDReqRejReason::UnsupportedMarketDepth) == "UnsupportedMarketDepth");
}
