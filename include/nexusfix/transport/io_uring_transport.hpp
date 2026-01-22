#pragma once

#include "nexusfix/transport/socket.hpp"
#include "nexusfix/session/coroutine.hpp"

// Only include io_uring on Linux when available
#if defined(NFX_HAS_IO_URING) && NFX_HAS_IO_URING
    #include <liburing.h>
    #define NFX_IO_URING_AVAILABLE 1
#else
    #define NFX_IO_URING_AVAILABLE 0
#endif

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

namespace nfx {

#if NFX_IO_URING_AVAILABLE

// ============================================================================
// io_uring Event Types
// ============================================================================

/// Type of io_uring operation
enum class IoOperation : uint8_t {
    None,
    Accept,
    Connect,
    Read,
    Write,
    Close
};

/// Completion event from io_uring
struct IoEvent {
    IoOperation op;
    int result;       // Result code (bytes or error)
    void* user_data;  // User-provided context
};

// ============================================================================
// io_uring Context
// ============================================================================

/// Manages io_uring instance
class IoUringContext {
public:
    static constexpr unsigned QUEUE_DEPTH = 256;

    IoUringContext() noexcept : initialized_{false} {}

    ~IoUringContext() {
        if (initialized_) {
            io_uring_queue_exit(&ring_);
        }
    }

    // Non-copyable, non-movable
    IoUringContext(const IoUringContext&) = delete;
    IoUringContext& operator=(const IoUringContext&) = delete;

    /// Initialize io_uring
    [[nodiscard]] TransportResult<void> init(unsigned queue_depth = QUEUE_DEPTH) noexcept {
        int ret = io_uring_queue_init(queue_depth, &ring_, 0);
        if (ret < 0) {
            return std::unexpected{TransportError{TransportErrorCode::SocketError, -ret}};
        }
        initialized_ = true;
        return {};
    }

    /// Check if initialized
    [[nodiscard]] bool is_initialized() const noexcept {
        return initialized_;
    }

    /// Get a submission queue entry
    [[nodiscard]] struct io_uring_sqe* get_sqe() noexcept {
        return io_uring_get_sqe(&ring_);
    }

    /// Submit pending entries
    int submit() noexcept {
        return io_uring_submit(&ring_);
    }

    /// Wait for completion
    int wait(struct io_uring_cqe** cqe, int timeout_ms = -1) noexcept {
        if (timeout_ms < 0) {
            return io_uring_wait_cqe(&ring_, cqe);
        }

        struct __kernel_timespec ts;
        ts.tv_sec = timeout_ms / 1000;
        ts.tv_nsec = (timeout_ms % 1000) * 1000000;
        return io_uring_wait_cqe_timeout(&ring_, cqe, &ts);
    }

    /// Mark completion as seen
    void seen(struct io_uring_cqe* cqe) noexcept {
        io_uring_cqe_seen(&ring_, cqe);
    }

    /// Peek completions (non-blocking)
    int peek(struct io_uring_cqe** cqe) noexcept {
        return io_uring_peek_cqe(&ring_, cqe);
    }

    /// Get underlying ring
    [[nodiscard]] struct io_uring* ring() noexcept {
        return &ring_;
    }

private:
    struct io_uring ring_;
    bool initialized_;
};

// ============================================================================
// io_uring Socket
// ============================================================================

/// Socket using io_uring for async I/O
class IoUringSocket {
public:
    IoUringSocket(IoUringContext& ctx) noexcept
        : ctx_{ctx}
        , fd_{-1}
        , state_{ConnectionState::Disconnected} {}

    ~IoUringSocket() {
        close_sync();
    }

    // Non-copyable
    IoUringSocket(const IoUringSocket&) = delete;
    IoUringSocket& operator=(const IoUringSocket&) = delete;

    /// Create socket
    [[nodiscard]] TransportResult<void> create() noexcept {
        fd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (fd_ < 0) {
            return std::unexpected{TransportError{TransportErrorCode::SocketError, errno}};
        }
        return {};
    }

    /// Submit async connect
    [[nodiscard]] TransportResult<void> submit_connect(
        const struct sockaddr* addr,
        socklen_t addrlen,
        void* user_data = nullptr) noexcept
    {
        auto sqe = ctx_.get_sqe();
        if (!sqe) {
            return std::unexpected{TransportError{TransportErrorCode::SocketError}};
        }

        io_uring_prep_connect(sqe, fd_, addr, addrlen);
        io_uring_sqe_set_data(sqe, user_data);
        state_ = ConnectionState::Connecting;

        return {};
    }

    /// Submit async read
    [[nodiscard]] TransportResult<void> submit_read(
        std::span<char> buffer,
        void* user_data = nullptr) noexcept
    {
        auto sqe = ctx_.get_sqe();
        if (!sqe) {
            return std::unexpected{TransportError{TransportErrorCode::SocketError}};
        }

        io_uring_prep_recv(sqe, fd_, buffer.data(), buffer.size(), 0);
        io_uring_sqe_set_data(sqe, user_data);

        return {};
    }

    /// Submit async write
    [[nodiscard]] TransportResult<void> submit_write(
        std::span<const char> data,
        void* user_data = nullptr) noexcept
    {
        auto sqe = ctx_.get_sqe();
        if (!sqe) {
            return std::unexpected{TransportError{TransportErrorCode::SocketError}};
        }

        io_uring_prep_send(sqe, fd_, data.data(), data.size(), 0);
        io_uring_sqe_set_data(sqe, user_data);

        return {};
    }

    /// Submit async close
    [[nodiscard]] TransportResult<void> submit_close(void* user_data = nullptr) noexcept {
        auto sqe = ctx_.get_sqe();
        if (!sqe) {
            return std::unexpected{TransportError{TransportErrorCode::SocketError}};
        }

        io_uring_prep_close(sqe, fd_);
        io_uring_sqe_set_data(sqe, user_data);
        state_ = ConnectionState::Disconnecting;

        return {};
    }

    /// Synchronous close (for cleanup)
    void close_sync() noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
            state_ = ConnectionState::Disconnected;
        }
    }

    /// Handle connect completion
    void on_connect_complete(int result) noexcept {
        if (result == 0) {
            state_ = ConnectionState::Connected;
            apply_options();
        } else {
            state_ = ConnectionState::Error;
        }
    }

    /// Handle close completion
    void on_close_complete() noexcept {
        fd_ = -1;
        state_ = ConnectionState::Disconnected;
    }

    /// Socket options
    void set_nodelay(bool enable) noexcept {
        if (fd_ >= 0) {
            int flag = enable ? 1 : 0;
            ::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
        }
    }

    void set_keepalive(bool enable) noexcept {
        if (fd_ >= 0) {
            int flag = enable ? 1 : 0;
            ::setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag));
        }
    }

    [[nodiscard]] bool is_connected() const noexcept {
        return state_ == ConnectionState::Connected;
    }

    [[nodiscard]] ConnectionState state() const noexcept { return state_; }
    [[nodiscard]] int fd() const noexcept { return fd_; }

private:
    void apply_options() noexcept {
        set_nodelay(true);
        set_keepalive(true);
    }

    IoUringContext& ctx_;
    int fd_;
    ConnectionState state_;
};

// ============================================================================
// io_uring Transport
// ============================================================================

/// High-performance transport using io_uring
class IoUringTransport : public ITransport {
public:
    static constexpr size_t RECV_BUFFER_SIZE = 65536;

    explicit IoUringTransport(IoUringContext& ctx) noexcept
        : ctx_{ctx}
        , socket_{ctx}
        , recv_buffer_{}
        , recv_pending_{false} {}

    [[nodiscard]] TransportResult<void> connect(
        std::string_view host,
        uint16_t port) override
    {
        auto result = socket_.create();
        if (!result) return result;

        // Resolve address
        struct addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        char port_str[8];
        std::snprintf(port_str, sizeof(port_str), "%u", port);

        char host_buf[256];
        size_t host_len = std::min(host.size(), sizeof(host_buf) - 1);
        std::memcpy(host_buf, host.data(), host_len);
        host_buf[host_len] = '\0';

        struct addrinfo* addr_result = nullptr;
        int ret = ::getaddrinfo(host_buf, port_str, &hints, &addr_result);
        if (ret != 0) {
            return std::unexpected{TransportError{TransportErrorCode::AddressResolutionFailed}};
        }

        // Submit async connect
        result = socket_.submit_connect(addr_result->ai_addr, addr_result->ai_addrlen);
        ::freeaddrinfo(addr_result);

        if (!result) return result;

        // Submit and wait for connect
        ctx_.submit();

        struct io_uring_cqe* cqe;
        ctx_.wait(&cqe);
        int connect_result = cqe->res;
        ctx_.seen(cqe);

        socket_.on_connect_complete(connect_result);

        if (connect_result < 0) {
            return std::unexpected{TransportError{TransportErrorCode::ConnectionFailed, -connect_result}};
        }

        // Start async receive
        submit_recv();

        return {};
    }

    void disconnect() override {
        socket_.close_sync();
    }

    [[nodiscard]] bool is_connected() const override {
        return socket_.is_connected();
    }

    [[nodiscard]] TransportResult<size_t> send(std::span<const char> data) override {
        if (!is_connected()) {
            return std::unexpected{TransportError{TransportErrorCode::ConnectionClosed}};
        }

        auto result = socket_.submit_write(data);
        if (!result) return std::unexpected{result.error()};

        ctx_.submit();

        struct io_uring_cqe* cqe;
        ctx_.wait(&cqe);
        int send_result = cqe->res;
        ctx_.seen(cqe);

        if (send_result < 0) {
            return std::unexpected{TransportError{TransportErrorCode::WriteError, -send_result}};
        }

        return static_cast<size_t>(send_result);
    }

    [[nodiscard]] TransportResult<size_t> receive(std::span<char> buffer) override {
        if (!is_connected()) {
            return std::unexpected{TransportError{TransportErrorCode::ConnectionClosed}};
        }

        // Check if data available in buffer
        if (!recv_buffer_.empty()) {
            return recv_buffer_.read(buffer);
        }

        // Submit receive and wait
        auto result = socket_.submit_read(buffer);
        if (!result) return std::unexpected{result.error()};

        ctx_.submit();

        struct io_uring_cqe* cqe;
        ctx_.wait(&cqe);
        int recv_result = cqe->res;
        ctx_.seen(cqe);

        if (recv_result <= 0) {
            if (recv_result == 0) {
                return std::unexpected{TransportError{TransportErrorCode::ConnectionClosed}};
            }
            return std::unexpected{TransportError{TransportErrorCode::ReadError, -recv_result}};
        }

        return static_cast<size_t>(recv_result);
    }

    void set_nodelay(bool enable) override {
        socket_.set_nodelay(enable);
    }

    void set_keepalive(bool enable) override {
        socket_.set_keepalive(enable);
    }

    void set_receive_timeout(int /*milliseconds*/) override {
        // io_uring uses async operations, timeout handled differently
    }

    void set_send_timeout(int /*milliseconds*/) override {
        // io_uring uses async operations, timeout handled differently
    }

    /// Process pending completions (non-blocking)
    int poll() noexcept {
        struct io_uring_cqe* cqe;
        int processed = 0;

        while (ctx_.peek(&cqe) == 0) {
            // Process completion
            int result = cqe->res;
            ctx_.seen(cqe);
            ++processed;

            if (recv_pending_ && result > 0) {
                // Receive completed, submit next
                recv_pending_ = false;
                submit_recv();
            }
        }

        return processed;
    }

private:
    void submit_recv() noexcept {
        if (recv_pending_) return;

        auto span = recv_buffer_.write_span();
        if (span.empty()) return;

        socket_.submit_read(span);
        ctx_.submit();
        recv_pending_ = true;
    }

    IoUringContext& ctx_;
    IoUringSocket socket_;
    RingBuffer<RECV_BUFFER_SIZE> recv_buffer_;
    bool recv_pending_;
};

#else  // !NFX_IO_URING_AVAILABLE

// Stub implementation when io_uring is not available
class IoUringContext {
public:
    [[nodiscard]] TransportResult<void> init(unsigned = 256) noexcept {
        return std::unexpected{TransportError{TransportErrorCode::SocketError}};
    }
    [[nodiscard]] bool is_initialized() const noexcept { return false; }
};

class IoUringTransport : public ITransport {
public:
    explicit IoUringTransport(IoUringContext&) noexcept {}

    [[nodiscard]] TransportResult<void> connect(std::string_view, uint16_t) override {
        return std::unexpected{TransportError{TransportErrorCode::SocketError}};
    }
    void disconnect() override {}
    [[nodiscard]] bool is_connected() const override { return false; }
    [[nodiscard]] TransportResult<size_t> send(std::span<const char>) override {
        return std::unexpected{TransportError{TransportErrorCode::SocketError}};
    }
    [[nodiscard]] TransportResult<size_t> receive(std::span<char>) override {
        return std::unexpected{TransportError{TransportErrorCode::SocketError}};
    }
    void set_nodelay(bool) override {}
    void set_keepalive(bool) override {}
    void set_receive_timeout(int) override {}
    void set_send_timeout(int) override {}
};

#endif  // NFX_IO_URING_AVAILABLE

} // namespace nfx
