#pragma once

#include <array>
#include <concepts>
#include <span>
#include <string_view>
#include <cstdint>

#include "nexusfix/types/tag.hpp"
#include "nexusfix/types/field_types.hpp"
#include "nexusfix/types/error.hpp"

namespace nfx {

// ============================================================================
// FIX Message Type Constants
// ============================================================================

namespace msg_type {

// ============================================================================
// Message Type Constants
// ============================================================================

inline constexpr char Heartbeat        = '0';
inline constexpr char TestRequest      = '1';
inline constexpr char ResendRequest    = '2';
inline constexpr char Reject           = '3';
inline constexpr char SequenceReset    = '4';
inline constexpr char Logout           = '5';
inline constexpr char Logon            = 'A';
inline constexpr char NewOrderSingle   = 'D';
inline constexpr char OrderCancelRequest = 'F';
inline constexpr char OrderCancelReplaceRequest = 'G';
inline constexpr char OrderStatusRequest = 'H';
inline constexpr char ExecutionReport  = '8';
inline constexpr char OrderCancelReject = '9';
// Market Data Messages
inline constexpr char MarketDataRequest = 'V';
inline constexpr char MarketDataSnapshotFullRefresh = 'W';
inline constexpr char MarketDataIncrementalRefresh = 'X';
inline constexpr char MarketDataRequestReject = 'Y';

// ============================================================================
// Compile-time Message Type Info (TICKET_022)
// Single source of truth for message type metadata
// ============================================================================

namespace detail {

/// Compile-time message type metadata
template<char MsgType>
struct MsgTypeInfo {
    static constexpr std::string_view name = "Unknown";
    static constexpr bool is_admin = false;
    static constexpr bool is_valid = false;
};

// Admin/Session messages (is_admin = true)
template<> struct MsgTypeInfo<'0'> {  // Heartbeat
    static constexpr std::string_view name = "Heartbeat";
    static constexpr bool is_admin = true;
    static constexpr bool is_valid = true;
};

template<> struct MsgTypeInfo<'1'> {  // TestRequest
    static constexpr std::string_view name = "TestRequest";
    static constexpr bool is_admin = true;
    static constexpr bool is_valid = true;
};

template<> struct MsgTypeInfo<'2'> {  // ResendRequest
    static constexpr std::string_view name = "ResendRequest";
    static constexpr bool is_admin = true;
    static constexpr bool is_valid = true;
};

template<> struct MsgTypeInfo<'3'> {  // Reject
    static constexpr std::string_view name = "Reject";
    static constexpr bool is_admin = true;
    static constexpr bool is_valid = true;
};

template<> struct MsgTypeInfo<'4'> {  // SequenceReset
    static constexpr std::string_view name = "SequenceReset";
    static constexpr bool is_admin = true;
    static constexpr bool is_valid = true;
};

template<> struct MsgTypeInfo<'5'> {  // Logout
    static constexpr std::string_view name = "Logout";
    static constexpr bool is_admin = true;
    static constexpr bool is_valid = true;
};

template<> struct MsgTypeInfo<'A'> {  // Logon
    static constexpr std::string_view name = "Logon";
    static constexpr bool is_admin = true;
    static constexpr bool is_valid = true;
};

// Application messages (is_admin = false)
template<> struct MsgTypeInfo<'8'> {  // ExecutionReport
    static constexpr std::string_view name = "ExecutionReport";
    static constexpr bool is_admin = false;
    static constexpr bool is_valid = true;
};

template<> struct MsgTypeInfo<'9'> {  // OrderCancelReject
    static constexpr std::string_view name = "OrderCancelReject";
    static constexpr bool is_admin = false;
    static constexpr bool is_valid = true;
};

template<> struct MsgTypeInfo<'D'> {  // NewOrderSingle
    static constexpr std::string_view name = "NewOrderSingle";
    static constexpr bool is_admin = false;
    static constexpr bool is_valid = true;
};

template<> struct MsgTypeInfo<'F'> {  // OrderCancelRequest
    static constexpr std::string_view name = "OrderCancelRequest";
    static constexpr bool is_admin = false;
    static constexpr bool is_valid = true;
};

template<> struct MsgTypeInfo<'G'> {  // OrderCancelReplaceRequest
    static constexpr std::string_view name = "OrderCancelReplaceRequest";
    static constexpr bool is_admin = false;
    static constexpr bool is_valid = true;
};

template<> struct MsgTypeInfo<'H'> {  // OrderStatusRequest
    static constexpr std::string_view name = "OrderStatusRequest";
    static constexpr bool is_admin = false;
    static constexpr bool is_valid = true;
};

template<> struct MsgTypeInfo<'V'> {  // MarketDataRequest
    static constexpr std::string_view name = "MarketDataRequest";
    static constexpr bool is_admin = false;
    static constexpr bool is_valid = true;
};

template<> struct MsgTypeInfo<'W'> {  // MarketDataSnapshotFullRefresh
    static constexpr std::string_view name = "MarketDataSnapshotFullRefresh";
    static constexpr bool is_admin = false;
    static constexpr bool is_valid = true;
};

template<> struct MsgTypeInfo<'X'> {  // MarketDataIncrementalRefresh
    static constexpr std::string_view name = "MarketDataIncrementalRefresh";
    static constexpr bool is_admin = false;
    static constexpr bool is_valid = true;
};

template<> struct MsgTypeInfo<'Y'> {  // MarketDataRequestReject
    static constexpr std::string_view name = "MarketDataRequestReject";
    static constexpr bool is_admin = false;
    static constexpr bool is_valid = true;
};

// ============================================================================
// Compile-time lookup table generation
// ============================================================================

struct MsgTypeEntry {
    std::string_view name;
    bool is_admin;
    bool is_valid;
};

/// Generate lookup table at compile time
consteval std::array<MsgTypeEntry, 128> create_lookup_table() {
    std::array<MsgTypeEntry, 128> table{};

    // Initialize all entries as unknown/invalid
    for (auto& entry : table) {
        entry = {"Unknown", false, false};
    }

    // Populate known message types using template info
    table['0'] = {MsgTypeInfo<'0'>::name, MsgTypeInfo<'0'>::is_admin, true};
    table['1'] = {MsgTypeInfo<'1'>::name, MsgTypeInfo<'1'>::is_admin, true};
    table['2'] = {MsgTypeInfo<'2'>::name, MsgTypeInfo<'2'>::is_admin, true};
    table['3'] = {MsgTypeInfo<'3'>::name, MsgTypeInfo<'3'>::is_admin, true};
    table['4'] = {MsgTypeInfo<'4'>::name, MsgTypeInfo<'4'>::is_admin, true};
    table['5'] = {MsgTypeInfo<'5'>::name, MsgTypeInfo<'5'>::is_admin, true};
    table['8'] = {MsgTypeInfo<'8'>::name, MsgTypeInfo<'8'>::is_admin, true};
    table['9'] = {MsgTypeInfo<'9'>::name, MsgTypeInfo<'9'>::is_admin, true};
    table['A'] = {MsgTypeInfo<'A'>::name, MsgTypeInfo<'A'>::is_admin, true};
    table['D'] = {MsgTypeInfo<'D'>::name, MsgTypeInfo<'D'>::is_admin, true};
    table['F'] = {MsgTypeInfo<'F'>::name, MsgTypeInfo<'F'>::is_admin, true};
    table['G'] = {MsgTypeInfo<'G'>::name, MsgTypeInfo<'G'>::is_admin, true};
    table['H'] = {MsgTypeInfo<'H'>::name, MsgTypeInfo<'H'>::is_admin, true};
    table['V'] = {MsgTypeInfo<'V'>::name, MsgTypeInfo<'V'>::is_admin, true};
    table['W'] = {MsgTypeInfo<'W'>::name, MsgTypeInfo<'W'>::is_admin, true};
    table['X'] = {MsgTypeInfo<'X'>::name, MsgTypeInfo<'X'>::is_admin, true};
    table['Y'] = {MsgTypeInfo<'Y'>::name, MsgTypeInfo<'Y'>::is_admin, true};

    return table;
}

/// Compile-time generated lookup table (baked into binary)
inline constexpr auto LOOKUP_TABLE = create_lookup_table();

} // namespace detail

// ============================================================================
// Compile-time query (when MsgType is known at compile time)
// ============================================================================

template<char MsgType>
[[nodiscard]] consteval std::string_view name() noexcept {
    return detail::MsgTypeInfo<MsgType>::name;
}

template<char MsgType>
[[nodiscard]] consteval bool is_admin() noexcept {
    return detail::MsgTypeInfo<MsgType>::is_admin;
}

template<char MsgType>
[[nodiscard]] consteval bool is_app() noexcept {
    return !detail::MsgTypeInfo<MsgType>::is_admin;
}

template<char MsgType>
[[nodiscard]] consteval bool is_valid() noexcept {
    return detail::MsgTypeInfo<MsgType>::is_valid;
}

// ============================================================================
// Runtime query (when MsgType is runtime variable)
// Uses O(1) lookup table instead of switch statement
// ============================================================================

[[nodiscard]] inline constexpr std::string_view name(char type) noexcept {
    const auto idx = static_cast<unsigned char>(type);
    if (idx < detail::LOOKUP_TABLE.size()) [[likely]] {
        return detail::LOOKUP_TABLE[idx].name;
    }
    return "Unknown";
}

[[nodiscard]] inline constexpr bool is_admin(char type) noexcept {
    const auto idx = static_cast<unsigned char>(type);
    if (idx < detail::LOOKUP_TABLE.size()) [[likely]] {
        return detail::LOOKUP_TABLE[idx].is_admin;
    }
    return false;
}

[[nodiscard]] inline constexpr bool is_app(char type) noexcept {
    return !is_admin(type);
}

[[nodiscard]] inline constexpr bool is_valid(char type) noexcept {
    const auto idx = static_cast<unsigned char>(type);
    if (idx < detail::LOOKUP_TABLE.size()) [[likely]] {
        return detail::LOOKUP_TABLE[idx].is_valid;
    }
    return false;
}

// ============================================================================
// Compile-time verification (static_assert)
// ============================================================================

// Verify admin message types
static_assert(detail::MsgTypeInfo<Heartbeat>::is_admin == true);
static_assert(detail::MsgTypeInfo<TestRequest>::is_admin == true);
static_assert(detail::MsgTypeInfo<Logon>::is_admin == true);
static_assert(detail::MsgTypeInfo<Logout>::is_admin == true);

// Verify application message types
static_assert(detail::MsgTypeInfo<NewOrderSingle>::is_admin == false);
static_assert(detail::MsgTypeInfo<ExecutionReport>::is_admin == false);
static_assert(detail::MsgTypeInfo<MarketDataRequest>::is_admin == false);

// Verify name correctness
static_assert(detail::MsgTypeInfo<Heartbeat>::name == "Heartbeat");
static_assert(detail::MsgTypeInfo<Logon>::name == "Logon");
static_assert(detail::MsgTypeInfo<ExecutionReport>::name == "ExecutionReport");

// Verify lookup table matches template info
static_assert(detail::LOOKUP_TABLE['0'].name == "Heartbeat");
static_assert(detail::LOOKUP_TABLE['A'].name == "Logon");
static_assert(detail::LOOKUP_TABLE['8'].name == "ExecutionReport");
static_assert(detail::LOOKUP_TABLE['D'].is_admin == false);
static_assert(detail::LOOKUP_TABLE['5'].is_admin == true);

}

// ============================================================================
// Message Concept
// ============================================================================

/// Concept for FIX message types
template <typename T>
concept Message = requires(T msg, const T cmsg) {
    // Must have a message type
    { T::MSG_TYPE } -> std::convertible_to<char>;

    // Must be parseable from buffer
    { T::from_buffer(std::declval<std::span<const char>>()) }
        -> std::same_as<ParseResult<T>>;

    // Must provide raw buffer access
    { cmsg.raw() } -> std::convertible_to<std::span<const char>>;

    // Must provide header fields
    { cmsg.msg_seq_num() } -> std::convertible_to<uint32_t>;
    { cmsg.sender_comp_id() } -> std::convertible_to<std::string_view>;
    { cmsg.target_comp_id() } -> std::convertible_to<std::string_view>;
    { cmsg.sending_time() } -> std::convertible_to<std::string_view>;
};

/// Concept for messages that can be serialized
template <typename T>
concept SerializableMessage = Message<T> && requires(const T msg) {
    { msg.serialize() } -> std::convertible_to<std::span<const char>>;
    { msg.body_length() } -> std::convertible_to<size_t>;
};

/// Concept for order messages
template <typename T>
concept OrderMessage = Message<T> && requires(const T msg) {
    { msg.cl_ord_id() } -> std::convertible_to<std::string_view>;
    { msg.symbol() } -> std::convertible_to<std::string_view>;
    { msg.side() } -> std::convertible_to<Side>;
    { msg.order_qty() } -> std::convertible_to<Qty>;
};

/// Concept for execution messages
template <typename T>
concept ExecutionMessage = Message<T> && requires(const T msg) {
    { msg.order_id() } -> std::convertible_to<std::string_view>;
    { msg.exec_id() } -> std::convertible_to<std::string_view>;
    { msg.exec_type() } -> std::convertible_to<ExecType>;
    { msg.ord_status() } -> std::convertible_to<OrdStatus>;
};

// ============================================================================
// Message Header (common to all messages)
// ============================================================================

struct MessageHeader {
    std::string_view begin_string;   // Tag 8
    int body_length;                  // Tag 9
    char msg_type;                    // Tag 35
    std::string_view sender_comp_id; // Tag 49
    std::string_view target_comp_id; // Tag 56
    uint32_t msg_seq_num;            // Tag 34
    std::string_view sending_time;   // Tag 52
    bool poss_dup_flag;              // Tag 43
    bool poss_resend;                // Tag 97
    std::string_view orig_sending_time; // Tag 122

    constexpr MessageHeader() noexcept
        : begin_string{}
        , body_length{0}
        , msg_type{'\0'}
        , sender_comp_id{}
        , target_comp_id{}
        , msg_seq_num{0}
        , sending_time{}
        , poss_dup_flag{false}
        , poss_resend{false}
        , orig_sending_time{} {}

    // ========================================================================
    // Version Detection
    // ========================================================================

    /// Check if this is a FIXT 1.1 message (FIX 5.0+ transport)
    [[nodiscard]] constexpr bool is_fixt11() const noexcept {
        return begin_string == "FIXT.1.1";
    }

    /// Check if this is a FIX 4.x message
    [[nodiscard]] constexpr bool is_fix4() const noexcept {
        return begin_string.starts_with("FIX.4.");
    }

    /// Check if this is a FIX 4.4 message
    [[nodiscard]] constexpr bool is_fix44() const noexcept {
        return begin_string == "FIX.4.4";
    }

    /// Check if this is a FIX 4.2 message
    [[nodiscard]] constexpr bool is_fix42() const noexcept {
        return begin_string == "FIX.4.2";
    }

    /// Check if this is an admin/session message
    [[nodiscard]] constexpr bool is_admin_message() const noexcept {
        return msg_type::is_admin(msg_type);
    }

    /// Check if this is an application message
    [[nodiscard]] constexpr bool is_app_message() const noexcept {
        return msg_type::is_app(msg_type);
    }
};

// ============================================================================
// Message Trailer
// ============================================================================

struct MessageTrailer {
    std::string_view check_sum;  // Tag 10 (3-byte string)

    constexpr MessageTrailer() noexcept : check_sum{} {}
};

// ============================================================================
// FIX Protocol Constants
// ============================================================================

namespace fix {
    inline constexpr char SOH = '\001';  // Field delimiter
    inline constexpr char EQUALS = '=';  // Tag-value separator

    inline constexpr std::string_view FIX_4_4 = "FIX.4.4";
    inline constexpr std::string_view FIX_4_2 = "FIX.4.2";
    inline constexpr std::string_view FIX_4_0 = "FIX.4.0";
    inline constexpr std::string_view FIXT_1_1 = "FIXT.1.1";  // FIXT 1.1 transport layer

    inline constexpr size_t MAX_MESSAGE_SIZE = 65536;
    inline constexpr size_t MIN_MESSAGE_SIZE = 20;  // Minimal valid message
    inline constexpr size_t CHECKSUM_LENGTH = 3;

    /// Calculate FIX checksum
    [[nodiscard]] constexpr uint8_t calculate_checksum(std::span<const char> data) noexcept {
        uint32_t sum = 0;
        for (char c : data) {
            sum += static_cast<uint8_t>(c);
        }
        return static_cast<uint8_t>(sum % 256);
    }

    /// Format checksum as 3-digit string
    [[nodiscard]] constexpr std::array<char, 3> format_checksum(uint8_t checksum) noexcept {
        return {
            static_cast<char>('0' + (checksum / 100)),
            static_cast<char>('0' + ((checksum / 10) % 10)),
            static_cast<char>('0' + (checksum % 10))
        };
    }
}

} // namespace nfx
