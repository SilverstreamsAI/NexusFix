#pragma once

#include <array>
#include <climits>
#include <cstdint>
#include <expected>
#include <optional>
#include <source_location>
#include <string_view>

namespace nfx {

// ============================================================================
// Error Categories
// ============================================================================

enum class ErrorCategory : uint8_t {
    None = 0,
    Parse,
    Session,
    Transport,
    Validation,
    Internal
};

// ============================================================================
// Parse Errors (zero-allocation, deterministic)
// ============================================================================

enum class ParseErrorCode : uint8_t {
    None = 0,
    BufferTooShort,
    InvalidBeginString,
    InvalidBodyLength,
    InvalidChecksum,
    MissingRequiredField,
    InvalidFieldFormat,
    InvalidTagNumber,
    DuplicateTag,
    UnterminatedField,
    InvalidMsgType,
    GarbledMessage
};

inline constexpr size_t PARSE_ERROR_COUNT = 12;

// ============================================================================
// Compile-time ParseError Info (TICKET_023)
// ============================================================================

namespace detail {

template<ParseErrorCode Code>
struct ParseErrorInfo {
    static constexpr std::string_view message = "Unknown error";
};

template<> struct ParseErrorInfo<ParseErrorCode::None> {
    static constexpr std::string_view message = "No error";
};

template<> struct ParseErrorInfo<ParseErrorCode::BufferTooShort> {
    static constexpr std::string_view message = "Buffer too short";
};

template<> struct ParseErrorInfo<ParseErrorCode::InvalidBeginString> {
    static constexpr std::string_view message = "Invalid BeginString";
};

template<> struct ParseErrorInfo<ParseErrorCode::InvalidBodyLength> {
    static constexpr std::string_view message = "Invalid BodyLength";
};

template<> struct ParseErrorInfo<ParseErrorCode::InvalidChecksum> {
    static constexpr std::string_view message = "Invalid CheckSum";
};

template<> struct ParseErrorInfo<ParseErrorCode::MissingRequiredField> {
    static constexpr std::string_view message = "Missing required field";
};

template<> struct ParseErrorInfo<ParseErrorCode::InvalidFieldFormat> {
    static constexpr std::string_view message = "Invalid field format";
};

template<> struct ParseErrorInfo<ParseErrorCode::InvalidTagNumber> {
    static constexpr std::string_view message = "Invalid tag number";
};

template<> struct ParseErrorInfo<ParseErrorCode::DuplicateTag> {
    static constexpr std::string_view message = "Duplicate tag";
};

template<> struct ParseErrorInfo<ParseErrorCode::UnterminatedField> {
    static constexpr std::string_view message = "Unterminated field";
};

template<> struct ParseErrorInfo<ParseErrorCode::InvalidMsgType> {
    static constexpr std::string_view message = "Invalid MsgType";
};

template<> struct ParseErrorInfo<ParseErrorCode::GarbledMessage> {
    static constexpr std::string_view message = "Garbled message";
};

/// Generate ParseError lookup table at compile time
consteval std::array<std::string_view, PARSE_ERROR_COUNT> create_parse_error_table() {
    std::array<std::string_view, PARSE_ERROR_COUNT> table{};
    table[0]  = ParseErrorInfo<ParseErrorCode::None>::message;
    table[1]  = ParseErrorInfo<ParseErrorCode::BufferTooShort>::message;
    table[2]  = ParseErrorInfo<ParseErrorCode::InvalidBeginString>::message;
    table[3]  = ParseErrorInfo<ParseErrorCode::InvalidBodyLength>::message;
    table[4]  = ParseErrorInfo<ParseErrorCode::InvalidChecksum>::message;
    table[5]  = ParseErrorInfo<ParseErrorCode::MissingRequiredField>::message;
    table[6]  = ParseErrorInfo<ParseErrorCode::InvalidFieldFormat>::message;
    table[7]  = ParseErrorInfo<ParseErrorCode::InvalidTagNumber>::message;
    table[8]  = ParseErrorInfo<ParseErrorCode::DuplicateTag>::message;
    table[9]  = ParseErrorInfo<ParseErrorCode::UnterminatedField>::message;
    table[10] = ParseErrorInfo<ParseErrorCode::InvalidMsgType>::message;
    table[11] = ParseErrorInfo<ParseErrorCode::GarbledMessage>::message;
    return table;
}

inline constexpr auto PARSE_ERROR_TABLE = create_parse_error_table();

} // namespace detail

/// Compile-time query (when code is known at compile time)
template<ParseErrorCode Code>
[[nodiscard]] consteval std::string_view parse_error_message() noexcept {
    return detail::ParseErrorInfo<Code>::message;
}

/// Runtime query using O(1) lookup table
[[nodiscard]] inline constexpr std::string_view parse_error_message(ParseErrorCode code) noexcept {
    const auto idx = static_cast<uint8_t>(code);
    if (idx < detail::PARSE_ERROR_TABLE.size()) [[likely]] {
        return detail::PARSE_ERROR_TABLE[idx];
    }
    return "Unknown error";
}

// Static assertions for ParseError
static_assert(detail::ParseErrorInfo<ParseErrorCode::None>::message == "No error");
static_assert(detail::ParseErrorInfo<ParseErrorCode::InvalidChecksum>::message == "Invalid CheckSum");
static_assert(detail::PARSE_ERROR_TABLE[0] == "No error");
static_assert(detail::PARSE_ERROR_TABLE[4] == "Invalid CheckSum");

struct ParseError {
    ParseErrorCode code;
    int tag;           // Offending tag (0 if N/A)
    size_t offset;     // Byte offset in buffer where error occurred

    constexpr ParseError() noexcept
        : code{ParseErrorCode::None}, tag{0}, offset{0} {}

    constexpr ParseError(ParseErrorCode c) noexcept
        : code{c}, tag{0}, offset{0} {}

    constexpr ParseError(ParseErrorCode c, int t) noexcept
        : code{c}, tag{t}, offset{0} {}

    constexpr ParseError(ParseErrorCode c, int t, size_t off) noexcept
        : code{c}, tag{t}, offset{off} {}

    [[nodiscard]] constexpr bool ok() const noexcept {
        return code == ParseErrorCode::None;
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return !ok();
    }

    [[nodiscard]] constexpr std::string_view message() const noexcept {
        return parse_error_message(code);
    }
};

// ============================================================================
// Session Errors
// ============================================================================

enum class SessionErrorCode : uint8_t {
    None = 0,
    NotConnected,
    AlreadyConnected,
    LogonRejected,
    LogonTimeout,
    HeartbeatTimeout,
    SequenceGap,
    InvalidState,
    Disconnected
};

inline constexpr size_t SESSION_ERROR_COUNT = 9;

// ============================================================================
// Compile-time SessionError Info (TICKET_023)
// ============================================================================

namespace detail {

template<SessionErrorCode Code>
struct SessionErrorInfo {
    static constexpr std::string_view message = "Unknown error";
};

template<> struct SessionErrorInfo<SessionErrorCode::None> {
    static constexpr std::string_view message = "No error";
};

template<> struct SessionErrorInfo<SessionErrorCode::NotConnected> {
    static constexpr std::string_view message = "Not connected";
};

template<> struct SessionErrorInfo<SessionErrorCode::AlreadyConnected> {
    static constexpr std::string_view message = "Already connected";
};

template<> struct SessionErrorInfo<SessionErrorCode::LogonRejected> {
    static constexpr std::string_view message = "Logon rejected";
};

template<> struct SessionErrorInfo<SessionErrorCode::LogonTimeout> {
    static constexpr std::string_view message = "Logon timeout";
};

template<> struct SessionErrorInfo<SessionErrorCode::HeartbeatTimeout> {
    static constexpr std::string_view message = "Heartbeat timeout";
};

template<> struct SessionErrorInfo<SessionErrorCode::SequenceGap> {
    static constexpr std::string_view message = "Sequence gap detected";
};

template<> struct SessionErrorInfo<SessionErrorCode::InvalidState> {
    static constexpr std::string_view message = "Invalid session state";
};

template<> struct SessionErrorInfo<SessionErrorCode::Disconnected> {
    static constexpr std::string_view message = "Disconnected";
};

/// Generate SessionError lookup table at compile time
consteval std::array<std::string_view, SESSION_ERROR_COUNT> create_session_error_table() {
    std::array<std::string_view, SESSION_ERROR_COUNT> table{};
    table[0] = SessionErrorInfo<SessionErrorCode::None>::message;
    table[1] = SessionErrorInfo<SessionErrorCode::NotConnected>::message;
    table[2] = SessionErrorInfo<SessionErrorCode::AlreadyConnected>::message;
    table[3] = SessionErrorInfo<SessionErrorCode::LogonRejected>::message;
    table[4] = SessionErrorInfo<SessionErrorCode::LogonTimeout>::message;
    table[5] = SessionErrorInfo<SessionErrorCode::HeartbeatTimeout>::message;
    table[6] = SessionErrorInfo<SessionErrorCode::SequenceGap>::message;
    table[7] = SessionErrorInfo<SessionErrorCode::InvalidState>::message;
    table[8] = SessionErrorInfo<SessionErrorCode::Disconnected>::message;
    return table;
}

inline constexpr auto SESSION_ERROR_TABLE = create_session_error_table();

} // namespace detail

/// Compile-time query (when code is known at compile time)
template<SessionErrorCode Code>
[[nodiscard]] consteval std::string_view session_error_message() noexcept {
    return detail::SessionErrorInfo<Code>::message;
}

/// Runtime query using O(1) lookup table
[[nodiscard]] inline constexpr std::string_view session_error_message(SessionErrorCode code) noexcept {
    const auto idx = static_cast<uint8_t>(code);
    if (idx < detail::SESSION_ERROR_TABLE.size()) [[likely]] {
        return detail::SESSION_ERROR_TABLE[idx];
    }
    return "Unknown error";
}

// Static assertions for SessionError
static_assert(detail::SessionErrorInfo<SessionErrorCode::None>::message == "No error");
static_assert(detail::SessionErrorInfo<SessionErrorCode::LogonTimeout>::message == "Logon timeout");
static_assert(detail::SESSION_ERROR_TABLE[0] == "No error");
static_assert(detail::SESSION_ERROR_TABLE[4] == "Logon timeout");

struct SessionError {
    SessionErrorCode code;
    uint32_t expected_seq;  // For sequence errors
    uint32_t received_seq;

    constexpr SessionError() noexcept
        : code{SessionErrorCode::None}, expected_seq{0}, received_seq{0} {}

    constexpr SessionError(SessionErrorCode c) noexcept
        : code{c}, expected_seq{0}, received_seq{0} {}

    constexpr SessionError(SessionErrorCode c, uint32_t exp, uint32_t recv) noexcept
        : code{c}, expected_seq{exp}, received_seq{recv} {}

    [[nodiscard]] constexpr bool ok() const noexcept {
        return code == SessionErrorCode::None;
    }

    [[nodiscard]] constexpr std::string_view message() const noexcept {
        return session_error_message(code);
    }
};

// ============================================================================
// Transport Errors
// ============================================================================

enum class TransportErrorCode : uint8_t {
    None = 0,

    // Connection errors
    ConnectionFailed,
    ConnectionClosed,
    ConnectionRefused,
    ConnectionReset,
    ConnectionAborted,

    // I/O errors
    ReadError,
    WriteError,
    Timeout,

    // Address errors
    AddressResolutionFailed,
    NetworkUnreachable,
    HostUnreachable,

    // Socket state errors
    SocketError,
    WouldBlock,
    InProgress,
    NotConnected,
    NoBufferSpace,

    // Platform-specific errors
    WinsockInitFailed,    // WSAStartup failed (Windows)
    IocpError,            // IOCP operation failed (Windows)
    KqueueError           // kqueue operation failed (macOS)
};

inline constexpr size_t TRANSPORT_ERROR_COUNT = 20;

// ============================================================================
// Compile-time TransportError Info (TICKET_023)
// ============================================================================

namespace detail {

template<TransportErrorCode Code>
struct TransportErrorInfo {
    static constexpr std::string_view message = "Unknown error";
};

template<> struct TransportErrorInfo<TransportErrorCode::None> {
    static constexpr std::string_view message = "No error";
};

// Connection errors
template<> struct TransportErrorInfo<TransportErrorCode::ConnectionFailed> {
    static constexpr std::string_view message = "Connection failed";
};

template<> struct TransportErrorInfo<TransportErrorCode::ConnectionClosed> {
    static constexpr std::string_view message = "Connection closed";
};

template<> struct TransportErrorInfo<TransportErrorCode::ConnectionRefused> {
    static constexpr std::string_view message = "Connection refused";
};

template<> struct TransportErrorInfo<TransportErrorCode::ConnectionReset> {
    static constexpr std::string_view message = "Connection reset by peer";
};

template<> struct TransportErrorInfo<TransportErrorCode::ConnectionAborted> {
    static constexpr std::string_view message = "Connection aborted";
};

// I/O errors
template<> struct TransportErrorInfo<TransportErrorCode::ReadError> {
    static constexpr std::string_view message = "Read error";
};

template<> struct TransportErrorInfo<TransportErrorCode::WriteError> {
    static constexpr std::string_view message = "Write error";
};

template<> struct TransportErrorInfo<TransportErrorCode::Timeout> {
    static constexpr std::string_view message = "Timeout";
};

// Address errors
template<> struct TransportErrorInfo<TransportErrorCode::AddressResolutionFailed> {
    static constexpr std::string_view message = "Address resolution failed";
};

template<> struct TransportErrorInfo<TransportErrorCode::NetworkUnreachable> {
    static constexpr std::string_view message = "Network unreachable";
};

template<> struct TransportErrorInfo<TransportErrorCode::HostUnreachable> {
    static constexpr std::string_view message = "Host unreachable";
};

// Socket state errors
template<> struct TransportErrorInfo<TransportErrorCode::SocketError> {
    static constexpr std::string_view message = "Socket error";
};

template<> struct TransportErrorInfo<TransportErrorCode::WouldBlock> {
    static constexpr std::string_view message = "Operation would block";
};

template<> struct TransportErrorInfo<TransportErrorCode::InProgress> {
    static constexpr std::string_view message = "Operation in progress";
};

template<> struct TransportErrorInfo<TransportErrorCode::NotConnected> {
    static constexpr std::string_view message = "Socket not connected";
};

template<> struct TransportErrorInfo<TransportErrorCode::NoBufferSpace> {
    static constexpr std::string_view message = "No buffer space available";
};

// Platform-specific errors
template<> struct TransportErrorInfo<TransportErrorCode::WinsockInitFailed> {
    static constexpr std::string_view message = "Winsock initialization failed";
};

template<> struct TransportErrorInfo<TransportErrorCode::IocpError> {
    static constexpr std::string_view message = "IOCP operation failed";
};

template<> struct TransportErrorInfo<TransportErrorCode::KqueueError> {
    static constexpr std::string_view message = "kqueue operation failed";
};

/// Generate TransportError lookup table at compile time
consteval std::array<std::string_view, TRANSPORT_ERROR_COUNT> create_transport_error_table() {
    std::array<std::string_view, TRANSPORT_ERROR_COUNT> table{};
    table[0]  = TransportErrorInfo<TransportErrorCode::None>::message;
    table[1]  = TransportErrorInfo<TransportErrorCode::ConnectionFailed>::message;
    table[2]  = TransportErrorInfo<TransportErrorCode::ConnectionClosed>::message;
    table[3]  = TransportErrorInfo<TransportErrorCode::ConnectionRefused>::message;
    table[4]  = TransportErrorInfo<TransportErrorCode::ConnectionReset>::message;
    table[5]  = TransportErrorInfo<TransportErrorCode::ConnectionAborted>::message;
    table[6]  = TransportErrorInfo<TransportErrorCode::ReadError>::message;
    table[7]  = TransportErrorInfo<TransportErrorCode::WriteError>::message;
    table[8]  = TransportErrorInfo<TransportErrorCode::Timeout>::message;
    table[9]  = TransportErrorInfo<TransportErrorCode::AddressResolutionFailed>::message;
    table[10] = TransportErrorInfo<TransportErrorCode::NetworkUnreachable>::message;
    table[11] = TransportErrorInfo<TransportErrorCode::HostUnreachable>::message;
    table[12] = TransportErrorInfo<TransportErrorCode::SocketError>::message;
    table[13] = TransportErrorInfo<TransportErrorCode::WouldBlock>::message;
    table[14] = TransportErrorInfo<TransportErrorCode::InProgress>::message;
    table[15] = TransportErrorInfo<TransportErrorCode::NotConnected>::message;
    table[16] = TransportErrorInfo<TransportErrorCode::NoBufferSpace>::message;
    table[17] = TransportErrorInfo<TransportErrorCode::WinsockInitFailed>::message;
    table[18] = TransportErrorInfo<TransportErrorCode::IocpError>::message;
    table[19] = TransportErrorInfo<TransportErrorCode::KqueueError>::message;
    return table;
}

inline constexpr auto TRANSPORT_ERROR_TABLE = create_transport_error_table();

} // namespace detail

/// Compile-time query (when code is known at compile time)
template<TransportErrorCode Code>
[[nodiscard]] consteval std::string_view transport_error_message() noexcept {
    return detail::TransportErrorInfo<Code>::message;
}

/// Runtime query using O(1) lookup table
[[nodiscard]] inline constexpr std::string_view transport_error_message(TransportErrorCode code) noexcept {
    const auto idx = static_cast<uint8_t>(code);
    if (idx < detail::TRANSPORT_ERROR_TABLE.size()) [[likely]] {
        return detail::TRANSPORT_ERROR_TABLE[idx];
    }
    return "Unknown error";
}

// Static assertions for TransportError
static_assert(detail::TransportErrorInfo<TransportErrorCode::None>::message == "No error");
static_assert(detail::TransportErrorInfo<TransportErrorCode::Timeout>::message == "Timeout");
static_assert(detail::TRANSPORT_ERROR_TABLE[0] == "No error");
static_assert(detail::TRANSPORT_ERROR_TABLE[8] == "Timeout");

struct TransportError {
    TransportErrorCode code;
    int system_errno;  // OS-level errno if applicable

    constexpr TransportError() noexcept
        : code{TransportErrorCode::None}, system_errno{0} {}

    constexpr TransportError(TransportErrorCode c) noexcept
        : code{c}, system_errno{0} {}

    constexpr TransportError(TransportErrorCode c, int err) noexcept
        : code{c}, system_errno{err} {}

    [[nodiscard]] constexpr bool ok() const noexcept {
        return code == TransportErrorCode::None;
    }

    [[nodiscard]] constexpr std::string_view message() const noexcept {
        return transport_error_message(code);
    }
};

// ============================================================================
// Validation Errors
// ============================================================================

enum class ValidationErrorCode : uint8_t {
    None = 0,
    InvalidPrice,
    InvalidQuantity,
    InvalidSide,
    InvalidOrderType,
    InvalidTimeInForce,
    InvalidSymbol,
    PriceOutOfRange,
    QuantityOutOfRange
};

inline constexpr size_t VALIDATION_ERROR_COUNT = 9;

// ============================================================================
// Compile-time ValidationError Info (TICKET_023)
// ============================================================================

namespace detail {

template<ValidationErrorCode Code>
struct ValidationErrorInfo {
    static constexpr std::string_view message = "Unknown error";
};

template<> struct ValidationErrorInfo<ValidationErrorCode::None> {
    static constexpr std::string_view message = "No error";
};

template<> struct ValidationErrorInfo<ValidationErrorCode::InvalidPrice> {
    static constexpr std::string_view message = "Invalid price";
};

template<> struct ValidationErrorInfo<ValidationErrorCode::InvalidQuantity> {
    static constexpr std::string_view message = "Invalid quantity";
};

template<> struct ValidationErrorInfo<ValidationErrorCode::InvalidSide> {
    static constexpr std::string_view message = "Invalid side";
};

template<> struct ValidationErrorInfo<ValidationErrorCode::InvalidOrderType> {
    static constexpr std::string_view message = "Invalid order type";
};

template<> struct ValidationErrorInfo<ValidationErrorCode::InvalidTimeInForce> {
    static constexpr std::string_view message = "Invalid time in force";
};

template<> struct ValidationErrorInfo<ValidationErrorCode::InvalidSymbol> {
    static constexpr std::string_view message = "Invalid symbol";
};

template<> struct ValidationErrorInfo<ValidationErrorCode::PriceOutOfRange> {
    static constexpr std::string_view message = "Price out of range";
};

template<> struct ValidationErrorInfo<ValidationErrorCode::QuantityOutOfRange> {
    static constexpr std::string_view message = "Quantity out of range";
};

/// Generate ValidationError lookup table at compile time
consteval std::array<std::string_view, VALIDATION_ERROR_COUNT> create_validation_error_table() {
    std::array<std::string_view, VALIDATION_ERROR_COUNT> table{};
    table[0] = ValidationErrorInfo<ValidationErrorCode::None>::message;
    table[1] = ValidationErrorInfo<ValidationErrorCode::InvalidPrice>::message;
    table[2] = ValidationErrorInfo<ValidationErrorCode::InvalidQuantity>::message;
    table[3] = ValidationErrorInfo<ValidationErrorCode::InvalidSide>::message;
    table[4] = ValidationErrorInfo<ValidationErrorCode::InvalidOrderType>::message;
    table[5] = ValidationErrorInfo<ValidationErrorCode::InvalidTimeInForce>::message;
    table[6] = ValidationErrorInfo<ValidationErrorCode::InvalidSymbol>::message;
    table[7] = ValidationErrorInfo<ValidationErrorCode::PriceOutOfRange>::message;
    table[8] = ValidationErrorInfo<ValidationErrorCode::QuantityOutOfRange>::message;
    return table;
}

inline constexpr auto VALIDATION_ERROR_TABLE = create_validation_error_table();

} // namespace detail

/// Compile-time query (when code is known at compile time)
template<ValidationErrorCode Code>
[[nodiscard]] consteval std::string_view validation_error_message() noexcept {
    return detail::ValidationErrorInfo<Code>::message;
}

/// Runtime query using O(1) lookup table
[[nodiscard]] inline constexpr std::string_view validation_error_message(ValidationErrorCode code) noexcept {
    const auto idx = static_cast<uint8_t>(code);
    if (idx < detail::VALIDATION_ERROR_TABLE.size()) [[likely]] {
        return detail::VALIDATION_ERROR_TABLE[idx];
    }
    return "Unknown error";
}

// Static assertions for ValidationError
static_assert(detail::ValidationErrorInfo<ValidationErrorCode::None>::message == "No error");
static_assert(detail::ValidationErrorInfo<ValidationErrorCode::InvalidPrice>::message == "Invalid price");
static_assert(detail::VALIDATION_ERROR_TABLE[0] == "No error");
static_assert(detail::VALIDATION_ERROR_TABLE[1] == "Invalid price");

struct ValidationError {
    ValidationErrorCode code;
    int tag;

    constexpr ValidationError() noexcept
        : code{ValidationErrorCode::None}, tag{0} {}

    constexpr ValidationError(ValidationErrorCode c, int t = 0) noexcept
        : code{c}, tag{t} {}

    [[nodiscard]] constexpr bool ok() const noexcept {
        return code == ValidationErrorCode::None;
    }

    [[nodiscard]] constexpr std::string_view message() const noexcept {
        return validation_error_message(code);
    }
};

// ============================================================================
// Result Type Aliases (using std::expected)
// ============================================================================

template <typename T>
using ParseResult = std::expected<T, ParseError>;

template <typename T>
using SessionResult = std::expected<T, SessionError>;

template <typename T>
using TransportResult = std::expected<T, TransportError>;

template <typename T>
using ValidationResult = std::expected<T, ValidationError>;

// ============================================================================
// Utility Functions
// ============================================================================

/// Create success result
template <typename T>
[[nodiscard]] constexpr auto make_result(T&& value) noexcept {
    return std::expected<std::decay_t<T>, ParseError>{std::forward<T>(value)};
}

/// Create error result
template <typename E>
[[nodiscard]] constexpr auto make_error(E error) noexcept {
    return std::unexpected{error};
}

// ============================================================================
// Monadic Utilities (C++23 std::expected/optional enhancements)
// ============================================================================

/// Transform optional<int64_t> to optional<uint32_t> with bounds checking
[[nodiscard]] inline constexpr std::optional<uint32_t>
to_uint32(std::optional<int64_t> opt) noexcept {
    return opt.and_then([](int64_t v) -> std::optional<uint32_t> {
        if (v >= 0 && v <= static_cast<int64_t>(UINT32_MAX)) [[likely]] {
            return static_cast<uint32_t>(v);
        }
        return std::nullopt;
    });
}

/// Transform optional<int64_t> to optional<int> with bounds checking
[[nodiscard]] inline constexpr std::optional<int>
to_int(std::optional<int64_t> opt) noexcept {
    return opt.and_then([](int64_t v) -> std::optional<int> {
        if (v >= INT_MIN && v <= INT_MAX) [[likely]] {
            return static_cast<int>(v);
        }
        return std::nullopt;
    });
}

/// Apply a function to optional value, ignoring the result (for side effects)
template <typename T, typename F>
constexpr void if_has_value(const std::optional<T>& opt, F&& func) noexcept {
    if (opt.has_value()) {
        std::forward<F>(func)(*opt);
    }
}

/// Chain multiple expected operations with error propagation
/// Usage: chain_parse(parse1(data), parse2, parse3)
template <typename T, typename E, typename F>
[[nodiscard]] constexpr auto chain_result(std::expected<T, E>&& result, F&& func)
    -> decltype(std::forward<F>(func)(std::move(*result)))
{
    return std::move(result).and_then(std::forward<F>(func));
}

/// Create ParseResult error for invalid message type
[[nodiscard]] inline constexpr ParseResult<void>
require_msg_type(char actual, char expected) noexcept {
    if (actual == expected) [[likely]] {
        return {};
    }
    return std::unexpected{ParseError{ParseErrorCode::InvalidMsgType}};
}

/// Require a field to be present, returning error if missing
template <typename T>
[[nodiscard]] inline constexpr ParseResult<T>
require_field(std::optional<T> opt, int tag) noexcept {
    if (opt.has_value()) [[likely]] {
        return *opt;
    }
    return std::unexpected{ParseError{ParseErrorCode::MissingRequiredField, tag}};
}

/// Transform expected value or propagate error
/// Usage: result.transform(to_upper).or_else(log_error)
template <typename T, typename E, typename F>
[[nodiscard]] constexpr auto
transform_or_error(std::expected<T, E>&& result, F&& func) noexcept
    -> std::expected<decltype(std::forward<F>(func)(std::move(*result))), E>
{
    return std::move(result).transform(std::forward<F>(func));
}

} // namespace nfx
