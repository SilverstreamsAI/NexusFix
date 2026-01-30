// SPDX-License-Identifier: MIT
// Copyright (c) 2025 SilverstreamsAI

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <string_view>

#include "nexusfix/sbe/sbe.hpp"

using namespace nfx;
using namespace nfx::sbe;

// ============================================================================
// SBE Types Tests
// ============================================================================

TEST_CASE("SBE read/write int64", "[sbe][types]") {
    alignas(8) char buffer[256]{};

    SECTION("Positive value") {
        constexpr SbeInt64 value = 123456789012345LL;
        write_int64(buffer, value);
        REQUIRE(read_int64(buffer) == value);
    }

    SECTION("Negative value") {
        constexpr SbeInt64 value = -987654321012345LL;
        write_int64(buffer, value);
        REQUIRE(read_int64(buffer) == value);
    }

    SECTION("Zero") {
        write_int64(buffer, 0);
        REQUIRE(read_int64(buffer) == 0);
    }
}

TEST_CASE("SBE read/write uint16", "[sbe][types]") {
    alignas(8) char buffer[256]{};

    constexpr SbeUint16 value = 12345;
    write_uint16(buffer, value);
    REQUIRE(read_uint16(buffer) == value);
}

TEST_CASE("SBE read/write char", "[sbe][types]") {
    alignas(8) char buffer[256]{};

    constexpr SbeChar value = 'X';
    write_char(buffer, value);
    REQUIRE(read_char(buffer) == value);
}

TEST_CASE("SBE null values", "[sbe][types]") {
    REQUIRE(null_value::INT64 == INT64_MIN);
    REQUIRE(null_value::UINT64 == UINT64_MAX);
    REQUIRE(null_value::CHAR == '\0');
}

// ============================================================================
// FixedString Tests
// ============================================================================

TEST_CASE("FixedString8 encode/decode", "[sbe][fixedstring]") {
    alignas(8) char buffer[256]{};

    SECTION("Exact length string") {
        FixedString8::encode(buffer, "AAPL    ");
        REQUIRE(FixedString8::decode(buffer) == "AAPL");
    }

    SECTION("Shorter string (with padding)") {
        FixedString8::encode(buffer, "IBM");
        REQUIRE(FixedString8::decode(buffer) == "IBM");
        // Verify padding
        REQUIRE(buffer[3] == ' ');
        REQUIRE(buffer[7] == ' ');
    }

    SECTION("Longer string (truncated)") {
        FixedString8::encode(buffer, "VERYLONGSYMBOL");
        // Should truncate to 8 chars
        REQUIRE(FixedString8::decode(buffer) == "VERYLONG");
    }

    SECTION("Empty string") {
        FixedString8::clear(buffer);
        REQUIRE(FixedString8::is_null(buffer));
        REQUIRE(FixedString8::decode(buffer) == "");
    }
}

TEST_CASE("FixedString20 encode/decode", "[sbe][fixedstring]") {
    alignas(8) char buffer[256]{};

    SECTION("Max length string") {
        FixedString20::encode(buffer, "ORDER12345678901234");
        // 20 chars max
        auto decoded = FixedString20::decode(buffer);
        REQUIRE(decoded.size() <= 20);
    }

    SECTION("Shorter string") {
        FixedString20::encode(buffer, "ORD001");
        REQUIRE(FixedString20::decode(buffer) == "ORD001");
    }
}

// ============================================================================
// DecimalPrice Tests
// ============================================================================

TEST_CASE("DecimalPrice encode/decode", "[sbe][decimal]") {
    alignas(8) char buffer[256]{};

    SECTION("Normal price") {
        FixedPrice price;
        price.raw = 15050000000LL;  // 150.50 with 8 decimals

        DecimalPrice::encode(buffer, price);
        FixedPrice decoded = DecimalPrice::decode(buffer);

        REQUIRE(decoded.raw == price.raw);
    }

    SECTION("Null price") {
        DecimalPrice::write_null(buffer);
        REQUIRE(DecimalPrice::is_null(buffer));
    }
}

// ============================================================================
// DecimalQty Tests
// ============================================================================

TEST_CASE("DecimalQty encode/decode", "[sbe][decimal]") {
    alignas(8) char buffer[256]{};

    Qty qty;
    qty.raw = 1000000LL;  // 100 with 4 decimals

    DecimalQty::encode(buffer, qty);
    Qty decoded = DecimalQty::decode(buffer);

    REQUIRE(decoded.raw == qty.raw);
}

// ============================================================================
// SbeTimestamp Tests
// ============================================================================

TEST_CASE("SbeTimestamp encode/decode", "[sbe][timestamp]") {
    alignas(8) char buffer[256]{};

    Timestamp ts{1234567890123456789LL};

    SbeTimestamp::encode(buffer, ts);
    Timestamp decoded = SbeTimestamp::decode(buffer);

    REQUIRE(decoded.nanos == ts.nanos);
}

// ============================================================================
// Enum Tests
// ============================================================================

TEST_CASE("SBE enum encode/decode", "[sbe][enum]") {
    alignas(8) char buffer[256]{};

    SECTION("Side::Buy") {
        SbeSide::encode(buffer, Side::Buy);
        REQUIRE(SbeSide::decode(buffer) == Side::Buy);
    }

    SECTION("Side::Sell") {
        SbeSide::encode(buffer, Side::Sell);
        REQUIRE(SbeSide::decode(buffer) == Side::Sell);
    }

    SECTION("OrdType::Limit") {
        SbeOrdType::encode(buffer, OrdType::Limit);
        REQUIRE(SbeOrdType::decode(buffer) == OrdType::Limit);
    }

    SECTION("ExecType::New") {
        SbeExecType::encode(buffer, ExecType::New);
        REQUIRE(SbeExecType::decode(buffer) == ExecType::New);
    }

    SECTION("OrdStatus::Filled") {
        SbeOrdStatus::encode(buffer, OrdStatus::Filled);
        REQUIRE(SbeOrdStatus::decode(buffer) == OrdStatus::Filled);
    }
}

// ============================================================================
// MessageHeader Tests
// ============================================================================

TEST_CASE("MessageHeader encode/decode", "[sbe][header]") {
    alignas(8) char buffer[256]{};

    SECTION("Normal header") {
        auto header = MessageHeader::wrapForEncode(buffer, sizeof(buffer));
        header.encodeHeader(56, MessageHeader::TemplateId::NewOrderSingle);

        auto decoded = MessageHeader::wrapForDecode(buffer, sizeof(buffer));
        REQUIRE(decoded.isValid());
        REQUIRE(decoded.blockLength() == 56);
        REQUIRE(decoded.templateId() == MessageHeader::TemplateId::NewOrderSingle);
        REQUIRE(decoded.schemaId() == MessageHeader::SCHEMA_ID);
        REQUIRE(decoded.version() == MessageHeader::SCHEMA_VERSION);
    }

    SECTION("Schema validation") {
        auto header = MessageHeader::wrapForEncode(buffer, sizeof(buffer));
        header.encodeHeader(56, 1);

        auto decoded = MessageHeader::wrapForDecode(buffer, sizeof(buffer));
        REQUIRE(decoded.validateSchema());
    }

    SECTION("Invalid buffer size") {
        auto header = MessageHeader::wrapForDecode(buffer, 4);  // Too small
        REQUIRE_FALSE(header.isValid());
    }
}

// ============================================================================
// NewOrderSingleCodec Tests
// ============================================================================

TEST_CASE("NewOrderSingleCodec layout constants", "[sbe][nos]") {
    REQUIRE(NewOrderSingleCodec::TOTAL_SIZE == 64u);
    REQUIRE(NewOrderSingleCodec::BLOCK_LENGTH == 56u);
    REQUIRE(NewOrderSingleCodec::TEMPLATE_ID == 1u);
}

TEST_CASE("NewOrderSingleCodec encode/decode roundtrip", "[sbe][nos]") {
    alignas(8) char buffer[NewOrderSingleCodec::TOTAL_SIZE]{};

    FixedPrice price;
    price.raw = 15050000000LL;  // 150.50

    Qty qty;
    qty.raw = 1000000LL;  // 100

    Timestamp ts{1234567890123456789LL};

    // Encode
    NewOrderSingleCodec::wrapForEncode(buffer, sizeof(buffer))
        .encodeHeader()
        .clOrdId("ORD001")
        .symbol("AAPL")
        .side(Side::Buy)
        .ordType(OrdType::Limit)
        .price(price)
        .orderQty(qty)
        .transactTime(ts);

    // Decode
    auto decoder = NewOrderSingleCodec::wrapForDecode(buffer, sizeof(buffer));
    REQUIRE(decoder.isValid());

    REQUIRE(decoder.clOrdId() == "ORD001");
    REQUIRE(decoder.symbol() == "AAPL");
    REQUIRE(decoder.side() == Side::Buy);
    REQUIRE(decoder.ordType() == OrdType::Limit);
    REQUIRE(decoder.price().raw == price.raw);
    REQUIRE(decoder.orderQty().raw == qty.raw);
    REQUIRE(decoder.transactTime().nanos == ts.nanos);
}

TEST_CASE("NewOrderSingleCodec header validation", "[sbe][nos]") {
    alignas(8) char buffer[NewOrderSingleCodec::TOTAL_SIZE]{};

    NewOrderSingleCodec::wrapForEncode(buffer, sizeof(buffer))
        .encodeHeader();

    auto header = MessageHeader::wrapForDecode(buffer, sizeof(buffer));
    REQUIRE(header.templateId() == NewOrderSingleCodec::TEMPLATE_ID);
    REQUIRE(header.blockLength() == NewOrderSingleCodec::BLOCK_LENGTH);
}

TEST_CASE("NewOrderSingleCodec invalid buffer", "[sbe][nos]") {
    alignas(8) char buffer[NewOrderSingleCodec::TOTAL_SIZE]{};

    auto decoder = NewOrderSingleCodec::wrapForDecode(buffer, 32);  // Too small
    REQUIRE_FALSE(decoder.isValid());
}

TEST_CASE("NewOrderSingleCodec encoded span", "[sbe][nos]") {
    alignas(8) char buffer[NewOrderSingleCodec::TOTAL_SIZE]{};

    auto codec = NewOrderSingleCodec::wrapForEncode(buffer, sizeof(buffer))
        .encodeHeader();

    auto span = codec.encoded();
    REQUIRE(span.size() == NewOrderSingleCodec::TOTAL_SIZE);
    REQUIRE(span.data() == buffer);
}

TEST_CASE("NewOrderSingleCodec alignment verification", "[sbe][nos]") {
    // Verify int64 fields are 8-byte aligned within message
    REQUIRE(NewOrderSingleCodec::Offset::Price % 8 == 0);
    REQUIRE(NewOrderSingleCodec::Offset::OrderQty % 8 == 0);
    REQUIRE(NewOrderSingleCodec::Offset::TransactTime % 8 == 0);
}

// ============================================================================
// ExecutionReportCodec Tests
// ============================================================================

TEST_CASE("ExecutionReportCodec layout constants", "[sbe][execrpt]") {
    REQUIRE(ExecutionReportCodec::TOTAL_SIZE == 144u);
    REQUIRE(ExecutionReportCodec::BLOCK_LENGTH == 136u);
    REQUIRE(ExecutionReportCodec::TEMPLATE_ID == 8u);
}

TEST_CASE("ExecutionReportCodec encode/decode roundtrip", "[sbe][execrpt]") {
    alignas(8) char buffer[ExecutionReportCodec::TOTAL_SIZE]{};

    FixedPrice price, lastPx, avgPx;
    price.raw = 15050000000LL;   // 150.50
    lastPx.raw = 15055000000LL;  // 150.55
    avgPx.raw = 15052500000LL;   // 150.525

    Qty orderQty, lastQty, leavesQty, cumQty;
    orderQty.raw = 1000000LL;  // 100
    lastQty.raw = 500000LL;    // 50
    leavesQty.raw = 500000LL;  // 50
    cumQty.raw = 500000LL;     // 50

    Timestamp ts{1234567890123456789LL};

    // Encode
    ExecutionReportCodec::wrapForEncode(buffer, sizeof(buffer))
        .encodeHeader()
        .orderId("EX001")
        .execId("EXEC001")
        .clOrdId("ORD001")
        .symbol("AAPL")
        .side(Side::Buy)
        .execType(ExecType::PartialFill)
        .ordStatus(OrdStatus::PartiallyFilled)
        .price(price)
        .orderQty(orderQty)
        .lastPx(lastPx)
        .lastQty(lastQty)
        .leavesQty(leavesQty)
        .cumQty(cumQty)
        .avgPx(avgPx)
        .transactTime(ts);

    // Decode
    auto decoder = ExecutionReportCodec::wrapForDecode(buffer, sizeof(buffer));
    REQUIRE(decoder.isValid());

    REQUIRE(decoder.orderId() == "EX001");
    REQUIRE(decoder.execId() == "EXEC001");
    REQUIRE(decoder.clOrdId() == "ORD001");
    REQUIRE(decoder.symbol() == "AAPL");
    REQUIRE(decoder.side() == Side::Buy);
    REQUIRE(decoder.execType() == ExecType::PartialFill);
    REQUIRE(decoder.ordStatus() == OrdStatus::PartiallyFilled);
    REQUIRE(decoder.price().raw == price.raw);
    REQUIRE(decoder.orderQty().raw == orderQty.raw);
    REQUIRE(decoder.lastPx().raw == lastPx.raw);
    REQUIRE(decoder.lastQty().raw == lastQty.raw);
    REQUIRE(decoder.leavesQty().raw == leavesQty.raw);
    REQUIRE(decoder.cumQty().raw == cumQty.raw);
    REQUIRE(decoder.avgPx().raw == avgPx.raw);
    REQUIRE(decoder.transactTime().nanos == ts.nanos);
}

TEST_CASE("ExecutionReportCodec header validation", "[sbe][execrpt]") {
    alignas(8) char buffer[ExecutionReportCodec::TOTAL_SIZE]{};

    ExecutionReportCodec::wrapForEncode(buffer, sizeof(buffer))
        .encodeHeader();

    auto header = MessageHeader::wrapForDecode(buffer, sizeof(buffer));
    REQUIRE(header.templateId() == ExecutionReportCodec::TEMPLATE_ID);
    REQUIRE(header.blockLength() == ExecutionReportCodec::BLOCK_LENGTH);
}

TEST_CASE("ExecutionReportCodec invalid buffer", "[sbe][execrpt]") {
    alignas(8) char buffer[ExecutionReportCodec::TOTAL_SIZE]{};

    auto decoder = ExecutionReportCodec::wrapForDecode(buffer, 64);  // Too small
    REQUIRE_FALSE(decoder.isValid());
}

TEST_CASE("ExecutionReportCodec alignment verification", "[sbe][execrpt]") {
    // Verify int64 fields are 8-byte aligned within message
    REQUIRE(ExecutionReportCodec::Offset::Price % 8 == 0);
    REQUIRE(ExecutionReportCodec::Offset::OrderQty % 8 == 0);
    REQUIRE(ExecutionReportCodec::Offset::LastPx % 8 == 0);
    REQUIRE(ExecutionReportCodec::Offset::LastQty % 8 == 0);
    REQUIRE(ExecutionReportCodec::Offset::LeavesQty % 8 == 0);
    REQUIRE(ExecutionReportCodec::Offset::CumQty % 8 == 0);
    REQUIRE(ExecutionReportCodec::Offset::AvgPx % 8 == 0);
    REQUIRE(ExecutionReportCodec::Offset::TransactTime % 8 == 0);
}

// ============================================================================
// Dispatch Tests
// ============================================================================

TEST_CASE("Dispatch NewOrderSingle", "[sbe][dispatch]") {
    alignas(8) char buffer[NewOrderSingleCodec::TOTAL_SIZE]{};

    NewOrderSingleCodec::wrapForEncode(buffer, sizeof(buffer))
        .encodeHeader()
        .clOrdId("ORD001")
        .symbol("AAPL");

    bool dispatched = false;
    dispatch(buffer, sizeof(buffer), [&](auto& codec) {
        using T = std::decay_t<decltype(codec)>;
        if constexpr (std::is_same_v<T, NewOrderSingleCodec>) {
            dispatched = true;
            REQUIRE(codec.clOrdId() == "ORD001");
            REQUIRE(codec.symbol() == "AAPL");
        }
    });
    REQUIRE(dispatched);
}

TEST_CASE("Dispatch ExecutionReport", "[sbe][dispatch]") {
    alignas(8) char buffer[ExecutionReportCodec::TOTAL_SIZE]{};

    ExecutionReportCodec::wrapForEncode(buffer, sizeof(buffer))
        .encodeHeader()
        .orderId("EX001")
        .symbol("MSFT");

    bool dispatched = false;
    dispatch(buffer, sizeof(buffer), [&](auto& codec) {
        using T = std::decay_t<decltype(codec)>;
        if constexpr (std::is_same_v<T, ExecutionReportCodec>) {
            dispatched = true;
            REQUIRE(codec.orderId() == "EX001");
            REQUIRE(codec.symbol() == "MSFT");
        }
    });
    REQUIRE(dispatched);
}

TEST_CASE("Dispatch unknown message", "[sbe][dispatch]") {
    alignas(8) char buffer[256]{};

    // Write invalid template ID
    auto header = MessageHeader::wrapForEncode(buffer, sizeof(buffer));
    header.encodeHeader(100, 999);  // Unknown template ID

    bool dispatched = false;
    dispatch(buffer, sizeof(buffer), [&](auto& codec) {
        using T = std::decay_t<decltype(codec)>;
        if constexpr (std::is_same_v<T, UnknownMessage>) {
            dispatched = true;
            REQUIRE(codec.templateId == 999);
        }
    });
    REQUIRE(dispatched);
}

TEST_CASE("Dispatch empty buffer", "[sbe][dispatch]") {
    alignas(8) char buffer[256]{};

    bool dispatched = false;
    dispatch(buffer, 0, [&](auto& codec) {
        using T = std::decay_t<decltype(codec)>;
        if constexpr (std::is_same_v<T, UnknownMessage>) {
            dispatched = true;
        }
    });
    REQUIRE(dispatched);
}

// ============================================================================
// Type Traits Tests
// ============================================================================

TEST_CASE("SBE type traits", "[sbe][traits]") {
    SECTION("is_sbe_codec") {
        REQUIRE(is_sbe_codec_v<NewOrderSingleCodec>);
        REQUIRE(is_sbe_codec_v<ExecutionReportCodec>);
        REQUIRE_FALSE(is_sbe_codec_v<MessageHeader>);
        REQUIRE_FALSE(is_sbe_codec_v<int>);
    }

    SECTION("required_buffer_size") {
        REQUIRE(required_buffer_size<NewOrderSingleCodec>() == 64u);
        REQUIRE(required_buffer_size<ExecutionReportCodec>() == 144u);
    }

    SECTION("MAX_MESSAGE_SIZE") {
        REQUIRE(MAX_MESSAGE_SIZE == 144u);  // ExecutionReport is largest
    }
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_CASE("NewOrderSingleCodec edge cases", "[sbe][nos][edge]") {
    alignas(8) char buffer[NewOrderSingleCodec::TOTAL_SIZE]{};

    SECTION("Empty strings") {
        NewOrderSingleCodec::wrapForEncode(buffer, sizeof(buffer))
            .encodeHeader()
            .clOrdId("")
            .symbol("");

        auto decoder = NewOrderSingleCodec::wrapForDecode(buffer, sizeof(buffer));
        REQUIRE(decoder.isValid());
        REQUIRE(decoder.clOrdId() == "");
        REQUIRE(decoder.symbol() == "");
    }

    SECTION("Max length strings") {
        // 20-char clOrdId, 8-char symbol
        NewOrderSingleCodec::wrapForEncode(buffer, sizeof(buffer))
            .encodeHeader()
            .clOrdId("12345678901234567890")
            .symbol("12345678");

        auto decoder = NewOrderSingleCodec::wrapForDecode(buffer, sizeof(buffer));
        REQUIRE(decoder.isValid());
        REQUIRE(decoder.clOrdId().size() == 20u);
        REQUIRE(decoder.symbol().size() == 8u);
    }

    SECTION("Zero values") {
        FixedPrice zero_price;
        zero_price.raw = 0;

        Qty zero_qty;
        zero_qty.raw = 0;

        Timestamp zero_ts{0};

        NewOrderSingleCodec::wrapForEncode(buffer, sizeof(buffer))
            .encodeHeader()
            .clOrdId("ORD001")
            .symbol("AAPL")
            .side(Side::Buy)
            .ordType(OrdType::Market)
            .price(zero_price)
            .orderQty(zero_qty)
            .transactTime(zero_ts);

        auto decoder = NewOrderSingleCodec::wrapForDecode(buffer, sizeof(buffer));
        REQUIRE(decoder.isValid());
        REQUIRE(decoder.price().raw == 0);
        REQUIRE(decoder.orderQty().raw == 0);
        REQUIRE(decoder.transactTime().nanos == 0);
    }

    SECTION("Negative price") {
        // Some exchanges support negative prices (e.g., oil futures)
        FixedPrice negative;
        negative.raw = -3700000000LL;  // -37.00

        NewOrderSingleCodec::wrapForEncode(buffer, sizeof(buffer))
            .encodeHeader()
            .price(negative);

        auto decoder = NewOrderSingleCodec::wrapForDecode(buffer, sizeof(buffer));
        REQUIRE(decoder.price().raw == negative.raw);
    }
}
