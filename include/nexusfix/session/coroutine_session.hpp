#pragma once

#include <chrono>
#include <cstdio>
#include <span>
#include <string_view>

#include "nexusfix/session/async_primitives.hpp"
#include "nexusfix/session/coroutine.hpp"
#include "nexusfix/session/sequence.hpp"
#include "nexusfix/session/session_handler.hpp"
#include "nexusfix/session/session_manager.hpp"
#include "nexusfix/session/state.hpp"
#include "nexusfix/transport/async_transport.hpp"
#include "nexusfix/types/error.hpp"

#include "nexusfix/messages/common/header.hpp"
#include "nexusfix/messages/common/trailer.hpp"
#include "nexusfix/messages/fix44/logon.hpp"
#include "nexusfix/messages/fix44/heartbeat.hpp"

#include "nexusfix/store/i_message_store.hpp"
#include "nexusfix/util/rdtsc_timestamp.hpp"

namespace nfx {

// ============================================================================
// CoroutineSession - Coroutine-based FIX Session Lifecycle
// ============================================================================

/// Coroutine-based FIX session implementation.
/// Alternative to SessionManager for coroutine-based architectures.
/// Reuses all existing types: SessionConfig, SessionState, SessionEvent,
/// SequenceManager, HeartbeatTimer, MessageAssembler, etc.
template <SessionHandler Handler>
class CoroutineSession {
public:
    CoroutineSession(const SessionConfig& config,
                     Handler& handler,
                     AsyncTransport& transport) noexcept
        : config_{config}
        , handler_{handler}
        , transport_{transport}
        , state_{SessionState::Disconnected}
        , heartbeat_timer_{config.heart_bt_int}
        , assembler_{}
        , sequences_{}
        , stats_{}
        , message_store_{nullptr} {}

    CoroutineSession(const CoroutineSession&) = delete;
    CoroutineSession& operator=(const CoroutineSession&) = delete;

    // ========================================================================
    // Session Control
    // ========================================================================

    /// Run the full session lifecycle: connect -> logon -> active -> logout
    [[nodiscard]] Task<SessionResult<void>> run(
        std::string_view host, uint16_t port) {

        // Phase 1: Connect
        auto connect_result = co_await connect_phase(host, port);
        if (!connect_result.has_value()) {
            co_return std::unexpected{connect_result.error()};
        }

        // Phase 2: Logon
        auto logon_result = co_await logon_phase();
        if (!logon_result.has_value()) {
            co_return std::unexpected{logon_result.error()};
        }

        // Phase 3: Active (runs until shutdown or error)
        auto active_result = co_await active_phase();
        if (!active_result.has_value()) {
            // If error, still attempt graceful logout
            co_await logout_phase();
            co_return std::unexpected{active_result.error()};
        }

        // Phase 4: Logout
        co_await logout_phase();
        co_return SessionResult<void>{};
    }

    /// Request graceful shutdown
    void request_shutdown() noexcept {
        shutdown_event_.set();
    }

    /// Set message store for resend support
    void set_message_store(store::IMessageStore* store) noexcept {
        message_store_ = store;
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

    // ========================================================================
    // Message Sending (thread-safe via AsyncMutex)
    // ========================================================================

    /// Send an application message (serializes via send_mutex_)
    template <typename MsgBuilder>
    Task<SessionResult<void>> send_app_message(MsgBuilder& builder) {
        if (!can_send_app_messages(state_)) {
            co_return std::unexpected{SessionError{SessionErrorCode::InvalidState}};
        }

        auto lock = co_await send_mutex_.scoped_lock();

        auto msg = builder
            .sender_comp_id(config_.sender_comp_id)
            .target_comp_id(config_.target_comp_id)
            .msg_seq_num(sequences_.next_outbound())
            .sending_time(current_timestamp())
            .build(assembler_);

        auto send_result = co_await send_raw(msg);
        if (!send_result.has_value()) {
            co_return std::unexpected{SessionError{SessionErrorCode::NotConnected}};
        }

        co_return SessionResult<void>{};
    }

private:
    // ========================================================================
    // Phase 1: Connect
    // ========================================================================

    Task<SessionResult<void>> connect_phase(
        std::string_view host, uint16_t port) {

        auto result = co_await transport_.connect_async(host, port);
        if (!result.has_value()) {
            transition(SessionEvent::Error);
            co_return std::unexpected{
                SessionError{SessionErrorCode::NotConnected}};
        }

        transition(SessionEvent::Connect);
        co_return SessionResult<void>{};
    }

    // ========================================================================
    // Phase 2: Logon
    // ========================================================================

    Task<SessionResult<void>> logon_phase() {
        // Build and send logon
        auto msg = fix44::Logon::Builder{}
            .sender_comp_id(config_.sender_comp_id)
            .target_comp_id(config_.target_comp_id)
            .msg_seq_num(sequences_.next_outbound())
            .sending_time(current_timestamp())
            .encrypt_method(0)
            .heart_bt_int(config_.heart_bt_int)
            .reset_seq_num_flag(config_.reset_seq_num_on_logon)
            .build(assembler_);

        auto send_result = co_await send_raw(msg);
        if (!send_result.has_value()) {
            co_return std::unexpected{SessionError{SessionErrorCode::NotConnected}};
        }

        transition(SessionEvent::LogonSent);

        // Wait for logon response with timeout
        // Use deadline-based polling instead of when_any to avoid
        // deep coroutine nesting complexity
        using Clock = std::chrono::steady_clock;
        auto deadline = Clock::now() +
            std::chrono::seconds{config_.logon_timeout};

        co_await wait_for_logon_with_deadline(deadline);

        if (state_ != SessionState::Active) {
            transition(SessionEvent::HeartbeatTimeout);
            co_return std::unexpected{SessionError{SessionErrorCode::LogonTimeout}};
        }

        heartbeat_timer_.reset();
        handler_.on_logon();
        co_return SessionResult<void>{};
    }

    /// Wait for logon response with deadline-based timeout
    Task<void> wait_for_logon_with_deadline(
        std::chrono::steady_clock::time_point deadline) {

        using Clock = std::chrono::steady_clock;
        std::array<char, 4096> buf{};

        while (state_ == SessionState::LogonSent) {
            if (Clock::now() >= deadline) {
                co_return;  // Timed out
            }

            auto recv_result = co_await transport_.receive_async(buf);
            if (!recv_result.has_value()) {
                transition(SessionEvent::Disconnect);
                co_return;
            }

            if (*recv_result == 0) {
                co_await Yield{};
                continue;
            }

            auto data = std::span<const char>{buf.data(), *recv_result};
            heartbeat_timer_.message_received();
            ++stats_.messages_received;
            stats_.bytes_received += data.size();

            auto parse_result = ParsedMessage::parse(data);
            if (!parse_result.has_value()) {
                co_await Yield{};
                continue;
            }

            auto& parsed = *parse_result;
            if (parsed.msg_type() == msg_type::Logon) {
                if (auto v = parsed.get_int(108)) {
                    heartbeat_timer_.set_interval(static_cast<int>(*v));
                }
                transition(SessionEvent::LogonReceived);
            } else if (parsed.msg_type() == msg_type::Logout) {
                transition(SessionEvent::LogonRejected);
            }
        }
    }

    // ========================================================================
    // Phase 3: Active
    // ========================================================================

    Task<SessionResult<void>> active_phase() {
        // Run heartbeat loop, message receiver, and shutdown wait concurrently
        auto hb = heartbeat_loop();
        auto rx = message_receiver_loop();
        auto shutdown = wait_for_shutdown();

        std::vector<Task<void>> tasks;
        tasks.push_back(std::move(hb));
        tasks.push_back(std::move(rx));
        tasks.push_back(std::move(shutdown));

        size_t winner = co_await when_any(std::move(tasks));

        if (winner == 0) {
            // Heartbeat loop exited (timeout)
            co_return std::unexpected{
                SessionError{SessionErrorCode::HeartbeatTimeout}};
        }

        if (winner == 1) {
            // Receiver exited (disconnect or error)
            if (state_ == SessionState::Active) {
                co_return std::unexpected{
                    SessionError{SessionErrorCode::Disconnected}};
            }
        }

        // winner == 2: graceful shutdown requested
        co_return SessionResult<void>{};
    }

    /// Periodically send heartbeats and detect timeout
    Task<void> heartbeat_loop() {
        while (state_ == SessionState::Active) {
            if (heartbeat_timer_.has_timed_out()) {
                transition(SessionEvent::HeartbeatTimeout);
                co_return;
            }

            if (heartbeat_timer_.should_send_test_request()) {
                co_await send_test_request();
            } else if (heartbeat_timer_.should_send_heartbeat()) {
                co_await send_heartbeat();
            }

            co_await Yield{};
        }
    }

    /// Receive and route incoming messages
    Task<void> message_receiver_loop() {
        std::array<char, 4096> buf{};

        while (state_ == SessionState::Active) {
            auto recv_result = co_await transport_.receive_async(buf);
            if (!recv_result.has_value()) {
                transition(SessionEvent::Disconnect);
                co_return;
            }

            if (*recv_result == 0) {
                co_await Yield{};
                continue;
            }

            auto data = std::span<const char>{buf.data(), *recv_result};
            heartbeat_timer_.message_received();
            ++stats_.messages_received;
            stats_.bytes_received += data.size();

            auto parse_result = ParsedMessage::parse(data);
            if (!parse_result.has_value()) {
                handler_.on_error(SessionError{SessionErrorCode::InvalidState});
                continue;
            }

            auto& parsed = *parse_result;

            // Validate sequence
            auto seq_result = sequences_.validate_inbound(parsed.msg_seq_num());
            if (seq_result == SequenceManager::SequenceResult::GapDetected) {
                co_await handle_sequence_gap(parsed.msg_seq_num());
            } else if (seq_result == SequenceManager::SequenceResult::TooLow) {
                if (!parsed.header().poss_dup_flag) {
                    handler_.on_error(SessionError{
                        SessionErrorCode::SequenceGap,
                        sequences_.expected_inbound(),
                        parsed.msg_seq_num()});
                    continue;
                }
            }

            // Route message
            char mt = parsed.msg_type();
            if (msg_type::is_admin(mt)) {
                co_await handle_admin_message(parsed);
            } else {
                handler_.on_app_message(parsed);
            }
        }
    }

    /// Wait for shutdown event
    Task<void> wait_for_shutdown() {
        co_await shutdown_event_;
    }

    // ========================================================================
    // Phase 4: Logout
    // ========================================================================

    Task<SessionResult<void>> logout_phase() {
        if (state_ != SessionState::Active &&
            state_ != SessionState::LogoutReceived) {
            co_return SessionResult<void>{};
        }

        if (state_ == SessionState::Active) {
            auto msg = fix44::Logout::Builder{}
                .sender_comp_id(config_.sender_comp_id)
                .target_comp_id(config_.target_comp_id)
                .msg_seq_num(sequences_.next_outbound())
                .sending_time(current_timestamp())
                .build(assembler_);

            co_await send_raw(msg);
            transition(SessionEvent::LogoutSent);

            // Wait for logout response with deadline-based timeout
            using Clock = std::chrono::steady_clock;
            auto deadline = Clock::now() +
                std::chrono::seconds{config_.logout_timeout};
            co_await wait_for_logout_with_deadline(deadline);
        }

        handler_.on_logout("Session ended");
        transport_.disconnect();
        transition(SessionEvent::Disconnect);
        co_return SessionResult<void>{};
    }

    /// Wait for logout response with deadline-based timeout
    Task<void> wait_for_logout_with_deadline(
        std::chrono::steady_clock::time_point deadline) {

        using Clock = std::chrono::steady_clock;
        std::array<char, 4096> buf{};

        while (state_ == SessionState::LogoutPending) {
            if (Clock::now() >= deadline) {
                co_return;  // Timed out
            }

            auto recv_result = co_await transport_.receive_async(buf);
            if (!recv_result.has_value()) {
                co_return;
            }

            if (*recv_result == 0) {
                co_await Yield{};
                continue;
            }

            auto data = std::span<const char>{buf.data(), *recv_result};
            auto parse_result = ParsedMessage::parse(data);
            if (!parse_result.has_value()) {
                co_await Yield{};
                continue;
            }

            if (parse_result->msg_type() == msg_type::Logout) {
                transition(SessionEvent::LogoutReceived);
            }
        }
    }

    // ========================================================================
    // Admin Message Handling
    // ========================================================================

    Task<void> handle_admin_message(const ParsedMessage& msg) {
        switch (msg.msg_type()) {
            case msg_type::Heartbeat:
                ++stats_.heartbeats_received;
                break;

            case msg_type::TestRequest:
                co_await handle_test_request(msg);
                break;

            case msg_type::Logout:
                handle_logout(msg);
                break;

            case msg_type::ResendRequest:
                co_await handle_resend_request(msg);
                break;

            case msg_type::SequenceReset:
                handle_sequence_reset(msg);
                break;

            case msg_type::Reject:
                handler_.on_error(SessionError{SessionErrorCode::InvalidState});
                break;

            default:
                break;
        }
        co_return;
    }

    Task<void> handle_test_request(const ParsedMessage& msg) {
        std::string_view test_req_id = msg.get_string(tag::TestReqID::value);
        co_await send_heartbeat(test_req_id);
    }

    void handle_logout(const ParsedMessage& msg) {
        std::string_view text = msg.get_string(tag::Text::value);
        transition(SessionEvent::LogoutReceived);
        handler_.on_logout(text);
    }

    void handle_sequence_reset(const ParsedMessage& msg) {
        ++stats_.sequence_resets;
        if (auto new_seq = msg.get_int(36)) {
            sequences_.set_inbound(static_cast<uint32_t>(*new_seq));
        }
    }

    Task<void> handle_resend_request(const ParsedMessage& msg) {
        ++stats_.resend_requests_sent;

        auto begin_seq = msg.get_int(7);
        auto end_seq = msg.get_int(16);
        if (!begin_seq || !end_seq) co_return;

        uint32_t begin = static_cast<uint32_t>(*begin_seq);
        uint32_t end = static_cast<uint32_t>(*end_seq);

        if (message_store_) {
            auto messages = message_store_->retrieve_range(begin, end);
            if (!messages.empty()) {
                for (const auto& stored_msg : messages) {
                    co_await send_raw(stored_msg);
                }
                co_return;
            }
        }

        // Fallback: SequenceReset gap fill
        auto lock = co_await send_mutex_.scoped_lock();
        auto response = fix44::SequenceReset::Builder{}
            .sender_comp_id(config_.sender_comp_id)
            .target_comp_id(config_.target_comp_id)
            .msg_seq_num(begin)
            .sending_time(current_timestamp())
            .new_seq_no(sequences_.current_outbound())
            .gap_fill_flag(true)
            .build(assembler_);

        co_await send_raw_unlocked(response);
    }

    Task<void> handle_sequence_gap(uint32_t received) {
        auto [begin, end] = sequences_.gap_range(received);

        auto lock = co_await send_mutex_.scoped_lock();
        auto request = fix44::ResendRequest::Builder{}
            .sender_comp_id(config_.sender_comp_id)
            .target_comp_id(config_.target_comp_id)
            .msg_seq_num(sequences_.next_outbound())
            .sending_time(current_timestamp())
            .begin_seq_no(begin)
            .end_seq_no(end)
            .build(assembler_);

        co_await send_raw_unlocked(request);
    }

    // ========================================================================
    // Send Helpers
    // ========================================================================

    /// Send raw message data (acquires send_mutex_)
    Task<TransportResult<size_t>> send_raw(std::span<const char> msg) {
        auto lock = co_await send_mutex_.scoped_lock();
        co_return co_await send_raw_unlocked(msg);
    }

    /// Send raw message data (caller must hold send_mutex_)
    Task<TransportResult<size_t>> send_raw_unlocked(std::span<const char> msg) {
        if (message_store_) {
            uint32_t seq_num = sequences_.current_outbound();
            message_store_->store(seq_num, msg);
        }

        auto result = co_await transport_.send_async(msg);
        if (result.has_value()) {
            heartbeat_timer_.message_sent();
            ++stats_.messages_sent;
            stats_.bytes_sent += msg.size();
        }
        co_return result;
    }

    Task<void> send_heartbeat(std::string_view test_req_id = "") {
        auto lock = co_await send_mutex_.scoped_lock();
        auto msg = fix44::Heartbeat::Builder{}
            .sender_comp_id(config_.sender_comp_id)
            .target_comp_id(config_.target_comp_id)
            .msg_seq_num(sequences_.next_outbound())
            .sending_time(current_timestamp())
            .test_req_id(test_req_id)
            .build(assembler_);

        co_await send_raw_unlocked(msg);
        ++stats_.heartbeats_sent;
    }

    Task<void> send_test_request() {
        char id_buf[32];
        auto len = std::snprintf(id_buf, sizeof(id_buf), "TEST%lu",
            static_cast<unsigned long>(stats_.test_requests_sent + 1));

        auto lock = co_await send_mutex_.scoped_lock();
        auto msg = fix44::TestRequest::Builder{}
            .sender_comp_id(config_.sender_comp_id)
            .target_comp_id(config_.target_comp_id)
            .msg_seq_num(sequences_.next_outbound())
            .sending_time(current_timestamp())
            .test_req_id(std::string_view{id_buf, static_cast<size_t>(len)})
            .build(assembler_);

        co_await send_raw_unlocked(msg);
        heartbeat_timer_.test_request_sent();
        ++stats_.test_requests_sent;
    }

    // ========================================================================
    // State Machine
    // ========================================================================

    void transition(SessionEvent event) noexcept {
        SessionState prev = state_;
        SessionState next = next_state(state_, event);
        if (next != prev) {
            state_ = next;
            handler_.on_state_change(prev, next);
        }
    }

    // ========================================================================
    // Utilities
    // ========================================================================

    [[nodiscard]] std::string_view current_timestamp() noexcept {
        return timestamp_generator_.get();
    }

    // ========================================================================
    // Member Variables
    // ========================================================================

    const SessionConfig& config_;
    Handler& handler_;
    AsyncTransport& transport_;

    SessionState state_;
    HeartbeatTimer heartbeat_timer_;
    MessageAssembler assembler_;
    SequenceManager sequences_;
    SessionStats stats_;
    util::RdtscTimestamp timestamp_generator_;
    store::IMessageStore* message_store_;

    AsyncMutex send_mutex_;
    Event shutdown_event_;
};

// ============================================================================
// session_with_recovery - Reconnection with Exponential Backoff
// ============================================================================

/// Run a session with automatic reconnection and exponential backoff.
/// Returns when max_reconnect_attempts is exhausted or shutdown is requested.
template <SessionHandler Handler>
Task<SessionResult<void>> session_with_recovery(
    CoroutineSession<Handler>& session,
    std::string_view host,
    uint16_t port) {

    const auto& config = session.config();
    int attempts = 0;

    while (attempts < config.max_reconnect_attempts) {
        auto result = co_await session.run(host, port);

        if (result.has_value()) {
            // Graceful shutdown
            co_return SessionResult<void>{};
        }

        ++attempts;
        if (attempts >= config.max_reconnect_attempts) {
            co_return std::unexpected{result.error()};
        }

        // Exponential backoff: base * 2^attempt (capped at 60s)
        int delay_sec = config.reconnect_interval * (1 << attempts);
        if (delay_sec > 60) delay_sec = 60;

        // Cooperative wait
        using Clock = std::chrono::steady_clock;
        auto deadline = Clock::now() + std::chrono::seconds{delay_sec};
        while (Clock::now() < deadline) {
            co_await Yield{};
        }
    }

    co_return std::unexpected{SessionError{SessionErrorCode::Disconnected}};
}

} // namespace nfx
