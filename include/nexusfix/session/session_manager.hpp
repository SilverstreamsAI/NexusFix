#pragma once

#include <chrono>
#include <functional>
#include <span>

#include "nexusfix/types/tag.hpp"
#include "nexusfix/types/error.hpp"
#include "nexusfix/interfaces/i_message.hpp"
#include "nexusfix/parser/runtime_parser.hpp"
#include "nexusfix/messages/common/header.hpp"
#include "nexusfix/messages/common/trailer.hpp"
#include "nexusfix/messages/fix44/logon.hpp"
#include "nexusfix/messages/fix44/heartbeat.hpp"
#include "nexusfix/session/state.hpp"
#include "nexusfix/session/sequence.hpp"
#include "nexusfix/session/coroutine.hpp"

namespace nfx {

// ============================================================================
// Session Callbacks
// ============================================================================

/// Callback interface for session events
struct SessionCallbacks {
    /// Called when an application message is received
    std::function<void(const ParsedMessage&)> on_app_message;

    /// Called when session state changes
    std::function<void(SessionState, SessionState)> on_state_change;

    /// Called when a message needs to be sent
    std::function<bool(std::span<const char>)> on_send;

    /// Called on session error
    std::function<void(const SessionError&)> on_error;

    /// Called when logon is complete
    std::function<void()> on_logon;

    /// Called when logout is complete
    std::function<void(std::string_view)> on_logout;
};

// ============================================================================
// Heartbeat Timer
// ============================================================================

/// Manages heartbeat timing for a session
class HeartbeatTimer {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration = std::chrono::seconds;

    explicit HeartbeatTimer(int interval_seconds = 30) noexcept
        : interval_{interval_seconds}
        , last_sent_{Clock::now()}
        , last_received_{Clock::now()}
        , test_request_pending_{false} {}

    /// Record that a message was sent
    void message_sent() noexcept {
        last_sent_ = Clock::now();
    }

    /// Record that a message was received
    void message_received() noexcept {
        last_received_ = Clock::now();
        test_request_pending_ = false;
    }

    /// Check if heartbeat should be sent
    [[nodiscard]] bool should_send_heartbeat() const noexcept {
        auto now = Clock::now();
        auto elapsed = std::chrono::duration_cast<Duration>(now - last_sent_);
        return elapsed >= interval_;
    }

    /// Check if test request should be sent (no message received)
    [[nodiscard]] bool should_send_test_request() const noexcept {
        if (test_request_pending_) return false;

        auto now = Clock::now();
        auto elapsed = std::chrono::duration_cast<Duration>(now - last_received_);
        return elapsed >= interval_ + Duration{interval_.count() / 2};
    }

    /// Check if connection has timed out
    [[nodiscard]] bool has_timed_out() const noexcept {
        auto now = Clock::now();
        auto elapsed = std::chrono::duration_cast<Duration>(now - last_received_);
        return elapsed >= interval_ * 2;
    }

    /// Mark that a test request was sent
    void test_request_sent() noexcept {
        test_request_pending_ = true;
    }

    /// Set heartbeat interval
    void set_interval(int seconds) noexcept {
        interval_ = Duration{seconds};
    }

    /// Get heartbeat interval
    [[nodiscard]] int interval() const noexcept {
        return static_cast<int>(interval_.count());
    }

    /// Reset timer
    void reset() noexcept {
        auto now = Clock::now();
        last_sent_ = now;
        last_received_ = now;
        test_request_pending_ = false;
    }

private:
    Duration interval_;
    TimePoint last_sent_;
    TimePoint last_received_;
    bool test_request_pending_;
};

// ============================================================================
// Session Manager
// ============================================================================

/// Manages FIX session lifecycle and message handling
class SessionManager {
public:
    explicit SessionManager(const SessionConfig& config) noexcept
        : config_{config}
        , state_{SessionState::Disconnected}
        , heartbeat_timer_{config.heart_bt_int}
        , assembler_{}
        , sequences_{}
        , stats_{} {}

    // Non-copyable, non-movable
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;

    // ========================================================================
    // Session Control
    // ========================================================================

    /// Set callbacks
    void set_callbacks(SessionCallbacks callbacks) noexcept {
        callbacks_ = std::move(callbacks);
    }

    /// Called when TCP connection is established
    void on_connect() noexcept {
        transition(SessionEvent::Connect);
    }

    /// Called when TCP connection is lost
    void on_disconnect() noexcept {
        transition(SessionEvent::Disconnect);
    }

    /// Initiate logon
    SessionResult<void> initiate_logon() noexcept {
        if (state_ != SessionState::SocketConnected) {
            return std::unexpected{SessionError{SessionErrorCode::InvalidState}};
        }

        // Build logon message
        auto msg = fix44::Logon::Builder{}
            .sender_comp_id(config_.sender_comp_id)
            .target_comp_id(config_.target_comp_id)
            .msg_seq_num(sequences_.next_outbound())
            .sending_time(current_timestamp())
            .encrypt_method(0)
            .heart_bt_int(config_.heart_bt_int)
            .reset_seq_num_flag(config_.reset_seq_num_on_logon)
            .build(assembler_);

        if (!send_message(msg)) {
            return std::unexpected{SessionError{SessionErrorCode::NotConnected}};
        }

        transition(SessionEvent::LogonSent);
        return {};
    }

    /// Initiate logout
    SessionResult<void> initiate_logout(std::string_view text = "") noexcept {
        if (state_ != SessionState::Active) {
            return std::unexpected{SessionError{SessionErrorCode::InvalidState}};
        }

        auto msg = fix44::Logout::Builder{}
            .sender_comp_id(config_.sender_comp_id)
            .target_comp_id(config_.target_comp_id)
            .msg_seq_num(sequences_.next_outbound())
            .sending_time(current_timestamp())
            .text(text)
            .build(assembler_);

        if (!send_message(msg)) {
            return std::unexpected{SessionError{SessionErrorCode::NotConnected}};
        }

        transition(SessionEvent::LogoutSent);
        return {};
    }

    /// Process incoming data
    void on_data_received(std::span<const char> data) noexcept {
        // Update heartbeat timer
        heartbeat_timer_.message_received();
        ++stats_.messages_received;
        stats_.bytes_received += data.size();

        // Parse message
        auto result = ParsedMessage::parse(data);
        if (!result.has_value()) {
            handle_parse_error(result.error());
            return;
        }

        auto& msg = *result;

        // Validate sequence number
        auto seq_result = sequences_.validate_inbound(msg.msg_seq_num());
        if (seq_result == SequenceManager::SequenceResult::GapDetected) {
            handle_sequence_gap(msg.msg_seq_num());
        } else if (seq_result == SequenceManager::SequenceResult::TooLow) {
            // Possible duplicate, check PossDupFlag
            if (!msg.header().poss_dup_flag) {
                // Sequence too low and not marked as duplicate
                handle_sequence_error(msg.msg_seq_num());
                return;
            }
        }

        // Route by message type
        char msg_type = msg.msg_type();
        if (msg_type::is_admin(msg_type)) {
            handle_admin_message(msg);
        } else {
            handle_app_message(msg);
        }
    }

    /// Periodic timer tick (call regularly, e.g., every 100ms)
    void on_timer_tick() noexcept {
        if (state_ != SessionState::Active) return;

        if (heartbeat_timer_.has_timed_out()) {
            transition(SessionEvent::HeartbeatTimeout);
            return;
        }

        if (heartbeat_timer_.should_send_test_request()) {
            send_test_request();
        } else if (heartbeat_timer_.should_send_heartbeat()) {
            send_heartbeat();
        }
    }

    // ========================================================================
    // Message Sending
    // ========================================================================

    /// Send an application message
    template <typename MsgBuilder>
    SessionResult<void> send_app_message(MsgBuilder& builder) noexcept {
        if (!can_send_app_messages(state_)) {
            return std::unexpected{SessionError{SessionErrorCode::InvalidState}};
        }

        auto msg = builder
            .sender_comp_id(config_.sender_comp_id)
            .target_comp_id(config_.target_comp_id)
            .msg_seq_num(sequences_.next_outbound())
            .sending_time(current_timestamp())
            .build(assembler_);

        if (!send_message(msg)) {
            return std::unexpected{SessionError{SessionErrorCode::NotConnected}};
        }

        return {};
    }

    // ========================================================================
    // Accessors
    // ========================================================================

    [[nodiscard]] SessionState state() const noexcept { return state_; }
    [[nodiscard]] const SessionConfig& config() const noexcept { return config_; }
    [[nodiscard]] const SessionStats& stats() const noexcept { return stats_; }
    [[nodiscard]] const SequenceManager& sequences() const noexcept { return sequences_; }

    [[nodiscard]] SessionId session_id() const noexcept {
        return SessionId{config_.sender_comp_id, config_.target_comp_id, config_.begin_string};
    }

private:
    // ========================================================================
    // State Machine
    // ========================================================================

    void transition(SessionEvent event) noexcept {
        SessionState prev = state_;
        SessionState next = next_state(state_, event);

        if (next != prev) {
            state_ = next;
            if (callbacks_.on_state_change) {
                callbacks_.on_state_change(prev, next);
            }
        }
    }

    // ========================================================================
    // Admin Message Handling
    // ========================================================================

    void handle_admin_message(const ParsedMessage& msg) noexcept {
        switch (msg.msg_type()) {
            case msg_type::Logon:
                handle_logon(msg);
                break;
            case msg_type::Logout:
                handle_logout(msg);
                break;
            case msg_type::Heartbeat:
                handle_heartbeat(msg);
                break;
            case msg_type::TestRequest:
                handle_test_request(msg);
                break;
            case msg_type::ResendRequest:
                handle_resend_request(msg);
                break;
            case msg_type::SequenceReset:
                handle_sequence_reset(msg);
                break;
            case msg_type::Reject:
                handle_reject(msg);
                break;
        }
    }

    void handle_logon(const ParsedMessage& msg) noexcept {
        if (state_ == SessionState::LogonSent) {
            // Response to our logon
            if (auto v = msg.get_int(108)) {  // HeartBtInt
                heartbeat_timer_.set_interval(static_cast<int>(*v));
            }
            transition(SessionEvent::LogonReceived);
            heartbeat_timer_.reset();

            if (callbacks_.on_logon) {
                callbacks_.on_logon();
            }
        } else if (state_ == SessionState::SocketConnected) {
            // Incoming logon - we're the acceptor
            transition(SessionEvent::LogonReceived);

            // Send logon response
            auto response = fix44::Logon::Builder{}
                .sender_comp_id(config_.sender_comp_id)
                .target_comp_id(config_.target_comp_id)
                .msg_seq_num(sequences_.next_outbound())
                .sending_time(current_timestamp())
                .encrypt_method(0)
                .heart_bt_int(config_.heart_bt_int)
                .build(assembler_);

            send_message(response);
            transition(SessionEvent::LogonAcknowledged);
            heartbeat_timer_.reset();

            if (callbacks_.on_logon) {
                callbacks_.on_logon();
            }
        }
    }

    void handle_logout(const ParsedMessage& msg) noexcept {
        std::string_view text = msg.get_string(tag::Text::value);

        if (state_ == SessionState::LogoutPending) {
            // Response to our logout
            transition(SessionEvent::LogoutReceived);
        } else {
            // Incoming logout - send response
            transition(SessionEvent::LogoutReceived);

            auto response = fix44::Logout::Builder{}
                .sender_comp_id(config_.sender_comp_id)
                .target_comp_id(config_.target_comp_id)
                .msg_seq_num(sequences_.next_outbound())
                .sending_time(current_timestamp())
                .build(assembler_);

            send_message(response);
            transition(SessionEvent::LogoutSent);
        }

        if (callbacks_.on_logout) {
            callbacks_.on_logout(text);
        }
    }

    void handle_heartbeat(const ParsedMessage& msg) noexcept {
        ++stats_.heartbeats_received;
        // TestReqID handling if present
        // Nothing else to do - heartbeat timer already updated
    }

    void handle_test_request(const ParsedMessage& msg) noexcept {
        // Send heartbeat with TestReqID
        std::string_view test_req_id = msg.get_string(tag::TestReqID::value);

        auto response = fix44::Heartbeat::Builder{}
            .sender_comp_id(config_.sender_comp_id)
            .target_comp_id(config_.target_comp_id)
            .msg_seq_num(sequences_.next_outbound())
            .sending_time(current_timestamp())
            .test_req_id(test_req_id)
            .build(assembler_);

        send_message(response);
    }

    void handle_resend_request(const ParsedMessage& msg) noexcept {
        ++stats_.resend_requests_sent;
        // TODO: Implement message resend from store
        // For now, send sequence reset (gap fill)

        auto begin_seq = msg.get_int(7);  // BeginSeqNo
        auto end_seq = msg.get_int(16);   // EndSeqNo

        if (begin_seq && end_seq) {
            auto response = fix44::SequenceReset::Builder{}
                .sender_comp_id(config_.sender_comp_id)
                .target_comp_id(config_.target_comp_id)
                .msg_seq_num(static_cast<uint32_t>(*begin_seq))
                .sending_time(current_timestamp())
                .new_seq_no(sequences_.current_outbound())
                .gap_fill_flag(true)
                .build(assembler_);

            send_message(response);
        }
    }

    void handle_sequence_reset(const ParsedMessage& msg) noexcept {
        ++stats_.sequence_resets;

        if (auto new_seq = msg.get_int(36)) {  // NewSeqNo
            bool gap_fill = msg.get_char(123) == 'Y';  // GapFillFlag

            if (gap_fill) {
                // Gap fill - update expected without error
                sequences_.set_inbound(static_cast<uint32_t>(*new_seq));
            } else {
                // Hard reset
                sequences_.set_inbound(static_cast<uint32_t>(*new_seq));
            }
        }
    }

    void handle_reject(const ParsedMessage& msg) noexcept {
        // Session-level reject - log and continue
        [[maybe_unused]] std::string_view text = msg.get_string(tag::Text::value);
        if (callbacks_.on_error) {
            callbacks_.on_error(SessionError{SessionErrorCode::InvalidState});
        }
    }

    // ========================================================================
    // Application Message Handling
    // ========================================================================

    void handle_app_message(const ParsedMessage& msg) noexcept {
        if (callbacks_.on_app_message) {
            callbacks_.on_app_message(msg);
        }
    }

    // ========================================================================
    // Error Handling
    // ========================================================================

    void handle_parse_error(const ParseError& error) noexcept {
        if (callbacks_.on_error) {
            callbacks_.on_error(SessionError{SessionErrorCode::InvalidState});
        }
    }

    void handle_sequence_gap(uint32_t received) noexcept {
        auto [begin, end] = sequences_.gap_range(received);

        auto request = fix44::ResendRequest::Builder{}
            .sender_comp_id(config_.sender_comp_id)
            .target_comp_id(config_.target_comp_id)
            .msg_seq_num(sequences_.next_outbound())
            .sending_time(current_timestamp())
            .begin_seq_no(begin)
            .end_seq_no(end)
            .build(assembler_);

        send_message(request);
    }

    void handle_sequence_error(uint32_t received) noexcept {
        // Sequence too low - reject or logout
        if (callbacks_.on_error) {
            callbacks_.on_error(SessionError{
                SessionErrorCode::SequenceGap,
                sequences_.expected_inbound(),
                received
            });
        }
    }

    // ========================================================================
    // Message Sending
    // ========================================================================

    bool send_message(std::span<const char> msg) noexcept {
        if (!callbacks_.on_send) return false;

        bool sent = callbacks_.on_send(msg);
        if (sent) {
            heartbeat_timer_.message_sent();
            ++stats_.messages_sent;
            stats_.bytes_sent += msg.size();
        }
        return sent;
    }

    void send_heartbeat(std::string_view test_req_id = "") noexcept {
        auto msg = fix44::Heartbeat::Builder{}
            .sender_comp_id(config_.sender_comp_id)
            .target_comp_id(config_.target_comp_id)
            .msg_seq_num(sequences_.next_outbound())
            .sending_time(current_timestamp())
            .test_req_id(test_req_id)
            .build(assembler_);

        send_message(msg);
        ++stats_.heartbeats_sent;
    }

    void send_test_request() noexcept {
        // Generate test request ID
        char id_buf[32];
        auto len = std::snprintf(id_buf, sizeof(id_buf), "TEST%lu",
            static_cast<unsigned long>(stats_.test_requests_sent + 1));

        auto msg = fix44::TestRequest::Builder{}
            .sender_comp_id(config_.sender_comp_id)
            .target_comp_id(config_.target_comp_id)
            .msg_seq_num(sequences_.next_outbound())
            .sending_time(current_timestamp())
            .test_req_id(std::string_view{id_buf, static_cast<size_t>(len)})
            .build(assembler_);

        send_message(msg);
        heartbeat_timer_.test_request_sent();
        ++stats_.test_requests_sent;
    }

    // ========================================================================
    // Utilities
    // ========================================================================

    [[nodiscard]] std::string_view current_timestamp() noexcept {
        // Format: YYYYMMDD-HH:MM:SS.sss
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::tm tm_buf;
        gmtime_r(&time_t_now, &tm_buf);

        int len = std::snprintf(timestamp_buf_, sizeof(timestamp_buf_),
            "%04d%02d%02d-%02d:%02d:%02d.%03d",
            tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
            tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
            static_cast<int>(ms.count()));

        return std::string_view{timestamp_buf_, static_cast<size_t>(len)};
    }

    // ========================================================================
    // Member Variables
    // ========================================================================

    SessionConfig config_;
    SessionState state_;
    SessionCallbacks callbacks_;
    HeartbeatTimer heartbeat_timer_;
    MessageAssembler assembler_;
    SequenceManager sequences_;
    SessionStats stats_;
    char timestamp_buf_[32]{};
};

} // namespace nfx
