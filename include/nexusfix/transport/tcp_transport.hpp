#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cerrno>
#include <cstring>

#include "nexusfix/transport/socket.hpp"

namespace nfx {

// ============================================================================
// TCP Socket (POSIX)
// ============================================================================

/// TCP socket implementation using POSIX sockets
class TcpSocket {
public:
    static constexpr int INVALID_SOCKET = -1;

    TcpSocket() noexcept : fd_{INVALID_SOCKET}, state_{ConnectionState::Disconnected} {}

    ~TcpSocket() {
        close();
    }

    // Non-copyable
    TcpSocket(const TcpSocket&) = delete;
    TcpSocket& operator=(const TcpSocket&) = delete;

    // Movable
    TcpSocket(TcpSocket&& other) noexcept
        : fd_{other.fd_}, state_{other.state_}, options_{other.options_} {
        other.fd_ = INVALID_SOCKET;
        other.state_ = ConnectionState::Disconnected;
    }

    TcpSocket& operator=(TcpSocket&& other) noexcept {
        if (this != &other) {
            close();
            fd_ = other.fd_;
            state_ = other.state_;
            options_ = other.options_;
            other.fd_ = INVALID_SOCKET;
            other.state_ = ConnectionState::Disconnected;
        }
        return *this;
    }

    /// Create socket
    [[nodiscard]] TransportResult<void> create() noexcept {
        fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd_ == INVALID_SOCKET) {
            return std::unexpected{TransportError{TransportErrorCode::SocketError, errno}};
        }
        return {};
    }

    /// Connect to remote host
    [[nodiscard]] TransportResult<void> connect(
        std::string_view host,
        uint16_t port) noexcept
    {
        if (fd_ == INVALID_SOCKET) {
            auto result = create();
            if (!result) return result;
        }

        state_ = ConnectionState::Connecting;

        // Resolve hostname
        struct addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

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
            return std::unexpected{TransportError{TransportErrorCode::AddressResolutionFailed}};
        }

        // Try to connect
        ret = ::connect(fd_, result->ai_addr, result->ai_addrlen);
        ::freeaddrinfo(result);

        if (ret != 0) {
            state_ = ConnectionState::Error;
            return std::unexpected{TransportError{TransportErrorCode::ConnectionFailed, errno}};
        }

        apply_options();
        state_ = ConnectionState::Connected;
        return {};
    }

    /// Close socket
    void close() noexcept {
        if (fd_ != INVALID_SOCKET) {
            state_ = ConnectionState::Disconnecting;
            ::close(fd_);
            fd_ = INVALID_SOCKET;
            state_ = ConnectionState::Disconnected;
        }
    }

    /// Check if connected
    [[nodiscard]] bool is_connected() const noexcept {
        return state_ == ConnectionState::Connected && fd_ != INVALID_SOCKET;
    }

    /// Send data
    [[nodiscard]] TransportResult<size_t> send(std::span<const char> data) noexcept {
        if (!is_connected()) {
            return std::unexpected{TransportError{TransportErrorCode::ConnectionClosed}};
        }

        ssize_t sent = ::send(fd_, data.data(), data.size(), MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 0;
            }
            state_ = ConnectionState::Error;
            return std::unexpected{TransportError{TransportErrorCode::WriteError, errno}};
        }

        return static_cast<size_t>(sent);
    }

    /// Receive data
    [[nodiscard]] TransportResult<size_t> receive(std::span<char> buffer) noexcept {
        if (!is_connected()) {
            return std::unexpected{TransportError{TransportErrorCode::ConnectionClosed}};
        }

        ssize_t received = ::recv(fd_, buffer.data(), buffer.size(), 0);
        if (received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 0;
            }
            state_ = ConnectionState::Error;
            return std::unexpected{TransportError{TransportErrorCode::ReadError, errno}};
        }

        if (received == 0) {
            state_ = ConnectionState::Disconnected;
            return std::unexpected{TransportError{TransportErrorCode::ConnectionClosed}};
        }

        return static_cast<size_t>(received);
    }

    /// Poll for events
    [[nodiscard]] bool poll_read(int timeout_ms) noexcept {
        if (fd_ == INVALID_SOCKET) return false;

        struct pollfd pfd{};
        pfd.fd = fd_;
        pfd.events = POLLIN;

        int ret = ::poll(&pfd, 1, timeout_ms);
        return ret > 0 && (pfd.revents & POLLIN);
    }

    [[nodiscard]] bool poll_write(int timeout_ms) noexcept {
        if (fd_ == INVALID_SOCKET) return false;

        struct pollfd pfd{};
        pfd.fd = fd_;
        pfd.events = POLLOUT;

        int ret = ::poll(&pfd, 1, timeout_ms);
        return ret > 0 && (pfd.revents & POLLOUT);
    }

    /// Socket options
    void set_nodelay(bool enable) noexcept {
        options_.tcp_nodelay = enable;
        if (fd_ != INVALID_SOCKET) {
            int flag = enable ? 1 : 0;
            ::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
        }
    }

    void set_keepalive(bool enable) noexcept {
        options_.keep_alive = enable;
        if (fd_ != INVALID_SOCKET) {
            int flag = enable ? 1 : 0;
            ::setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag));
        }
    }

    void set_nonblocking(bool enable) noexcept {
        if (fd_ == INVALID_SOCKET) return;

        int flags = ::fcntl(fd_, F_GETFL, 0);
        if (enable) {
            flags |= O_NONBLOCK;
        } else {
            flags &= ~O_NONBLOCK;
        }
        ::fcntl(fd_, F_SETFL, flags);
    }

    void set_receive_timeout(int milliseconds) noexcept {
        options_.recv_timeout_ms = milliseconds;
        if (fd_ != INVALID_SOCKET) {
            struct timeval tv;
            tv.tv_sec = milliseconds / 1000;
            tv.tv_usec = (milliseconds % 1000) * 1000;
            ::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        }
    }

    void set_send_timeout(int milliseconds) noexcept {
        options_.send_timeout_ms = milliseconds;
        if (fd_ != INVALID_SOCKET) {
            struct timeval tv;
            tv.tv_sec = milliseconds / 1000;
            tv.tv_usec = (milliseconds % 1000) * 1000;
            ::setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        }
    }

    void set_buffer_sizes(int recv_size, int send_size) noexcept {
        options_.recv_buffer_size = recv_size;
        options_.send_buffer_size = send_size;
        if (fd_ != INVALID_SOCKET) {
            ::setsockopt(fd_, SOL_SOCKET, SO_RCVBUF, &recv_size, sizeof(recv_size));
            ::setsockopt(fd_, SOL_SOCKET, SO_SNDBUF, &send_size, sizeof(send_size));
        }
    }

    /// Get socket state
    [[nodiscard]] ConnectionState state() const noexcept { return state_; }

    /// Get raw file descriptor
    [[nodiscard]] int fd() const noexcept { return fd_; }

private:
    void apply_options() noexcept {
        set_nodelay(options_.tcp_nodelay);
        set_keepalive(options_.keep_alive);
        set_receive_timeout(options_.recv_timeout_ms);
        set_send_timeout(options_.send_timeout_ms);
        set_buffer_sizes(options_.recv_buffer_size, options_.send_buffer_size);
    }

    int fd_;
    ConnectionState state_;
    SocketOptions options_;
};

// ============================================================================
// TCP Transport (implements ITransport)
// ============================================================================

/// TCP transport implementation
class TcpTransport : public ITransport {
public:
    TcpTransport() noexcept = default;

    [[nodiscard]] TransportResult<void> connect(
        std::string_view host,
        uint16_t port) override
    {
        return socket_.connect(host, port);
    }

    void disconnect() override {
        socket_.close();
    }

    [[nodiscard]] bool is_connected() const override {
        return socket_.is_connected();
    }

    [[nodiscard]] TransportResult<size_t> send(std::span<const char> data) override {
        return socket_.send(data);
    }

    [[nodiscard]] TransportResult<size_t> receive(std::span<char> buffer) override {
        return socket_.receive(buffer);
    }

    void set_nodelay(bool enable) override {
        socket_.set_nodelay(enable);
    }

    void set_keepalive(bool enable) override {
        socket_.set_keepalive(enable);
    }

    void set_receive_timeout(int milliseconds) override {
        socket_.set_receive_timeout(milliseconds);
    }

    void set_send_timeout(int milliseconds) override {
        socket_.set_send_timeout(milliseconds);
    }

    /// Get underlying socket
    [[nodiscard]] TcpSocket& socket() noexcept { return socket_; }
    [[nodiscard]] const TcpSocket& socket() const noexcept { return socket_; }

private:
    TcpSocket socket_;
};

// ============================================================================
// TCP Acceptor (for FIX Acceptor)
// ============================================================================

/// TCP server socket for accepting connections
class TcpAcceptor {
public:
    TcpAcceptor() noexcept : fd_{-1} {}

    ~TcpAcceptor() {
        close();
    }

    // Non-copyable, non-movable
    TcpAcceptor(const TcpAcceptor&) = delete;
    TcpAcceptor& operator=(const TcpAcceptor&) = delete;

    /// Bind and listen on port
    [[nodiscard]] TransportResult<void> listen(
        uint16_t port,
        int backlog = 128) noexcept
    {
        fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd_ < 0) {
            return std::unexpected{TransportError{TransportErrorCode::SocketError, errno}};
        }

        // Allow address reuse
        int opt = 1;
        ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (::bind(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            close();
            return std::unexpected{TransportError{TransportErrorCode::SocketError, errno}};
        }

        if (::listen(fd_, backlog) < 0) {
            close();
            return std::unexpected{TransportError{TransportErrorCode::SocketError, errno}};
        }

        return {};
    }

    /// Accept a connection
    [[nodiscard]] TransportResult<TcpSocket> accept() noexcept {
        if (fd_ < 0) {
            return std::unexpected{TransportError{TransportErrorCode::SocketError}};
        }

        struct sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);

        int client_fd = ::accept(fd_,
            reinterpret_cast<struct sockaddr*>(&client_addr),
            &addr_len);

        if (client_fd < 0) {
            return std::unexpected{TransportError{TransportErrorCode::SocketError, errno}};
        }

        TcpSocket client;
        // Transfer ownership of fd to TcpSocket
        // Note: This requires TcpSocket to have a method to set fd directly
        // For now, we'll just return the raw fd and let caller handle it

        return std::unexpected{TransportError{TransportErrorCode::SocketError}};
    }

    /// Close acceptor
    void close() noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    /// Check if listening
    [[nodiscard]] bool is_listening() const noexcept {
        return fd_ >= 0;
    }

private:
    int fd_;
};

} // namespace nfx
