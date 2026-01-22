// quickfix_compat.cpp
// NexusFIX QuickFIX Interoperability Test
// Validates message format compatibility with QuickFIX standard

#include <iostream>
#include <cassert>
#include <cstring>
#include <string>
#include <vector>

#include "nexusfix/nexusfix.hpp"

namespace nfx::test {

// ============================================================================
// Test Utilities
// ============================================================================

struct TestResult {
    const char* name;
    bool passed;
    std::string message;
};

std::vector<TestResult> test_results;

#define TEST(name) void test_##name()
#define RUN_TEST(name) run_test(#name, test_##name)
#define ASSERT_TRUE(cond) do { if (!(cond)) { throw std::runtime_error("Assertion failed: " #cond); } } while(0)
#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { throw std::runtime_error("Assertion failed: " #a " == " #b); } } while(0)
#define ASSERT_NE(a, b) do { if ((a) == (b)) { throw std::runtime_error("Assertion failed: " #a " != " #b); } } while(0)

void run_test(const char* name, void (*fn)()) {
    TestResult result{name, false, ""};
    try {
        fn();
        result.passed = true;
        std::cout << "[PASS] " << name << "\n";
    } catch (const std::exception& e) {
        result.message = e.what();
        std::cout << "[FAIL] " << name << ": " << e.what() << "\n";
    }
    test_results.push_back(result);
}

// ============================================================================
// QuickFIX Message Format Compatibility Tests
// ============================================================================

/// Test: Standard FIX 4.4 message header format
TEST(header_format) {
    // QuickFIX expects: 8=FIX.4.4|9=nnn|35=X|...
    constexpr std::string_view msg =
        "8=FIX.4.4\x01"
        "9=100\x01"
        "35=A\x01"
        "49=SENDER\x01"
        "56=TARGET\x01"
        "34=1\x01"
        "52=20240115-10:30:00.000\x01"
        "98=0\x01"
        "108=30\x01"
        "10=100\x01";

    IndexedParser parser;
    auto result = parser.parse(std::span<const char>{msg.data(), msg.size()});
    ASSERT_TRUE(result.has_value());

    auto& parsed = *result;

    // Verify header fields are in correct order
    auto begin_string = parsed.get_field(tag::BeginString::value);
    ASSERT_TRUE(begin_string.has_value());
    ASSERT_EQ(begin_string->as_string_view(), "FIX.4.4");

    auto body_length = parsed.get_field(tag::BodyLength::value);
    ASSERT_TRUE(body_length.has_value());
    ASSERT_EQ(body_length->as_int(), 100);

    auto msg_type = parsed.get_field(tag::MsgType::value);
    ASSERT_TRUE(msg_type.has_value());
    ASSERT_EQ(msg_type->as_char(), 'A');
}

/// Test: Logon message (35=A) format
TEST(logon_message_format) {
    constexpr std::string_view msg =
        "8=FIX.4.4\x01"
        "9=70\x01"
        "35=A\x01"
        "49=BANZAI\x01"
        "56=EXEC\x01"
        "34=1\x01"
        "52=20240115-10:30:00\x01"
        "98=0\x01"
        "108=30\x01"
        "10=000\x01";

    IndexedParser parser;
    auto result = parser.parse(std::span<const char>{msg.data(), msg.size()});
    ASSERT_TRUE(result.has_value());

    auto& parsed = *result;

    // Logon-specific fields
    auto encrypt_method = parsed.get_field(98);  // EncryptMethod
    ASSERT_TRUE(encrypt_method.has_value());
    ASSERT_EQ(encrypt_method->as_int(), 0);  // None

    auto heartbeat_int = parsed.get_field(108);  // HeartBtInt
    ASSERT_TRUE(heartbeat_int.has_value());
    ASSERT_EQ(heartbeat_int->as_int(), 30);
}

/// Test: Heartbeat message (35=0) format
TEST(heartbeat_message_format) {
    constexpr std::string_view msg =
        "8=FIX.4.4\x01"
        "9=55\x01"
        "35=0\x01"
        "49=BANZAI\x01"
        "56=EXEC\x01"
        "34=5\x01"
        "52=20240115-10:30:00\x01"
        "10=000\x01";

    IndexedParser parser;
    auto result = parser.parse(std::span<const char>{msg.data(), msg.size()});
    ASSERT_TRUE(result.has_value());

    auto msg_type = result->get_field(tag::MsgType::value);
    ASSERT_TRUE(msg_type.has_value());
    ASSERT_EQ(msg_type->as_char(), '0');
}

/// Test: Test Request message (35=1) format
TEST(test_request_message_format) {
    constexpr std::string_view msg =
        "8=FIX.4.4\x01"
        "9=65\x01"
        "35=1\x01"
        "49=BANZAI\x01"
        "56=EXEC\x01"
        "34=10\x01"
        "52=20240115-10:30:00\x01"
        "112=TEST123\x01"
        "10=000\x01";

    IndexedParser parser;
    auto result = parser.parse(std::span<const char>{msg.data(), msg.size()});
    ASSERT_TRUE(result.has_value());

    auto test_req_id = result->get_field(112);  // TestReqID
    ASSERT_TRUE(test_req_id.has_value());
    ASSERT_EQ(test_req_id->as_string_view(), "TEST123");
}

/// Test: NewOrderSingle message (35=D) format
TEST(new_order_single_format) {
    constexpr std::string_view msg =
        "8=FIX.4.4\x01"
        "9=140\x01"
        "35=D\x01"
        "49=BANZAI\x01"
        "56=EXEC\x01"
        "34=3\x01"
        "52=20240115-10:30:00\x01"
        "11=ORDER001\x01"
        "21=1\x01"
        "55=MSFT\x01"
        "54=1\x01"
        "60=20240115-10:30:00\x01"
        "38=100\x01"
        "40=2\x01"
        "44=150.50\x01"
        "59=0\x01"
        "10=000\x01";

    IndexedParser parser;
    auto result = parser.parse(std::span<const char>{msg.data(), msg.size()});
    ASSERT_TRUE(result.has_value());

    auto& parsed = *result;

    // NewOrderSingle fields
    auto cl_ord_id = parsed.get_field(11);  // ClOrdID
    ASSERT_TRUE(cl_ord_id.has_value());
    ASSERT_EQ(cl_ord_id->as_string_view(), "ORDER001");

    auto symbol = parsed.get_field(55);  // Symbol
    ASSERT_TRUE(symbol.has_value());
    ASSERT_EQ(symbol->as_string_view(), "MSFT");

    auto side = parsed.get_field(54);  // Side
    ASSERT_TRUE(side.has_value());
    ASSERT_EQ(side->as_char(), '1');  // Buy

    auto order_qty = parsed.get_field(38);  // OrderQty
    ASSERT_TRUE(order_qty.has_value());
    ASSERT_EQ(order_qty->as_int(), 100);

    auto ord_type = parsed.get_field(40);  // OrdType
    ASSERT_TRUE(ord_type.has_value());
    ASSERT_EQ(ord_type->as_char(), '2');  // Limit

    auto price = parsed.get_field(44);  // Price
    ASSERT_TRUE(price.has_value());
    // Price is 150.50
}

/// Test: ExecutionReport message (35=8) format
TEST(execution_report_format) {
    constexpr std::string_view msg =
        "8=FIX.4.4\x01"
        "9=180\x01"
        "35=8\x01"
        "49=EXEC\x01"
        "56=BANZAI\x01"
        "34=5\x01"
        "52=20240115-10:30:00\x01"
        "37=ORD123\x01"
        "17=EXEC456\x01"
        "150=0\x01"
        "39=0\x01"
        "11=ORDER001\x01"
        "55=MSFT\x01"
        "54=1\x01"
        "38=100\x01"
        "151=100\x01"
        "14=0\x01"
        "6=0\x01"
        "10=000\x01";

    IndexedParser parser;
    auto result = parser.parse(std::span<const char>{msg.data(), msg.size()});
    ASSERT_TRUE(result.has_value());

    auto& parsed = *result;

    // ExecutionReport fields
    auto order_id = parsed.get_field(37);  // OrderID
    ASSERT_TRUE(order_id.has_value());
    ASSERT_EQ(order_id->as_string_view(), "ORD123");

    auto exec_id = parsed.get_field(17);  // ExecID
    ASSERT_TRUE(exec_id.has_value());
    ASSERT_EQ(exec_id->as_string_view(), "EXEC456");

    auto exec_type = parsed.get_field(150);  // ExecType
    ASSERT_TRUE(exec_type.has_value());
    ASSERT_EQ(exec_type->as_char(), '0');  // New

    auto ord_status = parsed.get_field(39);  // OrdStatus
    ASSERT_TRUE(ord_status.has_value());
    ASSERT_EQ(ord_status->as_char(), '0');  // New

    auto leaves_qty = parsed.get_field(151);  // LeavesQty
    ASSERT_TRUE(leaves_qty.has_value());
    ASSERT_EQ(leaves_qty->as_int(), 100);

    auto cum_qty = parsed.get_field(14);  // CumQty
    ASSERT_TRUE(cum_qty.has_value());
    ASSERT_EQ(cum_qty->as_int(), 0);
}

/// Test: OrderCancelRequest message (35=F) format
TEST(order_cancel_request_format) {
    constexpr std::string_view msg =
        "8=FIX.4.4\x01"
        "9=130\x01"
        "35=F\x01"
        "49=BANZAI\x01"
        "56=EXEC\x01"
        "34=7\x01"
        "52=20240115-10:30:00\x01"
        "41=ORDER001\x01"
        "11=CANCEL001\x01"
        "55=MSFT\x01"
        "54=1\x01"
        "60=20240115-10:30:00\x01"
        "38=100\x01"
        "10=000\x01";

    IndexedParser parser;
    auto result = parser.parse(std::span<const char>{msg.data(), msg.size()});
    ASSERT_TRUE(result.has_value());

    auto& parsed = *result;

    auto orig_cl_ord_id = parsed.get_field(41);  // OrigClOrdID
    ASSERT_TRUE(orig_cl_ord_id.has_value());
    ASSERT_EQ(orig_cl_ord_id->as_string_view(), "ORDER001");

    auto cl_ord_id = parsed.get_field(11);  // ClOrdID
    ASSERT_TRUE(cl_ord_id.has_value());
    ASSERT_EQ(cl_ord_id->as_string_view(), "CANCEL001");
}

/// Test: Logout message (35=5) format
TEST(logout_message_format) {
    constexpr std::string_view msg =
        "8=FIX.4.4\x01"
        "9=70\x01"
        "35=5\x01"
        "49=BANZAI\x01"
        "56=EXEC\x01"
        "34=100\x01"
        "52=20240115-10:30:00\x01"
        "58=Normal logout\x01"
        "10=000\x01";

    IndexedParser parser;
    auto result = parser.parse(std::span<const char>{msg.data(), msg.size()});
    ASSERT_TRUE(result.has_value());

    auto text = result->get_field(58);  // Text
    ASSERT_TRUE(text.has_value());
    ASSERT_EQ(text->as_string_view(), "Normal logout");
}

/// Test: Reject message (35=3) format
TEST(reject_message_format) {
    constexpr std::string_view msg =
        "8=FIX.4.4\x01"
        "9=90\x01"
        "35=3\x01"
        "49=EXEC\x01"
        "56=BANZAI\x01"
        "34=10\x01"
        "52=20240115-10:30:00\x01"
        "45=5\x01"
        "373=1\x01"
        "58=Invalid tag number\x01"
        "10=000\x01";

    IndexedParser parser;
    auto result = parser.parse(std::span<const char>{msg.data(), msg.size()});
    ASSERT_TRUE(result.has_value());

    auto ref_seq_num = result->get_field(45);  // RefSeqNum
    ASSERT_TRUE(ref_seq_num.has_value());
    ASSERT_EQ(ref_seq_num->as_int(), 5);

    auto session_reject_reason = result->get_field(373);  // SessionRejectReason
    ASSERT_TRUE(session_reject_reason.has_value());
    ASSERT_EQ(session_reject_reason->as_int(), 1);  // Invalid tag number
}

/// Test: ResendRequest message (35=2) format
TEST(resend_request_format) {
    constexpr std::string_view msg =
        "8=FIX.4.4\x01"
        "9=65\x01"
        "35=2\x01"
        "49=BANZAI\x01"
        "56=EXEC\x01"
        "34=8\x01"
        "52=20240115-10:30:00\x01"
        "7=1\x01"
        "16=0\x01"
        "10=000\x01";

    IndexedParser parser;
    auto result = parser.parse(std::span<const char>{msg.data(), msg.size()});
    ASSERT_TRUE(result.has_value());

    auto begin_seq_no = result->get_field(7);  // BeginSeqNo
    ASSERT_TRUE(begin_seq_no.has_value());
    ASSERT_EQ(begin_seq_no->as_int(), 1);

    auto end_seq_no = result->get_field(16);  // EndSeqNo
    ASSERT_TRUE(end_seq_no.has_value());
    ASSERT_EQ(end_seq_no->as_int(), 0);  // 0 means infinity
}

/// Test: SequenceReset message (35=4) format
TEST(sequence_reset_format) {
    constexpr std::string_view msg =
        "8=FIX.4.4\x01"
        "9=70\x01"
        "35=4\x01"
        "49=EXEC\x01"
        "56=BANZAI\x01"
        "34=5\x01"
        "52=20240115-10:30:00\x01"
        "123=Y\x01"
        "36=10\x01"
        "10=000\x01";

    IndexedParser parser;
    auto result = parser.parse(std::span<const char>{msg.data(), msg.size()});
    ASSERT_TRUE(result.has_value());

    auto gap_fill_flag = result->get_field(123);  // GapFillFlag
    ASSERT_TRUE(gap_fill_flag.has_value());
    ASSERT_EQ(gap_fill_flag->as_char(), 'Y');

    auto new_seq_no = result->get_field(36);  // NewSeqNo
    ASSERT_TRUE(new_seq_no.has_value());
    ASSERT_EQ(new_seq_no->as_int(), 10);
}

// ============================================================================
// Checksum Compatibility Tests
// ============================================================================

/// Test: Checksum calculation matches QuickFIX
TEST(checksum_calculation) {
    // This message has a known checksum
    std::string msg =
        "8=FIX.4.4\x01"
        "9=5\x01"
        "35=0\x01";

    uint8_t checksum = fix::calculate_checksum(
        std::span<const char>{msg.data(), msg.size()});

    // Verify checksum is calculated correctly
    // Sum of all bytes mod 256
    uint32_t sum = 0;
    for (char c : msg) {
        sum += static_cast<unsigned char>(c);
    }
    ASSERT_EQ(checksum, static_cast<uint8_t>(sum % 256));
}

/// Test: Checksum format is 3-digit zero-padded
TEST(checksum_format) {
    auto formatted = fix::format_checksum(5);
    ASSERT_EQ(formatted[0], '0');
    ASSERT_EQ(formatted[1], '0');
    ASSERT_EQ(formatted[2], '5');

    formatted = fix::format_checksum(123);
    ASSERT_EQ(formatted[0], '1');
    ASSERT_EQ(formatted[1], '2');
    ASSERT_EQ(formatted[2], '3');

    formatted = fix::format_checksum(255);
    ASSERT_EQ(formatted[0], '2');
    ASSERT_EQ(formatted[1], '5');
    ASSERT_EQ(formatted[2], '5');
}

// ============================================================================
// Message Building Compatibility Tests
// ============================================================================

/// Test: Built messages can be parsed
TEST(message_roundtrip) {
    MessageAssembler assembler;

    auto msg = assembler.start()
        .field(tag::MsgType::value, "0")
        .field(tag::SenderCompID::value, "SENDER")
        .field(tag::TargetCompID::value, "TARGET")
        .field(tag::MsgSeqNum::value, int64_t{1})
        .field(tag::SendingTime::value, "20240115-10:30:00.000")
        .finish();

    IndexedParser parser;
    auto result = parser.parse(msg);
    ASSERT_TRUE(result.has_value());

    auto msg_type = result->get_field(tag::MsgType::value);
    ASSERT_TRUE(msg_type.has_value());
    ASSERT_EQ(msg_type->as_char(), '0');

    auto sender = result->get_field(tag::SenderCompID::value);
    ASSERT_TRUE(sender.has_value());
    ASSERT_EQ(sender->as_string_view(), "SENDER");
}

/// Test: Built Logon message format
TEST(build_logon_message) {
    fix44::Logon::Builder builder;
    MessageAssembler assembler;

    auto msg = builder
        .sender_comp_id("BANZAI")
        .target_comp_id("EXEC")
        .msg_seq_num(1)
        .sending_time("20240115-10:30:00.000")
        .encrypt_method(0)
        .heart_bt_int(30)
        .build(assembler);

    auto result = IndexedParser::parse(msg);
    ASSERT_TRUE(result.has_value());

    char msg_type = result->msg_type();
    ASSERT_EQ(msg_type, 'A');
}

/// Test: Built Heartbeat message format
TEST(build_heartbeat_message) {
    fix44::Heartbeat::Builder builder;
    MessageAssembler assembler;

    auto msg = builder
        .sender_comp_id("SENDER")
        .target_comp_id("TARGET")
        .msg_seq_num(5)
        .sending_time("20240115-10:30:00.000")
        .build(assembler);

    auto result = IndexedParser::parse(msg);
    ASSERT_TRUE(result.has_value());

    char msg_type = result->msg_type();
    ASSERT_EQ(msg_type, '0');
}

// ============================================================================
// Sequence Number Compatibility Tests
// ============================================================================

/// Test: Sequence numbers start at 1
TEST(sequence_starts_at_one) {
    SequenceManager seq_mgr;
    ASSERT_EQ(seq_mgr.current_outbound(), 1u);
    ASSERT_EQ(seq_mgr.expected_inbound(), 1u);
}

/// Test: Sequence increments correctly
TEST(sequence_increment) {
    SequenceManager seq_mgr;

    uint32_t seq1 = seq_mgr.next_outbound();
    uint32_t seq2 = seq_mgr.next_outbound();
    uint32_t seq3 = seq_mgr.next_outbound();

    ASSERT_EQ(seq1, 1u);
    ASSERT_EQ(seq2, 2u);
    ASSERT_EQ(seq3, 3u);
}

/// Test: Sequence gap detection
TEST(sequence_gap_detection) {
    SequenceManager seq_mgr;

    // First message OK
    auto result1 = seq_mgr.validate_inbound(1);
    ASSERT_EQ(result1, SequenceManager::SequenceResult::Ok);

    // Gap detected (missing 2, 3, 4)
    auto result2 = seq_mgr.validate_inbound(5);
    ASSERT_EQ(result2, SequenceManager::SequenceResult::GapDetected);

    // Too low (duplicate)
    auto result3 = seq_mgr.validate_inbound(1);
    ASSERT_EQ(result3, SequenceManager::SequenceResult::TooLow);
}

/// Test: Sequence reset
TEST(sequence_reset) {
    SequenceManager seq_mgr;

    seq_mgr.next_outbound();
    seq_mgr.next_outbound();
    seq_mgr.validate_inbound(1);
    seq_mgr.validate_inbound(2);

    seq_mgr.reset();

    ASSERT_EQ(seq_mgr.current_outbound(), 1u);
    ASSERT_EQ(seq_mgr.expected_inbound(), 1u);
}

// ============================================================================
// Field Type Compatibility Tests
// ============================================================================

/// Test: Side values match QuickFIX
TEST(side_values) {
    ASSERT_EQ(static_cast<char>(Side::Buy), '1');
    ASSERT_EQ(static_cast<char>(Side::Sell), '2');
    ASSERT_EQ(static_cast<char>(Side::SellShort), '5');
}

/// Test: OrdType values match QuickFIX
TEST(ord_type_values) {
    ASSERT_EQ(static_cast<char>(OrdType::Market), '1');
    ASSERT_EQ(static_cast<char>(OrdType::Limit), '2');
    ASSERT_EQ(static_cast<char>(OrdType::Stop), '3');
    ASSERT_EQ(static_cast<char>(OrdType::StopLimit), '4');
}

/// Test: ExecType values match QuickFIX
TEST(exec_type_values) {
    ASSERT_EQ(static_cast<char>(ExecType::New), '0');
    ASSERT_EQ(static_cast<char>(ExecType::PartialFill), '1');
    ASSERT_EQ(static_cast<char>(ExecType::Fill), '2');
    ASSERT_EQ(static_cast<char>(ExecType::Canceled), '4');
    ASSERT_EQ(static_cast<char>(ExecType::Rejected), '8');
}

/// Test: OrdStatus values match QuickFIX
TEST(ord_status_values) {
    ASSERT_EQ(static_cast<char>(OrdStatus::New), '0');
    ASSERT_EQ(static_cast<char>(OrdStatus::PartiallyFilled), '1');
    ASSERT_EQ(static_cast<char>(OrdStatus::Filled), '2');
    ASSERT_EQ(static_cast<char>(OrdStatus::Canceled), '4');
    ASSERT_EQ(static_cast<char>(OrdStatus::Rejected), '8');
}

/// Test: TimeInForce values match QuickFIX
TEST(time_in_force_values) {
    ASSERT_EQ(static_cast<char>(TimeInForce::Day), '0');
    ASSERT_EQ(static_cast<char>(TimeInForce::GTC), '1');
    ASSERT_EQ(static_cast<char>(TimeInForce::IOC), '3');
    ASSERT_EQ(static_cast<char>(TimeInForce::FOK), '4');
}

// ============================================================================
// Stream Parsing Compatibility Tests
// ============================================================================

/// Test: Parse multiple messages from stream
TEST(stream_multi_message) {
    std::string stream;
    stream +=
        "8=FIX.4.4\x01"
        "9=55\x01"
        "35=0\x01"
        "49=A\x01"
        "56=B\x01"
        "34=1\x01"
        "52=20240115-10:30:00\x01"
        "10=000\x01";
    stream +=
        "8=FIX.4.4\x01"
        "9=55\x01"
        "35=0\x01"
        "49=A\x01"
        "56=B\x01"
        "34=2\x01"
        "52=20240115-10:30:01\x01"
        "10=000\x01";

    StreamParser parser;
    parser.feed(std::span<const char>{stream.data(), stream.size()});

    ASSERT_TRUE(parser.has_message());
    auto [start1, end1] = parser.next_message();
    ASSERT_TRUE(end1 > start1);

    ASSERT_TRUE(parser.has_message());
    auto [start2, end2] = parser.next_message();
    ASSERT_TRUE(end2 > start2);

    ASSERT_FALSE(parser.has_message());

    // Verify sequence numbers
    std::span<const char> msg1{stream.data() + start1, end1 - start1};
    std::span<const char> msg2{stream.data() + start2, end2 - start2};

    auto parsed1 = IndexedParser::parse(msg1);
    ASSERT_TRUE(parsed1.has_value());
    ASSERT_EQ(parsed1->msg_seq_num(), 1u);

    auto parsed2 = IndexedParser::parse(msg2);
    ASSERT_TRUE(parsed2.has_value());
    ASSERT_EQ(parsed2->msg_seq_num(), 2u);
}

/// Test: Handle partial messages in stream
TEST(stream_partial_message) {
    // Note: StreamParser works on message boundaries, not buffering
    // This test verifies that it correctly identifies complete messages

    std::string complete_msg =
        "8=FIX.4.4\x01"
        "9=55\x01"
        "35=0\x01"
        "49=A\x01"
        "56=B\x01"
        "34=1\x01"
        "52=20240115-10:30:00\x01"
        "10=000\x01";

    StreamParser parser;
    parser.feed(std::span<const char>{complete_msg.data(), complete_msg.size()});

    ASSERT_TRUE(parser.has_message());
    auto [start, end] = parser.next_message();
    ASSERT_TRUE(end > start);

    // No more messages
    ASSERT_FALSE(parser.has_message());
}

} // namespace nfx::test

// ============================================================================
// Main
// ============================================================================

int main() {
    using namespace nfx::test;

    std::cout << "NexusFIX QuickFIX Compatibility Tests\n";
    std::cout << "======================================\n\n";

    // Header/message format tests
    RUN_TEST(header_format);
    RUN_TEST(logon_message_format);
    RUN_TEST(heartbeat_message_format);
    RUN_TEST(test_request_message_format);
    RUN_TEST(new_order_single_format);
    RUN_TEST(execution_report_format);
    RUN_TEST(order_cancel_request_format);
    RUN_TEST(logout_message_format);
    RUN_TEST(reject_message_format);
    RUN_TEST(resend_request_format);
    RUN_TEST(sequence_reset_format);

    // Checksum tests
    RUN_TEST(checksum_calculation);
    RUN_TEST(checksum_format);

    // Message building tests
    RUN_TEST(message_roundtrip);
    RUN_TEST(build_logon_message);
    RUN_TEST(build_heartbeat_message);

    // Sequence number tests
    RUN_TEST(sequence_starts_at_one);
    RUN_TEST(sequence_increment);
    RUN_TEST(sequence_gap_detection);
    RUN_TEST(sequence_reset);

    // Field type tests
    RUN_TEST(side_values);
    RUN_TEST(ord_type_values);
    RUN_TEST(exec_type_values);
    RUN_TEST(ord_status_values);
    RUN_TEST(time_in_force_values);

    // Stream parsing tests
    RUN_TEST(stream_multi_message);
    RUN_TEST(stream_partial_message);

    // Summary
    std::cout << "\n======================================\n";
    size_t passed = 0, failed = 0;
    for (const auto& result : test_results) {
        if (result.passed) ++passed;
        else ++failed;
    }
    std::cout << "Passed: " << passed << "/" << test_results.size() << "\n";
    std::cout << "Failed: " << failed << "/" << test_results.size() << "\n";

    return failed > 0 ? 1 : 0;
}
