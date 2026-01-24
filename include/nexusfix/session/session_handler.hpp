/*
    NexusFIX Session Handler Concepts

    Zero-overhead callbacks using C++20 concepts instead of std::function.
    Eliminates type erasure and virtual dispatch overhead on hot path.

    Usage:
        struct MyHandler {
            void on_app_message(const ParsedMessage& msg) noexcept { ... }
            void on_state_change(SessionState from, SessionState to) noexcept { ... }
            bool on_send(std::span<const char> data) noexcept { ... }
            void on_error(const SessionError& err) noexcept { ... }
            void on_logon() noexcept { ... }
            void on_logout(std::string_view reason) noexcept { ... }
        };

        SessionManager<MyHandler> session{config, handler};
*/

#pragma once

#include <concepts>
#include <span>
#include <string_view>

#include "nexusfix/session/state.hpp"
#include "nexusfix/types/error.hpp"

namespace nfx {

// Forward declaration
class ParsedMessage;

// ============================================================================
// Session Handler Concepts
// ============================================================================

/// Concept for application message callback
template <typename T>
concept HasOnAppMessage = requires(T& handler, const ParsedMessage& msg) {
    { handler.on_app_message(msg) } noexcept;
};

/// Concept for state change callback
template <typename T>
concept HasOnStateChange = requires(T& handler, SessionState from, SessionState to) {
    { handler.on_state_change(from, to) } noexcept;
};

/// Concept for send callback (returns bool for success)
template <typename T>
concept HasOnSend = requires(T& handler, std::span<const char> data) {
    { handler.on_send(data) } noexcept -> std::same_as<bool>;
};

/// Concept for error callback
template <typename T>
concept HasOnError = requires(T& handler, const SessionError& err) {
    { handler.on_error(err) } noexcept;
};

/// Concept for logon callback
template <typename T>
concept HasOnLogon = requires(T& handler) {
    { handler.on_logon() } noexcept;
};

/// Concept for logout callback
template <typename T>
concept HasOnLogout = requires(T& handler, std::string_view reason) {
    { handler.on_logout(reason) } noexcept;
};

/// Complete session handler concept - requires all callbacks
template <typename T>
concept SessionHandler = HasOnAppMessage<T> &&
                         HasOnStateChange<T> &&
                         HasOnSend<T> &&
                         HasOnError<T> &&
                         HasOnLogon<T> &&
                         HasOnLogout<T>;

// ============================================================================
// Null Session Handler (No-op implementation)
// ============================================================================

/// No-op handler for testing or minimal sessions
struct NullSessionHandler {
    void on_app_message(const ParsedMessage&) noexcept {}
    void on_state_change(SessionState, SessionState) noexcept {}
    bool on_send(std::span<const char>) noexcept { return true; }
    void on_error(const SessionError&) noexcept {}
    void on_logon() noexcept {}
    void on_logout(std::string_view) noexcept {}
};

static_assert(SessionHandler<NullSessionHandler>, "NullSessionHandler must satisfy SessionHandler concept");

// ============================================================================
// Function Pointer Handler (Alternative to std::function)
// ============================================================================

/// Handler using raw function pointers with user data
/// Lower overhead than std::function, suitable for C-style callbacks
struct FunctionPtrHandler {
    using AppMessageFn = void(*)(void* ctx, const ParsedMessage&) noexcept;
    using StateChangeFn = void(*)(void* ctx, SessionState, SessionState) noexcept;
    using SendFn = bool(*)(void* ctx, std::span<const char>) noexcept;
    using ErrorFn = void(*)(void* ctx, const SessionError&) noexcept;
    using LogonFn = void(*)(void* ctx) noexcept;
    using LogoutFn = void(*)(void* ctx, std::string_view) noexcept;

    void* context{nullptr};
    AppMessageFn app_message_fn{nullptr};
    StateChangeFn state_change_fn{nullptr};
    SendFn send_fn{nullptr};
    ErrorFn error_fn{nullptr};
    LogonFn logon_fn{nullptr};
    LogoutFn logout_fn{nullptr};

    void on_app_message(const ParsedMessage& msg) noexcept {
        if (app_message_fn) [[likely]] app_message_fn(context, msg);
    }

    void on_state_change(SessionState from, SessionState to) noexcept {
        if (state_change_fn) [[likely]] state_change_fn(context, from, to);
    }

    bool on_send(std::span<const char> data) noexcept {
        if (send_fn) [[likely]] return send_fn(context, data);
        return false;
    }

    void on_error(const SessionError& err) noexcept {
        if (error_fn) [[likely]] error_fn(context, err);
    }

    void on_logon() noexcept {
        if (logon_fn) [[likely]] logon_fn(context);
    }

    void on_logout(std::string_view reason) noexcept {
        if (logout_fn) [[likely]] logout_fn(context, reason);
    }
};

static_assert(SessionHandler<FunctionPtrHandler>, "FunctionPtrHandler must satisfy SessionHandler concept");

} // namespace nfx
