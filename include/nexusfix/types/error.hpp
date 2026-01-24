#pragma once

#include <expected>
#include <string_view>
#include <cstdint>
#include <source_location>

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
        switch (code) {
            case ParseErrorCode::None:               return "No error";
            case ParseErrorCode::BufferTooShort:     return "Buffer too short";
            case ParseErrorCode::InvalidBeginString: return "Invalid BeginString";
            case ParseErrorCode::InvalidBodyLength:  return "Invalid BodyLength";
            case ParseErrorCode::InvalidChecksum:    return "Invalid CheckSum";
            case ParseErrorCode::MissingRequiredField: return "Missing required field";
            case ParseErrorCode::InvalidFieldFormat: return "Invalid field format";
            case ParseErrorCode::InvalidTagNumber:   return "Invalid tag number";
            case ParseErrorCode::DuplicateTag:       return "Duplicate tag";
            case ParseErrorCode::UnterminatedField:  return "Unterminated field";
            case ParseErrorCode::InvalidMsgType:     return "Invalid MsgType";
            case ParseErrorCode::GarbledMessage:     return "Garbled message";
        }
        return "Unknown error";
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
        switch (code) {
            case SessionErrorCode::None:            return "No error";
            case SessionErrorCode::NotConnected:    return "Not connected";
            case SessionErrorCode::AlreadyConnected: return "Already connected";
            case SessionErrorCode::LogonRejected:   return "Logon rejected";
            case SessionErrorCode::LogonTimeout:    return "Logon timeout";
            case SessionErrorCode::HeartbeatTimeout: return "Heartbeat timeout";
            case SessionErrorCode::SequenceGap:     return "Sequence gap detected";
            case SessionErrorCode::InvalidState:    return "Invalid session state";
            case SessionErrorCode::Disconnected:    return "Disconnected";
        }
        return "Unknown error";
    }
};

// ============================================================================
// Transport Errors
// ============================================================================

enum class TransportErrorCode : uint8_t {
    None = 0,
    ConnectionFailed,
    ConnectionClosed,
    ReadError,
    WriteError,
    Timeout,
    AddressResolutionFailed,
    SocketError
};

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
        switch (code) {
            case TransportErrorCode::None:           return "No error";
            case TransportErrorCode::ConnectionFailed: return "Connection failed";
            case TransportErrorCode::ConnectionClosed: return "Connection closed";
            case TransportErrorCode::ReadError:      return "Read error";
            case TransportErrorCode::WriteError:     return "Write error";
            case TransportErrorCode::Timeout:        return "Timeout";
            case TransportErrorCode::AddressResolutionFailed: return "Address resolution failed";
            case TransportErrorCode::SocketError:    return "Socket error";
        }
        return "Unknown error";
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
        switch (code) {
            case ValidationErrorCode::None:           return "No error";
            case ValidationErrorCode::InvalidPrice:   return "Invalid price";
            case ValidationErrorCode::InvalidQuantity: return "Invalid quantity";
            case ValidationErrorCode::InvalidSide:    return "Invalid side";
            case ValidationErrorCode::InvalidOrderType: return "Invalid order type";
            case ValidationErrorCode::InvalidTimeInForce: return "Invalid time in force";
            case ValidationErrorCode::InvalidSymbol:  return "Invalid symbol";
            case ValidationErrorCode::PriceOutOfRange: return "Price out of range";
            case ValidationErrorCode::QuantityOutOfRange: return "Quantity out of range";
        }
        return "Unknown error";
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

} // namespace nfx
