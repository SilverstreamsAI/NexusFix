#pragma once

#include <span>
#include <string_view>

#include "nexusfix/session/coroutine.hpp"
#include "nexusfix/transport/socket.hpp"
#include "nexusfix/types/error.hpp"

namespace nfx {

// ============================================================================
// AsyncTransport - Awaitable Wrapper Around ITransport
// ============================================================================

/// Provides coroutine-awaitable wrappers for ITransport operations.
/// Delegates to the underlying blocking ITransport with cooperative yields
/// between retries for WouldBlock results.
class AsyncTransport {
public:
    explicit AsyncTransport(ITransport& transport) noexcept
        : transport_{transport} {}

    AsyncTransport(const AsyncTransport&) = delete;
    AsyncTransport& operator=(const AsyncTransport&) = delete;

    /// Async connect to remote endpoint
    [[nodiscard]] Task<TransportResult<void>> connect_async(
        std::string_view host, uint16_t port) {

        auto result = transport_.connect(host, port);
        while (!result.has_value() &&
               result.error().code == TransportErrorCode::WouldBlock) {
            co_await Yield{};
            result = transport_.connect(host, port);
        }
        co_return result;
    }

    /// Async send data
    [[nodiscard]] Task<TransportResult<size_t>> send_async(
        std::span<const char> data) {

        auto result = transport_.send(data);
        while (!result.has_value() &&
               result.error().code == TransportErrorCode::WouldBlock) {
            co_await Yield{};
            result = transport_.send(data);
        }
        co_return result;
    }

    /// Async receive data
    [[nodiscard]] Task<TransportResult<size_t>> receive_async(
        std::span<char> buffer) {

        auto result = transport_.receive(buffer);
        while (!result.has_value() &&
               result.error().code == TransportErrorCode::WouldBlock) {
            co_await Yield{};
            result = transport_.receive(buffer);
        }
        co_return result;
    }

    /// Check if underlying transport is connected
    [[nodiscard]] bool is_connected() const noexcept {
        return transport_.is_connected();
    }

    /// Disconnect the underlying transport
    void disconnect() noexcept {
        transport_.disconnect();
    }

    /// Get underlying transport reference
    [[nodiscard]] ITransport& transport() noexcept { return transport_; }
    [[nodiscard]] const ITransport& transport() const noexcept { return transport_; }

private:
    ITransport& transport_;
};

} // namespace nfx
