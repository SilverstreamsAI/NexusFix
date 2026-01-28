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

    [[nodiscard]] constexpr std::string_view name(char type) noexcept {
        switch (type) {
            case Heartbeat:        return "Heartbeat";
            case TestRequest:      return "TestRequest";
            case ResendRequest:    return "ResendRequest";
            case Reject:           return "Reject";
            case SequenceReset:    return "SequenceReset";
            case Logout:           return "Logout";
            case Logon:            return "Logon";
            case NewOrderSingle:   return "NewOrderSingle";
            case OrderCancelRequest: return "OrderCancelRequest";
            case OrderCancelReplaceRequest: return "OrderCancelReplaceRequest";
            case OrderStatusRequest: return "OrderStatusRequest";
            case ExecutionReport:  return "ExecutionReport";
            case OrderCancelReject: return "OrderCancelReject";
            case MarketDataRequest: return "MarketDataRequest";
            case MarketDataSnapshotFullRefresh: return "MarketDataSnapshotFullRefresh";
            case MarketDataIncrementalRefresh: return "MarketDataIncrementalRefresh";
            case MarketDataRequestReject: return "MarketDataRequestReject";
            default:               return "Unknown";
        }
    }

    [[nodiscard]] constexpr bool is_admin(char type) noexcept {
        return type == Heartbeat ||
               type == TestRequest ||
               type == ResendRequest ||
               type == Reject ||
               type == SequenceReset ||
               type == Logout ||
               type == Logon;
    }

    [[nodiscard]] constexpr bool is_app(char type) noexcept {
        return !is_admin(type);
    }
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
