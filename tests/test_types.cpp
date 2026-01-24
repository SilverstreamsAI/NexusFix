#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "nexusfix/types/tag.hpp"
#include "nexusfix/types/field_types.hpp"
#include "nexusfix/types/error.hpp"

using namespace nfx;
using namespace nfx::tag;
using namespace nfx::literals;

// ============================================================================
// Tag Tests
// ============================================================================

TEST_CASE("Tag compile-time values", "[types][tag]") {
    SECTION("Standard header tags") {
        static_assert(BeginString::value == 8);
        static_assert(BodyLength::value == 9);
        static_assert(MsgType::value == 35);
        static_assert(SenderCompID::value == 49);
        static_assert(TargetCompID::value == 56);
        static_assert(MsgSeqNum::value == 34);
        static_assert(SendingTime::value == 52);
        static_assert(CheckSum::value == 10);

        REQUIRE(tag_value<BeginString>() == 8);
        REQUIRE(tag_value<MsgType>() == 35);
    }

    SECTION("Execution report tags") {
        static_assert(OrderID::value == 37);
        static_assert(ExecID::value == 17);
        static_assert(ExecType::value == 150);
        static_assert(OrdStatus::value == 39);
        static_assert(LeavesQty::value == 151);
        static_assert(CumQty::value == 14);
        static_assert(AvgPx::value == 6);
    }

    SECTION("Tag comparison") {
        static_assert(same_tag<BeginString, BeginString>());
        static_assert(!same_tag<BeginString, BodyLength>());
    }
}

// ============================================================================
// FixedPrice Tests
// ============================================================================

TEST_CASE("FixedPrice arithmetic", "[types][price]") {
    SECTION("Construction and conversion") {
        auto price = FixedPrice::from_double(100.50);
        REQUIRE_THAT(price.to_double(), Catch::Matchers::WithinRel(100.50, 0.0001));

        auto price2 = 100.50_price;
        REQUIRE(price.raw == price2.raw);
    }

    SECTION("String parsing") {
        auto p1 = FixedPrice::from_string("123.45");
        REQUIRE_THAT(p1.to_double(), Catch::Matchers::WithinRel(123.45, 0.0001));

        auto p2 = FixedPrice::from_string("100");
        REQUIRE_THAT(p2.to_double(), Catch::Matchers::WithinRel(100.0, 0.0001));

        auto p3 = FixedPrice::from_string("-50.25");
        REQUIRE_THAT(p3.to_double(), Catch::Matchers::WithinRel(-50.25, 0.0001));

        auto p4 = FixedPrice::from_string("0.00000001");
        REQUIRE(p4.raw == 1);  // Minimum precision
    }

    SECTION("Arithmetic operations") {
        auto a = FixedPrice::from_double(100.0);
        auto b = FixedPrice::from_double(25.50);

        auto sum = a + b;
        REQUIRE_THAT(sum.to_double(), Catch::Matchers::WithinRel(125.50, 0.0001));

        auto diff = a - b;
        REQUIRE_THAT(diff.to_double(), Catch::Matchers::WithinRel(74.50, 0.0001));

        auto mult = a * 3;
        REQUIRE_THAT(mult.to_double(), Catch::Matchers::WithinRel(300.0, 0.0001));
    }

    SECTION("Comparison") {
        auto a = 100.0_price;
        auto b = 100.0_price;
        auto c = 99.99_price;

        REQUIRE(a == b);
        REQUIRE(a > c);
        REQUIRE(c < a);
    }
}

// ============================================================================
// Qty Tests
// ============================================================================

TEST_CASE("Qty operations", "[types][qty]") {
    SECTION("Construction") {
        auto q1 = Qty::from_int(100);
        REQUIRE(q1.whole() == 100);

        auto q2 = 500_qty;
        REQUIRE(q2.whole() == 500);
    }

    SECTION("String parsing") {
        auto q1 = Qty::from_string("1000");
        REQUIRE(q1.whole() == 1000);

        auto q2 = Qty::from_string("100.5");
        REQUIRE_THAT(q2.to_double(), Catch::Matchers::WithinRel(100.5, 0.0001));
    }

    SECTION("Arithmetic") {
        auto a = Qty::from_int(100);
        auto b = Qty::from_int(50);

        REQUIRE((a + b).whole() == 150);
        REQUIRE((a - b).whole() == 50);
    }
}

// ============================================================================
// SeqNum Tests
// ============================================================================

TEST_CASE("SeqNum operations", "[types][seqnum]") {
    SECTION("Construction and validation") {
        auto seq = 1_seq;
        REQUIRE(seq.get() == 1);
        REQUIRE(seq.is_valid());

        SeqNum zero{0};
        REQUIRE(!zero.is_valid());
    }

    SECTION("Next sequence") {
        auto seq = SeqNum{1};
        auto next = seq.next();
        REQUIRE(next.get() == 2);
    }

    SECTION("Wrap around") {
        auto max = SeqNum{SeqNum::MAX_VALUE};
        auto wrapped = max.next();
        REQUIRE(wrapped.get() == 1);
    }
}

// ============================================================================
// Enum Tests
// ============================================================================

TEST_CASE("Side enum", "[types][enums]") {
    REQUIRE(is_buy_side(Side::Buy));
    REQUIRE(is_buy_side(Side::BuyMinus));
    REQUIRE(!is_buy_side(Side::Sell));

    REQUIRE(is_sell_side(Side::Sell));
    REQUIRE(is_sell_side(Side::SellShort));
    REQUIRE(!is_sell_side(Side::Buy));
}

TEST_CASE("OrdStatus enum", "[types][enums]") {
    REQUIRE(is_terminal_status(OrdStatus::Filled));
    REQUIRE(is_terminal_status(OrdStatus::Canceled));
    REQUIRE(is_terminal_status(OrdStatus::Rejected));
    REQUIRE(!is_terminal_status(OrdStatus::New));
    REQUIRE(!is_terminal_status(OrdStatus::PartiallyFilled));
}

// ============================================================================
// Error Tests
// ============================================================================

TEST_CASE("ParseError", "[types][error]") {
    SECTION("Default construction") {
        ParseError err;
        REQUIRE(err.ok());
        REQUIRE(!err);  // operator bool returns true for error
    }

    SECTION("Error construction") {
        ParseError err{ParseErrorCode::BufferTooShort};
        REQUIRE(!err.ok());
        REQUIRE(err);
        REQUIRE(err.message() == "Buffer too short");
    }

    SECTION("Error with tag") {
        ParseError err{ParseErrorCode::MissingRequiredField, 35};
        REQUIRE(err.tag == 35);
        REQUIRE(err.message() == "Missing required field");
    }
}

TEST_CASE("std::expected usage", "[types][error]") {
    SECTION("Success result") {
        ParseResult<int> result{42};
        REQUIRE(result.has_value());
        REQUIRE(*result == 42);
    }

    SECTION("Error result") {
        ParseResult<int> result = std::unexpected{
            ParseError{ParseErrorCode::InvalidChecksum}};
        REQUIRE(!result.has_value());
        REQUIRE(result.error().code == ParseErrorCode::InvalidChecksum);
    }
}

// ============================================================================
// Timestamp Tests
// ============================================================================

TEST_CASE("Timestamp operations", "[types][timestamp]") {
    Timestamp ts{1000000000LL};  // 1 second in nanos

    REQUIRE(ts.as_nanos() == 1000000000LL);
    REQUIRE(ts.as_micros() == 1000000LL);
    REQUIRE(ts.as_millis() == 1000LL);
    REQUIRE(ts.as_seconds() == 1LL);
}
