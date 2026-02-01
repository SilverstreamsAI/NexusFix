#pragma once

#include <array>
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

inline constexpr size_t SESSION_STATE_COUNT = 9;

// ============================================================================
// Compile-time SessionState Info (TICKET_023)
// ============================================================================

namespace detail {

template<SessionState State>
struct StateInfo {
    static constexpr std::string_view name = "Unknown";
    static constexpr bool is_connected = false;
};

template<> struct StateInfo<SessionState::Disconnected> {
    static constexpr std::string_view name = "Disconnected";
    static constexpr bool is_connected = false;
};

template<> struct StateInfo<SessionState::SocketConnected> {
    static constexpr std::string_view name = "SocketConnected";
    static constexpr bool is_connected = true;
};

template<> struct StateInfo<SessionState::LogonSent> {
    static constexpr std::string_view name = "LogonSent";
    static constexpr bool is_connected = true;
};

template<> struct StateInfo<SessionState::LogonReceived> {
    static constexpr std::string_view name = "LogonReceived";
    static constexpr bool is_connected = true;
};

template<> struct StateInfo<SessionState::Active> {
    static constexpr std::string_view name = "Active";
    static constexpr bool is_connected = true;
};

template<> struct StateInfo<SessionState::LogoutPending> {
    static constexpr std::string_view name = "LogoutPending";
    static constexpr bool is_connected = true;
};

template<> struct StateInfo<SessionState::LogoutReceived> {
    static constexpr std::string_view name = "LogoutReceived";
    static constexpr bool is_connected = false;
};

template<> struct StateInfo<SessionState::Reconnecting> {
    static constexpr std::string_view name = "Reconnecting";
    static constexpr bool is_connected = false;
};

template<> struct StateInfo<SessionState::Error> {
    static constexpr std::string_view name = "Error";
    static constexpr bool is_connected = false;
};

/// Generate state name lookup table
consteval std::array<std::string_view, SESSION_STATE_COUNT> create_state_name_table() {
    std::array<std::string_view, SESSION_STATE_COUNT> table{};
    table[0] = StateInfo<SessionState::Disconnected>::name;
    table[1] = StateInfo<SessionState::SocketConnected>::name;
    table[2] = StateInfo<SessionState::LogonSent>::name;
    table[3] = StateInfo<SessionState::LogonReceived>::name;
    table[4] = StateInfo<SessionState::Active>::name;
    table[5] = StateInfo<SessionState::LogoutPending>::name;
    table[6] = StateInfo<SessionState::LogoutReceived>::name;
    table[7] = StateInfo<SessionState::Reconnecting>::name;
    table[8] = StateInfo<SessionState::Error>::name;
    return table;
}

/// Generate is_connected lookup table
consteval std::array<bool, SESSION_STATE_COUNT> create_is_connected_table() {
    std::array<bool, SESSION_STATE_COUNT> table{};
    table[0] = StateInfo<SessionState::Disconnected>::is_connected;
    table[1] = StateInfo<SessionState::SocketConnected>::is_connected;
    table[2] = StateInfo<SessionState::LogonSent>::is_connected;
    table[3] = StateInfo<SessionState::LogonReceived>::is_connected;
    table[4] = StateInfo<SessionState::Active>::is_connected;
    table[5] = StateInfo<SessionState::LogoutPending>::is_connected;
    table[6] = StateInfo<SessionState::LogoutReceived>::is_connected;
    table[7] = StateInfo<SessionState::Reconnecting>::is_connected;
    table[8] = StateInfo<SessionState::Error>::is_connected;
    return table;
}

inline constexpr auto STATE_NAME_TABLE = create_state_name_table();
inline constexpr auto IS_CONNECTED_TABLE = create_is_connected_table();

} // namespace detail

/// Compile-time state name query
template<SessionState State>
[[nodiscard]] consteval std::string_view state_name() noexcept {
    return detail::StateInfo<State>::name;
}

/// Runtime state name query using O(1) lookup
[[nodiscard]] inline constexpr std::string_view state_name(SessionState state) noexcept {
    const auto idx = static_cast<uint8_t>(state);
    if (idx < detail::STATE_NAME_TABLE.size()) [[likely]] {
        return detail::STATE_NAME_TABLE[idx];
    }
    return "Unknown";
}

// Static assertions for SessionState
static_assert(detail::StateInfo<SessionState::Disconnected>::name == "Disconnected");
static_assert(detail::StateInfo<SessionState::Active>::name == "Active");
static_assert(detail::STATE_NAME_TABLE[0] == "Disconnected");
static_assert(detail::STATE_NAME_TABLE[4] == "Active");

/// Compile-time is_connected query
template<SessionState State>
[[nodiscard]] consteval bool is_connected() noexcept {
    return detail::StateInfo<State>::is_connected;
}

/// Runtime is_connected query using O(1) lookup
[[nodiscard]] inline constexpr bool is_connected(SessionState state) noexcept {
    const auto idx = static_cast<uint8_t>(state);
    if (idx < detail::IS_CONNECTED_TABLE.size()) [[likely]] {
        return detail::IS_CONNECTED_TABLE[idx];
    }
    return false;
}

// Static assertions for is_connected
static_assert(detail::StateInfo<SessionState::Disconnected>::is_connected == false);
static_assert(detail::StateInfo<SessionState::Active>::is_connected == true);
static_assert(detail::IS_CONNECTED_TABLE[0] == false);  // Disconnected
static_assert(detail::IS_CONNECTED_TABLE[4] == true);   // Active

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

inline constexpr size_t SESSION_EVENT_COUNT = 13;

// ============================================================================
// Compile-time SessionEvent Info (TICKET_023)
// ============================================================================

namespace detail {

template<SessionEvent Event>
struct EventInfo {
    static constexpr std::string_view name = "Unknown";
};

template<> struct EventInfo<SessionEvent::Connect> {
    static constexpr std::string_view name = "Connect";
};

template<> struct EventInfo<SessionEvent::Disconnect> {
    static constexpr std::string_view name = "Disconnect";
};

template<> struct EventInfo<SessionEvent::LogonSent> {
    static constexpr std::string_view name = "LogonSent";
};

template<> struct EventInfo<SessionEvent::LogonReceived> {
    static constexpr std::string_view name = "LogonReceived";
};

template<> struct EventInfo<SessionEvent::LogonAcknowledged> {
    static constexpr std::string_view name = "LogonAcknowledged";
};

template<> struct EventInfo<SessionEvent::LogonRejected> {
    static constexpr std::string_view name = "LogonRejected";
};

template<> struct EventInfo<SessionEvent::LogoutSent> {
    static constexpr std::string_view name = "LogoutSent";
};

template<> struct EventInfo<SessionEvent::LogoutReceived> {
    static constexpr std::string_view name = "LogoutReceived";
};

template<> struct EventInfo<SessionEvent::HeartbeatTimeout> {
    static constexpr std::string_view name = "HeartbeatTimeout";
};

template<> struct EventInfo<SessionEvent::TestRequestSent> {
    static constexpr std::string_view name = "TestRequestSent";
};

template<> struct EventInfo<SessionEvent::TestRequestReceived> {
    static constexpr std::string_view name = "TestRequestReceived";
};

template<> struct EventInfo<SessionEvent::MessageReceived> {
    static constexpr std::string_view name = "MessageReceived";
};

template<> struct EventInfo<SessionEvent::Error> {
    static constexpr std::string_view name = "Error";
};

/// Generate event name lookup table
consteval std::array<std::string_view, SESSION_EVENT_COUNT> create_event_name_table() {
    std::array<std::string_view, SESSION_EVENT_COUNT> table{};
    table[0]  = EventInfo<SessionEvent::Connect>::name;
    table[1]  = EventInfo<SessionEvent::Disconnect>::name;
    table[2]  = EventInfo<SessionEvent::LogonSent>::name;
    table[3]  = EventInfo<SessionEvent::LogonReceived>::name;
    table[4]  = EventInfo<SessionEvent::LogonAcknowledged>::name;
    table[5]  = EventInfo<SessionEvent::LogonRejected>::name;
    table[6]  = EventInfo<SessionEvent::LogoutSent>::name;
    table[7]  = EventInfo<SessionEvent::LogoutReceived>::name;
    table[8]  = EventInfo<SessionEvent::HeartbeatTimeout>::name;
    table[9]  = EventInfo<SessionEvent::TestRequestSent>::name;
    table[10] = EventInfo<SessionEvent::TestRequestReceived>::name;
    table[11] = EventInfo<SessionEvent::MessageReceived>::name;
    table[12] = EventInfo<SessionEvent::Error>::name;
    return table;
}

inline constexpr auto EVENT_NAME_TABLE = create_event_name_table();

} // namespace detail

/// Compile-time event name query
template<SessionEvent Event>
[[nodiscard]] consteval std::string_view event_name() noexcept {
    return detail::EventInfo<Event>::name;
}

/// Runtime event name query using O(1) lookup
[[nodiscard]] inline constexpr std::string_view event_name(SessionEvent event) noexcept {
    const auto idx = static_cast<uint8_t>(event);
    if (idx < detail::EVENT_NAME_TABLE.size()) [[likely]] {
        return detail::EVENT_NAME_TABLE[idx];
    }
    return "Unknown";
}

// Static assertions for SessionEvent
static_assert(detail::EventInfo<SessionEvent::Connect>::name == "Connect");
static_assert(detail::EventInfo<SessionEvent::Error>::name == "Error");
static_assert(detail::EVENT_NAME_TABLE[0] == "Connect");
static_assert(detail::EVENT_NAME_TABLE[12] == "Error");

// ============================================================================
// Compile-time State Transition Table (TICKET_023)
// 2D lookup table: TRANSITION_TABLE[state][event] -> next_state
// ============================================================================

namespace detail {

/// Sentinel value meaning "no transition" (stay in current state)
inline constexpr uint8_t NO_TRANSITION = 0xFF;

/// Generate 2D state transition table at compile time
consteval std::array<std::array<uint8_t, SESSION_EVENT_COUNT>, SESSION_STATE_COUNT>
create_transition_table() {
    std::array<std::array<uint8_t, SESSION_EVENT_COUNT>, SESSION_STATE_COUNT> table{};

    // Initialize all entries to NO_TRANSITION
    for (auto& row : table) {
        for (auto& cell : row) {
            cell = NO_TRANSITION;
        }
    }

    // Disconnected transitions
    table[0][0] = 1;  // Connect -> SocketConnected

    // SocketConnected transitions
    table[1][1] = 0;  // Disconnect -> Disconnected
    table[1][2] = 2;  // LogonSent -> LogonSent
    table[1][3] = 3;  // LogonReceived -> LogonReceived

    // LogonSent transitions
    table[2][1] = 0;  // Disconnect -> Disconnected
    table[2][3] = 4;  // LogonReceived -> Active
    table[2][5] = 0;  // LogonRejected -> Disconnected
    table[2][8] = 8;  // HeartbeatTimeout -> Error

    // LogonReceived transitions
    table[3][1] = 0;  // Disconnect -> Disconnected
    table[3][4] = 4;  // LogonAcknowledged -> Active
    table[3][5] = 0;  // LogonRejected -> Disconnected

    // Active transitions
    table[4][1] = 7;  // Disconnect -> Reconnecting
    table[4][6] = 5;  // LogoutSent -> LogoutPending
    table[4][7] = 6;  // LogoutReceived -> LogoutReceived
    table[4][8] = 8;  // HeartbeatTimeout -> Error
    table[4][12] = 8; // Error -> Error

    // LogoutPending transitions
    table[5][1] = 0;  // Disconnect -> Disconnected
    table[5][7] = 0;  // LogoutReceived -> Disconnected
    table[5][8] = 0;  // HeartbeatTimeout -> Disconnected

    // LogoutReceived transitions
    table[6][1] = 0;  // Disconnect -> Disconnected
    table[6][6] = 0;  // LogoutSent -> Disconnected

    // Reconnecting transitions
    table[7][0] = 1;  // Connect -> SocketConnected
    table[7][12] = 8; // Error -> Error

    // Error transitions
    table[8][0] = 1;  // Connect -> SocketConnected

    return table;
}

inline constexpr auto TRANSITION_TABLE = create_transition_table();

} // namespace detail

/// Determine next state based on current state and event
/// Uses O(1) 2D lookup table instead of nested switch/if
[[nodiscard]] inline constexpr SessionState next_state(
    SessionState current,
    SessionEvent event) noexcept
{
    const auto state_idx = static_cast<uint8_t>(current);
    const auto event_idx = static_cast<uint8_t>(event);

    if (state_idx < SESSION_STATE_COUNT && event_idx < SESSION_EVENT_COUNT) [[likely]] {
        const auto next = detail::TRANSITION_TABLE[state_idx][event_idx];
        if (next != detail::NO_TRANSITION) {
            return static_cast<SessionState>(next);
        }
    }
    return current;  // No transition
}

// Static assertions for state transitions
static_assert(detail::TRANSITION_TABLE[0][0] == 1);  // Disconnected + Connect -> SocketConnected
static_assert(detail::TRANSITION_TABLE[4][6] == 5);  // Active + LogoutSent -> LogoutPending
static_assert(detail::TRANSITION_TABLE[4][7] == 6);  // Active + LogoutReceived -> LogoutReceived

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
