#pragma once

/// @file winsock_transport.hpp
/// @brief Windows Winsock2 TCP transport implementation
///
/// This file provides TCP socket functionality for Windows platforms.
/// For POSIX platforms (Linux, macOS), use tcp_transport.hpp instead.

#include "nexusfix/platform/platform.hpp"

#if NFX_PLATFORM_WINDOWS

#include "nexusfix/platform/socket_types.hpp"
#include "nexusfix/platform/error_mapping.hpp"
#include "nexusfix/transport/socket.hpp"
#include "nexusfix/transport/winsock_init.hpp"

#include <cstring>
#include <cstdio>
#include <algorithm>

namespace nfx {

// ============================================================================
// Winsock Socket (Windows)
// ============================================================================

/// TCP socket implementation using Windows Winsock2
class WinsockSocket {
public:
    WinsockSocket() noexcept
        : fd_{INVALID_SOCKET_HANDLE}
        , state_{ConnectionState::Disconnected} {}

    ~WinsockSocket() {
        close();
    }

    // Non-copyable
    WinsockSocket(const WinsockSocket&) = delete;
    WinsockSocket& operator=(const WinsockSocket&) = delete;

    // Movable
    WinsockSocket(WinsockSocket&& other) noexcept
        : fd_{other.fd_}
        , state_{other.state_}
        , options_{other.options_}
    {
        other.fd_ = INVALID_SOCKET_HANDLE;
        other.state_ = ConnectionState::Disconnected;
    }

    WinsockSocket& operator=(WinsockSocket&& other) noexcept {
        if (this != &other) {
            close();
            fd_ = other.fd_;
            state_ = other.state_;
            options_ = other.options_;
            other.fd_ = INVALID_SOCKET_HANDLE;
            other.state_ = ConnectionState::Disconnected;
        }
        return *this;
    }

    /// Create socket
    [[nodiscard]] TransportResult<void> create() noexcept {
        // Ensure Winsock is initialized
        if (!WinsockInit::ensure()) {
            return std::unexpected{WinsockInit::make_init_error()};
        }

        fd_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (!is_valid_socket(fd_)) {
            return std::unexpected{make_socket_error()};
        }
        return {};
    }

    /// Connect to remote host
    [[nodiscard]] TransportResult<void> connect(
        std::string_view host,
        uint16_t port) noexcept
    {
        if (!is_valid_socket(fd_)) {
            auto result = create();
            if (!result) return result;
        }

        state_ = ConnectionState::Connecting;

        // Resolve hostname
        struct addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        char port_str[8];
        std::snprintf(port_str, sizeof(port_str), "%u", port);

        // Need null-terminated string for getaddrinfo
        char host_buf[256];
        size_t host_len = std::min(host.size(), sizeof(host_buf) - 1);
        std::memcpy(host_buf, host.data(), host_len);
        host_buf[host_len] = '\0';

        struct addrinfo* result = nullptr;
        int ret = ::getaddrinfo(host_buf, port_str, &hints, &result);
        if (ret != 0) {
            state_ = ConnectionState::Error;
            return std::unexpected{make_gai_error(ret)};
        }

        // Try to connect
        ret = ::connect(fd_, result->ai_addr, static_cast<int>(result->ai_addrlen));
        ::freeaddrinfo(result);

        if (ret == SOCKET_ERROR) {
            state_ = ConnectionState::Error;
            return std::unexpected{make_socket_error()};
        }

        apply_options();
        state_ = ConnectionState::Connected;
        return {};
    }

    /// Close socket
    void close() noexcept {
        if (is_valid_socket(fd_)) {
            state_ = ConnectionState::Disconnecting;
            // Graceful shutdown
            ::shutdown(fd_, SD_BOTH);
            close_socket(fd_);
            fd_ = INVALID_SOCKET_HANDLE;
            state_ = ConnectionState::Disconnected;
        }
    }

    /// Check if connected
    [[nodiscard]] bool is_connected() const noexcept {
        return state_ == ConnectionState::Connected && is_valid_socket(fd_);
    }

    /// Send data
    [[nodiscard]] TransportResult<size_t> send(std::span<const char> data) noexcept {
        if (!is_connected()) {
            return std::unexpected{TransportError{TransportErrorCode::ConnectionClosed}};
        }

        int sent = ::send(fd_, data.data(), static_cast<int>(data.size()), 0);
        if (sent == SOCKET_ERROR) {
            int err = get_last_socket_error();
            if (is_would_block_error(err)) {
                return 0;
            }
            state_ = ConnectionState::Error;
            return std::unexpected{make_socket_error(err)};
        }

        return static_cast<size_t>(sent);
    }

    /// Receive data
    [[nodiscard]] TransportResult<size_t> receive(std::span<char> buffer) noexcept {
        if (!is_connected()) {
            return std::unexpected{TransportError{TransportErrorCode::ConnectionClosed}};
        }

        int received = ::recv(fd_, buffer.data(), static_cast<int>(buffer.size()), 0);
        if (received == SOCKET_ERROR) {
            int err = get_last_socket_error();
            if (is_would_block_error(err)) {
                return 0;
            }
            state_ = ConnectionState::Error;
            return std::unexpected{make_socket_error(err)};
        }

        if (received == 0) {
            state_ = ConnectionState::Disconnected;
            return std::unexpected{TransportError{TransportErrorCode::ConnectionClosed}};
        }

        return static_cast<size_t>(received);
    }

    /// Poll for read events using WSAPoll
    [[nodiscard]] bool poll_read(int timeout_ms) noexcept {
        if (!is_valid_socket(fd_)) return false;

        WSAPOLLFD pfd{};
        pfd.fd = fd_;
        pfd.events = POLLIN;

        int ret = WSAPoll(&pfd, 1, timeout_ms);
        return ret > 0 && (pfd.revents & POLLIN);
    }

    /// Poll for write events using WSAPoll
    [[nodiscard]] bool poll_write(int timeout_ms) noexcept {
        if (!is_valid_socket(fd_)) return false;

        WSAPOLLFD pfd{};
        pfd.fd = fd_;
        pfd.events = POLLOUT;

        int ret = WSAPoll(&pfd, 1, timeout_ms);
        return ret > 0 && (pfd.revents & POLLOUT);
    }

    /// Set TCP_NODELAY option
    [[nodiscard]] bool set_nodelay(bool enable) noexcept {
        options_.tcp_nodelay = enable;
        if (is_valid_socket(fd_)) {
            return set_tcp_nodelay(fd_, enable);
        }
        return true;
    }

    /// Set SO_KEEPALIVE option
    [[nodiscard]] bool set_keepalive(bool enable) noexcept {
        options_.keep_alive = enable;
        if (is_valid_socket(fd_)) {
            return set_socket_keepalive(fd_, enable);
        }
        return true;
    }

    /// Set non-blocking mode
    void set_nonblocking(bool enable) noexcept {
        if (is_valid_socket(fd_)) {
            (void)set_socket_nonblocking(fd_, enable);
        }
    }

    /// Set receive timeout
    [[nodiscard]] bool set_receive_timeout(int milliseconds) noexcept {
        options_.recv_timeout_ms = milliseconds;
        if (is_valid_socket(fd_)) {
            return set_socket_recv_timeout(fd_, milliseconds);
        }
        return true;
    }

    /// Set send timeout
    [[nodiscard]] bool set_send_timeout(int milliseconds) noexcept {
        options_.send_timeout_ms = milliseconds;
        if (is_valid_socket(fd_)) {
            return set_socket_send_timeout(fd_, milliseconds);
        }
        return true;
    }

    /// Set buffer sizes
    void set_buffer_sizes(int recv_size, int send_size) noexcept {
        options_.recv_buffer_size = recv_size;
        options_.send_buffer_size = send_size;
        if (is_valid_socket(fd_)) {
            (void)set_socket_recv_buffer(fd_, recv_size);
            (void)set_socket_send_buffer(fd_, send_size);
        }
    }

    /// Get socket state
    [[nodiscard]] ConnectionState state() const noexcept { return state_; }

    /// Get raw socket handle
    [[nodiscard]] SocketHandle fd() const noexcept { return fd_; }

private:
    void apply_options() noexcept {
        (void)set_nodelay(options_.tcp_nodelay);
        (void)set_keepalive(options_.keep_alive);
        (void)set_receive_timeout(options_.recv_timeout_ms);
        (void)set_send_timeout(options_.send_timeout_ms);
        set_buffer_sizes(options_.recv_buffer_size, options_.send_buffer_size);
    }

    SocketHandle fd_;
    ConnectionState state_;
    SocketOptions options_;
};

// ============================================================================
// Winsock Transport (implements ITransport)
// ============================================================================

/// Windows Winsock2 TCP transport implementation
class WinsockTransport : public ITransport {
public:
    WinsockTransport() noexcept = default;

    [[nodiscard]] TransportResult<void> connect(
        std::string_view host,
        uint16_t port) override
    {
        return socket_.connect(host, port);
    }

    void disconnect() noexcept override {
        socket_.close();
    }

    [[nodiscard]] bool is_connected() const noexcept override {
        return socket_.is_connected();
    }

    [[nodiscard]] TransportResult<size_t> send(std::span<const char> data) noexcept override {
        return socket_.send(data);
    }

    [[nodiscard]] TransportResult<size_t> receive(std::span<char> buffer) noexcept override {
        return socket_.receive(buffer);
    }

    [[nodiscard]] bool set_nodelay(bool enable) noexcept override {
        return socket_.set_nodelay(enable);
    }

    [[nodiscard]] bool set_keepalive(bool enable) noexcept override {
        return socket_.set_keepalive(enable);
    }

    [[nodiscard]] bool set_receive_timeout(int milliseconds) noexcept override {
        return socket_.set_receive_timeout(milliseconds);
    }

    [[nodiscard]] bool set_send_timeout(int milliseconds) noexcept override {
        return socket_.set_send_timeout(milliseconds);
    }

    /// Get underlying socket
    [[nodiscard]] WinsockSocket& socket() noexcept { return socket_; }
    [[nodiscard]] const WinsockSocket& socket() const noexcept { return socket_; }

private:
    WinsockSocket socket_;
};

// ============================================================================
// Winsock Acceptor (for FIX Acceptor)
// ============================================================================

/// Windows TCP server socket for accepting connections
class WinsockAcceptor {
public:
    WinsockAcceptor() noexcept : fd_{INVALID_SOCKET_HANDLE} {}

    ~WinsockAcceptor() {
        close();
    }

    // Non-copyable, non-movable
    WinsockAcceptor(const WinsockAcceptor&) = delete;
    WinsockAcceptor& operator=(const WinsockAcceptor&) = delete;

    /// Bind and listen on port
    [[nodiscard]] TransportResult<void> listen(
        uint16_t port,
        int backlog = 128) noexcept
    {
        // Ensure Winsock is initialized
        if (!WinsockInit::ensure()) {
            return std::unexpected{WinsockInit::make_init_error()};
        }

        fd_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (!is_valid_socket(fd_)) {
            return std::unexpected{make_socket_error()};
        }

        // Allow address reuse
        (void)set_socket_reuseaddr(fd_, true);

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (::bind(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
            auto err = make_socket_error();
            close();
            return std::unexpected{err};
        }

        if (::listen(fd_, backlog) == SOCKET_ERROR) {
            auto err = make_socket_error();
            close();
            return std::unexpected{err};
        }

        return {};
    }

    /// Accept a connection
    [[nodiscard]] TransportResult<SocketHandle> accept() noexcept {
        if (!is_valid_socket(fd_)) {
            return std::unexpected{TransportError{TransportErrorCode::SocketError}};
        }

        struct sockaddr_in client_addr{};
        int addr_len = sizeof(client_addr);

        SocketHandle client_fd = ::accept(fd_,
            reinterpret_cast<struct sockaddr*>(&client_addr),
            &addr_len);

        if (!is_valid_socket(client_fd)) {
            return std::unexpected{make_socket_error()};
        }

        return client_fd;
    }

    /// Close acceptor
    void close() noexcept {
        if (is_valid_socket(fd_)) {
            close_socket(fd_);
            fd_ = INVALID_SOCKET_HANDLE;
        }
    }

    /// Check if listening
    [[nodiscard]] bool is_listening() const noexcept {
        return is_valid_socket(fd_);
    }

    /// Get raw socket handle
    [[nodiscard]] SocketHandle fd() const noexcept { return fd_; }

private:
    SocketHandle fd_;
};

} // namespace nfx

#else  // !NFX_PLATFORM_WINDOWS

// ============================================================================
// Stub for non-Windows platforms
// ============================================================================

namespace nfx {

// On non-Windows, use TcpSocket/TcpTransport/TcpAcceptor from tcp_transport.hpp
// These type aliases provide a consistent API across platforms

} // namespace nfx

#endif  // NFX_PLATFORM_WINDOWS
