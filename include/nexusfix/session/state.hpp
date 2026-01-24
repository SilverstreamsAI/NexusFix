#pragma once

#include <cstdint>
#include <string_view>
#include <chrono>

namespace nfx {

// ============================================================================
// Session State Machine
// ============================================================================

/// FIX session states
enum class SessionState : uint8_t {
    Disconnected,           // Not connected
    SocketConnected,        // TCP connected, waiting for logon
    LogonSent,              // Logon message sent, waiting for response
    LogonReceived,          // Received logon, validating
    Active,                 // Session established, normal operation
    LogoutPending,          // Logout sent, waiting for response
    LogoutReceived,         // Received logout from counterparty
    Reconnecting,           // Attempting to reconnect
    Error                   // Unrecoverable error state
};

/// Get string representation of session state
[[nodiscard]] constexpr std::string_view state_name(SessionState state) noexcept {
    switch (state) {
        case SessionState::Disconnected:    return "Disconnected";
        case SessionState::SocketConnected: return "SocketConnected";
        case SessionState::LogonSent:       return "LogonSent";
        case SessionState::LogonReceived:   return "LogonReceived";
        case SessionState::Active:          return "Active";
        case SessionState::LogoutPending:   return "LogoutPending";
        case SessionState::LogoutReceived:  return "LogoutReceived";
        case SessionState::Reconnecting:    return "Reconnecting";
        case SessionState::Error:           return "Error";
    }
    return "Unknown";
}

/// Check if session is in a connected state
[[nodiscard]] constexpr bool is_connected(SessionState state) noexcept {
    return state == SessionState::SocketConnected ||
           state == SessionState::LogonSent ||
           state == SessionState::LogonReceived ||
           state == SessionState::Active ||
           state == SessionState::LogoutPending;
}

/// Check if session can send application messages
[[nodiscard]] constexpr bool can_send_app_messages(SessionState state) noexcept {
    return state == SessionState::Active;
}

// ============================================================================
// Session Events
// ============================================================================

/// Events that trigger state transitions
enum class SessionEvent : uint8_t {
    Connect,                // TCP connection established
    Disconnect,             // TCP connection lost
    LogonSent,              // Outgoing logon sent
    LogonReceived,          // Incoming logon received
    LogonAcknowledged,      // Logon acknowledged (session active)
    LogonRejected,          // Logon was rejected
    LogoutSent,             // Outgoing logout sent
    LogoutReceived,         // Incoming logout received
    HeartbeatTimeout,       // Heartbeat timeout
    TestRequestSent,        // Test request sent
    TestRequestReceived,    // Test request received
    MessageReceived,        // Any message received (resets timeout)
    Error                   // Error occurred
};

/// Get string representation of session event
[[nodiscard]] constexpr std::string_view event_name(SessionEvent event) noexcept {
    switch (event) {
        case SessionEvent::Connect:            return "Connect";
        case SessionEvent::Disconnect:         return "Disconnect";
        case SessionEvent::LogonSent:          return "LogonSent";
        case SessionEvent::LogonReceived:      return "LogonReceived";
        case SessionEvent::LogonAcknowledged:  return "LogonAcknowledged";
        case SessionEvent::LogonRejected:      return "LogonRejected";
        case SessionEvent::LogoutSent:         return "LogoutSent";
        case SessionEvent::LogoutReceived:     return "LogoutReceived";
        case SessionEvent::HeartbeatTimeout:   return "HeartbeatTimeout";
        case SessionEvent::TestRequestSent:    return "TestRequestSent";
        case SessionEvent::TestRequestReceived: return "TestRequestReceived";
        case SessionEvent::MessageReceived:    return "MessageReceived";
        case SessionEvent::Error:              return "Error";
    }
    return "Unknown";
}

// ============================================================================
// State Transition Table
// ============================================================================

/// Determine next state based on current state and event
[[nodiscard]] constexpr SessionState next_state(
    SessionState current,
    SessionEvent event) noexcept
{
    switch (current) {
        case SessionState::Disconnected:
            if (event == SessionEvent::Connect) return SessionState::SocketConnected;
            break;

        case SessionState::SocketConnected:
            if (event == SessionEvent::LogonSent) return SessionState::LogonSent;
            if (event == SessionEvent::LogonReceived) return SessionState::LogonReceived;
            if (event == SessionEvent::Disconnect) return SessionState::Disconnected;
            break;

        case SessionState::LogonSent:
            if (event == SessionEvent::LogonReceived) return SessionState::Active;
            if (event == SessionEvent::LogonRejected) return SessionState::Disconnected;
            if (event == SessionEvent::Disconnect) return SessionState::Disconnected;
            if (event == SessionEvent::HeartbeatTimeout) return SessionState::Error;
            break;

        case SessionState::LogonReceived:
            if (event == SessionEvent::LogonAcknowledged) return SessionState::Active;
            if (event == SessionEvent::LogonRejected) return SessionState::Disconnected;
            if (event == SessionEvent::Disconnect) return SessionState::Disconnected;
            break;

        case SessionState::Active:
            if (event == SessionEvent::LogoutSent) return SessionState::LogoutPending;
            if (event == SessionEvent::LogoutReceived) return SessionState::LogoutReceived;
            if (event == SessionEvent::Disconnect) return SessionState::Reconnecting;
            if (event == SessionEvent::HeartbeatTimeout) return SessionState::Error;
            if (event == SessionEvent::Error) return SessionState::Error;
            break;

        case SessionState::LogoutPending:
            if (event == SessionEvent::LogoutReceived) return SessionState::Disconnected;
            if (event == SessionEvent::HeartbeatTimeout) return SessionState::Disconnected;
            if (event == SessionEvent::Disconnect) return SessionState::Disconnected;
            break;

        case SessionState::LogoutReceived:
            if (event == SessionEvent::LogoutSent) return SessionState::Disconnected;
            if (event == SessionEvent::Disconnect) return SessionState::Disconnected;
            break;

        case SessionState::Reconnecting:
            if (event == SessionEvent::Connect) return SessionState::SocketConnected;
            if (event == SessionEvent::Error) return SessionState::Error;
            break;

        case SessionState::Error:
            if (event == SessionEvent::Connect) return SessionState::SocketConnected;
            break;
    }

    return current;  // No transition
}

// ============================================================================
// Session Configuration
// ============================================================================

/// Session configuration parameters
struct SessionConfig {
    std::string_view sender_comp_id;
    std::string_view target_comp_id;
    std::string_view begin_string{"FIX.4.4"};

    // Timing parameters (in seconds)
    int heart_bt_int{30};           // Heartbeat interval
    int logon_timeout{10};          // Logon response timeout
    int logout_timeout{5};          // Logout response timeout
    int reconnect_interval{5};      // Reconnection attempt interval
    int max_reconnect_attempts{3};  // Max reconnection attempts

    // Session behavior
    bool reset_seq_num_on_logon{false};
    bool validate_comp_ids{true};
    bool validate_checksum{true};
    bool persist_messages{false};

    // CPU affinity (for latency optimization)
    int cpu_affinity_core{-1};      // Pin session thread to specific core (-1 = auto/disabled)
    bool auto_pin_to_core{false};   // Auto-pin based on session ID hash

    constexpr SessionConfig() noexcept = default;
};

// ============================================================================
// Session Statistics
// ============================================================================

/// Runtime session statistics
struct SessionStats {
    uint64_t messages_sent{0};
    uint64_t messages_received{0};
    uint64_t bytes_sent{0};
    uint64_t bytes_received{0};
    uint64_t heartbeats_sent{0};
    uint64_t heartbeats_received{0};
    uint64_t test_requests_sent{0};
    uint64_t resend_requests_sent{0};
    uint64_t sequence_resets{0};
    uint64_t reconnect_count{0};

    using TimePoint = std::chrono::steady_clock::time_point;
    TimePoint session_start;
    TimePoint last_message_sent;
    TimePoint last_message_received;

    constexpr SessionStats() noexcept = default;

    void reset() noexcept {
        messages_sent = 0;
        messages_received = 0;
        bytes_sent = 0;
        bytes_received = 0;
        heartbeats_sent = 0;
        heartbeats_received = 0;
        test_requests_sent = 0;
        resend_requests_sent = 0;
        sequence_resets = 0;
        reconnect_count = 0;
    }
};

// ============================================================================
// Session Identity
// ============================================================================

/// Identifies a FIX session (SenderCompID + TargetCompID)
struct SessionId {
    std::string_view sender_comp_id;
    std::string_view target_comp_id;
    std::string_view begin_string;

    constexpr SessionId() noexcept = default;

    constexpr SessionId(
        std::string_view sender,
        std::string_view target,
        std::string_view version = "FIX.4.4") noexcept
        : sender_comp_id{sender}
        , target_comp_id{target}
        , begin_string{version} {}

    [[nodiscard]] constexpr bool operator==(const SessionId& other) const noexcept {
        return sender_comp_id == other.sender_comp_id &&
               target_comp_id == other.target_comp_id &&
               begin_string == other.begin_string;
    }

    /// Create reverse session ID (swap sender/target)
    [[nodiscard]] constexpr SessionId reverse() const noexcept {
        return SessionId{target_comp_id, sender_comp_id, begin_string};
    }
};

} // namespace nfx
