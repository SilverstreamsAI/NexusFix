#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <string>
#include <cstring>

#include "nexusfix/parser/field_view.hpp"
#include "nexusfix/parser/simd_scanner.hpp"
#include "nexusfix/parser/consteval_parser.hpp"
#include "nexusfix/parser/runtime_parser.hpp"
#include "nexusfix/parser/structural_index.hpp"
#include "nexusfix/interfaces/i_message.hpp"

using namespace nfx;

// ============================================================================
// Test Data
// ============================================================================

namespace {

// Sample ExecutionReport message (FIX 4.4)
// 8=FIX.4.4|9=176|35=8|49=SENDER|56=TARGET|34=1|52=20231215-10:30:00.000|
// 37=ORDER123|17=EXEC456|150=0|39=0|55=AAPL|54=1|38=100|44=150.50|151=100|14=0|6=0|10=004|
const std::string EXEC_REPORT =
    "8=FIX.4.4\x01" "9=176\x01" "35=8\x01" "49=SENDER\x01" "56=TARGET\x01"
    "34=1\x01" "52=20231215-10:30:00.000\x01" "37=ORDER123\x01" "17=EXEC456\x01"
    "150=0\x01" "39=0\x01" "55=AAPL\x01" "54=1\x01" "38=100\x01" "44=150.50\x01"
    "151=100\x01" "14=0\x01" "6=0\x01" "10=004\x01";

// Simple Logon message
const std::string LOGON =
    "8=FIX.4.4\x01" "9=70\x01" "35=A\x01" "49=CLIENT\x01" "56=SERVER\x01"
    "34=1\x01" "52=20231215-10:30:00\x01" "98=0\x01" "108=30\x01" "10=185\x01";

// Heartbeat message
const std::string HEARTBEAT =
    "8=FIX.4.4\x01" "9=55\x01" "35=0\x01" "49=SENDER\x01" "56=TARGET\x01"
    "34=5\x01" "52=20231215-10:30:00\x01" "10=136\x01";

}  // namespace

// ============================================================================
// FieldView Tests
// ============================================================================

TEST_CASE("FieldView basic operations", "[parser][field_view]") {
    SECTION("Construction") {
        const char* data = "12345";
        FieldView field{44, std::span<const char>{data, 5}};

        REQUIRE(field.tag == 44);
        REQUIRE(field.as_string() == "12345");
        REQUIRE(field.is_valid());
    }

    SECTION("Integer parsing") {
        const char* data = "12345";
        FieldView field{38, std::span<const char>{data, 5}};

        auto val = field.as_int();
        REQUIRE(val.has_value());
        REQUIRE(*val == 12345);
    }

    SECTION("Negative integer") {
        const char* data = "-500";
        FieldView field{38, std::span<const char>{data, 4}};

        auto val = field.as_int();
        REQUIRE(val.has_value());
        REQUIRE(*val == -500);
    }

    SECTION("Price parsing") {
        const char* data = "150.75";
        FieldView field{44, std::span<const char>{data, 6}};

        auto price = field.as_price();
        REQUIRE_THAT(price.to_double(),
            Catch::Matchers::WithinRel(150.75, 0.0001));
    }

    SECTION("Char parsing") {
        const char* data = "1";
        FieldView field{54, std::span<const char>{data, 1}};

        REQUIRE(field.as_char() == '1');
    }

    SECTION("Bool parsing") {
        const char* yes = "Y";
        const char* no = "N";

        FieldView field_y{43, std::span<const char>{yes, 1}};
        FieldView field_n{43, std::span<const char>{no, 1}};

        REQUIRE(field_y.as_bool() == true);
        REQUIRE(field_n.as_bool() == false);
    }

    SECTION("Side enum parsing") {
        const char* buy = "1";
        const char* sell = "2";

        FieldView field_buy{54, std::span<const char>{buy, 1}};
        FieldView field_sell{54, std::span<const char>{sell, 1}};

        REQUIRE(field_buy.as_side() == Side::Buy);
        REQUIRE(field_sell.as_side() == Side::Sell);
    }
}

// ============================================================================
// FieldIterator Tests
// ============================================================================

TEST_CASE("FieldIterator", "[parser][field_view]") {
    const std::string msg = "8=FIX.4.4\x01" "35=A\x01" "49=SENDER\x01";

    SECTION("Iterate all fields") {
        FieldIterator iter{std::span<const char>{msg.data(), msg.size()}};

        auto f1 = iter.next();
        REQUIRE(f1.tag == 8);
        REQUIRE(f1.as_string() == "FIX.4.4");

        auto f2 = iter.next();
        REQUIRE(f2.tag == 35);
        REQUIRE(f2.as_char() == 'A');

        auto f3 = iter.next();
        REQUIRE(f3.tag == 49);
        REQUIRE(f3.as_string() == "SENDER");

        auto f4 = iter.next();
        REQUIRE(!f4.is_valid());  // No more fields
    }
}

// ============================================================================
// FieldTable Tests
// ============================================================================

TEST_CASE("FieldTable O(1) lookup", "[parser][field_view]") {
    FieldTable<512> table;

    const char* val1 = "AAPL";
    const char* val2 = "100";

    table.set(55, std::span<const char>{val1, 4});
    table.set(38, std::span<const char>{val2, 3});

    SECTION("Lookup existing") {
        REQUIRE(table.has(55));
        REQUIRE(table.get_string(55) == "AAPL");

        REQUIRE(table.has(38));
        auto qty = table.get_int(38);
        REQUIRE(qty.has_value());
        REQUIRE(*qty == 100);
    }

    SECTION("Lookup non-existing") {
        REQUIRE(!table.has(999));
        REQUIRE(table.get_string(999) == "");
    }
}

// ============================================================================
// SIMD Scanner Tests
// ============================================================================

TEST_CASE("SIMD SOH scanning", "[parser][simd]") {
    SECTION("Scalar scanner") {
        const std::string data = "8=FIX.4.4\x01" "35=A\x01" "10=000\x01";
        auto positions = simd::scan_soh_scalar(
            std::span<const char>{data.data(), data.size()});

        REQUIRE(positions.count == 3);
        REQUIRE(data[positions[0]] == '\x01');
        REQUIRE(data[positions[1]] == '\x01');
        REQUIRE(data[positions[2]] == '\x01');
    }

    SECTION("Find SOH") {
        const std::string data = "8=FIX.4.4\x01" "35=A\x01";
        size_t pos = simd::find_soh(
            std::span<const char>{data.data(), data.size()});

        REQUIRE(pos == 9);  // Position of first SOH
    }

    SECTION("Find equals") {
        const std::string data = "35=A";
        size_t pos = simd::find_equals(
            std::span<const char>{data.data(), data.size()});

        REQUIRE(pos == 2);  // Position of '='
    }

    SECTION("Count SOH") {
        size_t count = simd::count_soh(
            std::span<const char>{EXEC_REPORT.data(), EXEC_REPORT.size()});

        REQUIRE(count == 19);  // Number of fields
    }
}

// ============================================================================
// Consteval Parser Tests
// ============================================================================

TEST_CASE("Header parsing", "[parser][consteval]") {
    SECTION("Valid header") {
        auto result = parse_header(
            std::span<const char>{EXEC_REPORT.data(), EXEC_REPORT.size()});

        REQUIRE(result.ok());
        REQUIRE(result.header.begin_string == "FIX.4.4");
        REQUIRE(result.header.body_length == 176);
        REQUIRE(result.header.msg_type == '8');
        REQUIRE(result.header.sender_comp_id == "SENDER");
        REQUIRE(result.header.target_comp_id == "TARGET");
        REQUIRE(result.header.msg_seq_num == 1);
    }

    SECTION("Logon header") {
        auto result = parse_header(
            std::span<const char>{LOGON.data(), LOGON.size()});

        REQUIRE(result.ok());
        REQUIRE(result.header.msg_type == 'A');
        REQUIRE(result.header.sender_comp_id == "CLIENT");
        REQUIRE(result.header.target_comp_id == "SERVER");
    }

    SECTION("Buffer too short") {
        const std::string short_msg = "8=FIX";
        auto result = parse_header(
            std::span<const char>{short_msg.data(), short_msg.size()});

        REQUIRE(!result.ok());
        REQUIRE(result.error.code == ParseErrorCode::BufferTooShort);
    }
}

// ============================================================================
// Runtime Parser Tests
// ============================================================================

TEST_CASE("ParsedMessage", "[parser][runtime]") {
    SECTION("Parse execution report") {
        auto result = ParsedMessage::parse(
            std::span<const char>{EXEC_REPORT.data(), EXEC_REPORT.size()});

        REQUIRE(result.has_value());

        auto& msg = *result;
        REQUIRE(msg.msg_type() == '8');
        REQUIRE(msg.sender_comp_id() == "SENDER");
        REQUIRE(msg.target_comp_id() == "TARGET");
        REQUIRE(msg.msg_seq_num() == 1);

        // Check body fields
        REQUIRE(msg.get_string(37) == "ORDER123");  // OrderID
        REQUIRE(msg.get_string(17) == "EXEC456");   // ExecID
        REQUIRE(msg.get_string(55) == "AAPL");      // Symbol
        REQUIRE(msg.get_char(54) == '1');           // Side = Buy
    }

    SECTION("Field iteration") {
        auto result = ParsedMessage::parse(
            std::span<const char>{HEARTBEAT.data(), HEARTBEAT.size()});

        REQUIRE(result.has_value());

        size_t count = 0;
        for (const auto& field : *result) {
            REQUIRE(field.is_valid());
            ++count;
        }
        REQUIRE(count == result->field_count());
    }
}

TEST_CASE("IndexedParser O(1) lookup", "[parser][runtime]") {
    auto result = IndexedParser::parse(
        std::span<const char>{EXEC_REPORT.data(), EXEC_REPORT.size()});

    REQUIRE(result.has_value());

    auto& parser = *result;

    SECTION("Header access") {
        REQUIRE(parser.msg_type() == '8');
        REQUIRE(parser.sender_comp_id() == "SENDER");
    }

    SECTION("Field lookup O(1)") {
        REQUIRE(parser.has_field(55));
        REQUIRE(parser.get_string(55) == "AAPL");

        REQUIRE(parser.has_field(54));
        REQUIRE(parser.get_char(54) == '1');

        REQUIRE(!parser.has_field(999));
    }
}

// ============================================================================
// Message Type Detection
// ============================================================================

TEST_CASE("Message type detection", "[parser]") {
    SECTION("Admin messages") {
        REQUIRE(msg_type::is_admin('0'));  // Heartbeat
        REQUIRE(msg_type::is_admin('A'));  // Logon
        REQUIRE(msg_type::is_admin('5'));  // Logout

        REQUIRE(!msg_type::is_admin('8'));  // ExecutionReport
        REQUIRE(!msg_type::is_admin('D'));  // NewOrderSingle
    }

    SECTION("App messages") {
        REQUIRE(msg_type::is_app('8'));  // ExecutionReport
        REQUIRE(msg_type::is_app('D'));  // NewOrderSingle
        REQUIRE(msg_type::is_app('F'));  // OrderCancelRequest

        REQUIRE(!msg_type::is_app('0'));  // Heartbeat
        REQUIRE(!msg_type::is_app('A'));  // Logon
    }

    SECTION("Message type names") {
        REQUIRE(msg_type::name('0') == "Heartbeat");
        REQUIRE(msg_type::name('A') == "Logon");
        REQUIRE(msg_type::name('8') == "ExecutionReport");
        REQUIRE(msg_type::name('D') == "NewOrderSingle");
    }
}

// ============================================================================
// FIX Protocol Utilities
// ============================================================================

TEST_CASE("FIX checksum", "[parser][fix]") {
    SECTION("Calculate checksum") {
        // Simple test data
        const std::string data = "8=FIX.4.4\x01" "9=5\x01" "35=0\x01";
        uint8_t checksum = fix::calculate_checksum(
            std::span<const char>{data.data(), data.size()});

        // checksum is uint8_t, so it's inherently in range [0, 255]
        REQUIRE(checksum > 0);
    }

    SECTION("Format checksum") {
        auto formatted = fix::format_checksum(7);
        REQUIRE(formatted[0] == '0');
        REQUIRE(formatted[1] == '0');
        REQUIRE(formatted[2] == '7');

        auto formatted2 = fix::format_checksum(123);
        REQUIRE(formatted2[0] == '1');
        REQUIRE(formatted2[1] == '2');
        REQUIRE(formatted2[2] == '3');
    }
}

// ============================================================================
// Stream Parser Tests
// ============================================================================

TEST_CASE("StreamParser message framing", "[parser][stream]") {
    StreamParser parser;

    SECTION("Single complete message") {
        size_t consumed = parser.feed(
            std::span<const char>{HEARTBEAT.data(), HEARTBEAT.size()});

        REQUIRE(consumed > 0);
        REQUIRE(parser.has_message());

        auto [start, end] = parser.next_message();
        REQUIRE(start == 0);
        REQUIRE(end == HEARTBEAT.size());
    }
}

// ============================================================================
// Message Boundary Detection
// ============================================================================

TEST_CASE("Message boundary detection", "[parser][simd]") {
    SECTION("Find complete message") {
        auto boundary = simd::find_message_boundary(
            std::span<const char>{EXEC_REPORT.data(), EXEC_REPORT.size()});

        REQUIRE(boundary.complete);
        REQUIRE(boundary.start == 0);
        REQUIRE(boundary.end == EXEC_REPORT.size());
    }
}

// ============================================================================
// Structural Index Tests (TICKET_208 simdjson-style)
// ============================================================================

TEST_CASE("FIXStructuralIndex scalar", "[parser][simd][structural]") {
    SECTION("Build index from execution report") {
        auto idx = simd::build_index_scalar(
            std::span<const char>{EXEC_REPORT.data(), EXEC_REPORT.size()});

        REQUIRE(idx.valid());
        REQUIRE(idx.soh_count == 19);     // 19 fields in EXEC_REPORT
        REQUIRE(idx.equals_count == 19);  // Each field has one '='
        REQUIRE(idx.field_count() == 19);
    }

    SECTION("Extract tag at index") {
        auto idx = simd::build_index_scalar(
            std::span<const char>{EXEC_REPORT.data(), EXEC_REPORT.size()});

        // First field is tag 8 (BeginString)
        REQUIRE(idx.tag_at(
            std::span<const char>{EXEC_REPORT.data(), EXEC_REPORT.size()}, 0) == 8);

        // Third field is tag 35 (MsgType)
        REQUIRE(idx.tag_at(
            std::span<const char>{EXEC_REPORT.data(), EXEC_REPORT.size()}, 2) == 35);
    }

    SECTION("Extract value at index") {
        auto idx = simd::build_index_scalar(
            std::span<const char>{EXEC_REPORT.data(), EXEC_REPORT.size()});

        // First field value is "FIX.4.4"
        REQUIRE(idx.value_at(
            std::span<const char>{EXEC_REPORT.data(), EXEC_REPORT.size()}, 0) == "FIX.4.4");

        // MsgType value is "8"
        REQUIRE(idx.value_at(
            std::span<const char>{EXEC_REPORT.data(), EXEC_REPORT.size()}, 2) == "8");
    }

    SECTION("Find tag by number") {
        auto idx = simd::build_index_scalar(
            std::span<const char>{EXEC_REPORT.data(), EXEC_REPORT.size()});

        size_t found = idx.find_tag(
            std::span<const char>{EXEC_REPORT.data(), EXEC_REPORT.size()}, 55);  // Symbol

        REQUIRE(found < idx.field_count());
        REQUIRE(idx.value_at(
            std::span<const char>{EXEC_REPORT.data(), EXEC_REPORT.size()}, found) == "AAPL");
    }
}

TEST_CASE("FIXStructuralIndex runtime dispatch", "[parser][simd][structural]") {
    // Initialize runtime dispatch
    simd::init_simd_dispatch();

    SECTION("Active implementation is detected") {
        auto impl = simd::active_simd_impl();
        INFO("Active SIMD implementation: " << simd::simd_impl_name(impl));

        // Should be at least scalar
        REQUIRE(impl >= simd::SimdImpl::Scalar);
    }

    SECTION("Build index via runtime dispatch") {
        auto idx = simd::build_index(
            std::span<const char>{EXEC_REPORT.data(), EXEC_REPORT.size()});

        REQUIRE(idx.valid());
        REQUIRE(idx.soh_count == 19);
        REQUIRE(idx.equals_count == 19);
    }
}

TEST_CASE("IndexedFieldAccessor", "[parser][simd][structural]") {
    auto idx = simd::build_index(
        std::span<const char>{EXEC_REPORT.data(), EXEC_REPORT.size()});

    simd::IndexedFieldAccessor accessor{idx,
        std::span<const char>{EXEC_REPORT.data(), EXEC_REPORT.size()}};

    SECTION("Field count") {
        REQUIRE(accessor.field_count() == 19);
    }

    SECTION("Get by tag") {
        REQUIRE(accessor.get(8) == "FIX.4.4");   // BeginString
        REQUIRE(accessor.get(35) == "8");        // MsgType (ExecutionReport)
        REQUIRE(accessor.get(55) == "AAPL");     // Symbol
        REQUIRE(accessor.get(37) == "ORDER123"); // OrderID
    }

    SECTION("Get as integer") {
        REQUIRE(accessor.get_int(9) == 176);   // BodyLength
        REQUIRE(accessor.get_int(34) == 1);    // MsgSeqNum
        REQUIRE(accessor.get_int(38) == 100);  // OrderQty
    }

    SECTION("Get as char") {
        REQUIRE(accessor.msg_type() == '8');   // ExecutionReport
        REQUIRE(accessor.get_char(54) == '1'); // Side = Buy
    }

    SECTION("Non-existent tag") {
        REQUIRE(accessor.get(999) == "");
        REQUIRE(accessor.get_int(999) == 0);
    }
}

TEST_CASE("PaddedMessageBuffer", "[parser][simd][structural]") {
    SECTION("Construction and set") {
        simd::MediumPaddedBuffer buffer;

        REQUIRE(buffer.empty());
        REQUIRE(buffer.capacity() == 1024);

        buffer.set(std::span<const char>{HEARTBEAT.data(), HEARTBEAT.size()});

        REQUIRE(!buffer.empty());
        REQUIRE(buffer.size() == HEARTBEAT.size());
    }

    SECTION("SIMD-safe pointer") {
        simd::MediumPaddedBuffer buffer;
        buffer.set(std::span<const char>{HEARTBEAT.data(), HEARTBEAT.size()});

        // Can safely read past end (padding is zeroed)
        const char* ptr = buffer.simd_safe_ptr();
        REQUIRE(ptr[buffer.size()] == '\0');     // First byte of padding
        REQUIRE(ptr[buffer.size() + 63] == '\0'); // Last byte of padding
    }

    SECTION("Build index from padded buffer") {
        simd::MediumPaddedBuffer buffer;
        buffer.set(std::span<const char>{EXEC_REPORT.data(), EXEC_REPORT.size()});

        auto idx = simd::build_index(buffer.data());

        REQUIRE(idx.valid());
        REQUIRE(idx.soh_count == 19);
    }
}
