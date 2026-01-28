#pragma once

#include <array>
#include <span>
#include <string_view>
#include <cstdint>
#include <optional>

#include "nexusfix/types/error.hpp"

namespace nfx {

// ============================================================================
// Transport Interface
// ============================================================================

/// Abstract interface for network transport
class ITransport {
public:
    virtual ~ITransport() = default;

    /// Connect to remote endpoint
    [[nodiscard]] virtual TransportResult<void> connect(
        std::string_view host,
        uint16_t port) = 0;

    /// Disconnect from remote endpoint
    virtual void disconnect() = 0;

    /// Check if connected
    [[nodiscard]] virtual bool is_connected() const = 0;

    /// Send data (blocking)
    [[nodiscard]] virtual TransportResult<size_t> send(
        std::span<const char> data) = 0;

    /// Receive data (blocking)
    [[nodiscard]] virtual TransportResult<size_t> receive(
        std::span<char> buffer) = 0;

    /// Set socket options
    [[nodiscard]] virtual bool set_nodelay(bool enable) = 0;
    [[nodiscard]] virtual bool set_keepalive(bool enable) = 0;
    [[nodiscard]] virtual bool set_receive_timeout(int milliseconds) = 0;
    [[nodiscard]] virtual bool set_send_timeout(int milliseconds) = 0;
};

// ============================================================================
// Socket Address
// ============================================================================

/// Network endpoint address
struct SocketAddress {
    std::string_view host;
    uint16_t port;

    constexpr SocketAddress() noexcept : host{}, port{0} {}

    constexpr SocketAddress(std::string_view h, uint16_t p) noexcept
        : host{h}, port{p} {}
};

// ============================================================================
// Socket Options
// ============================================================================

/// Common socket configuration options
struct SocketOptions {
    bool tcp_nodelay{true};           // Disable Nagle's algorithm
    bool keep_alive{true};            // Enable TCP keepalive
    int recv_timeout_ms{30000};       // Receive timeout in ms
    int send_timeout_ms{30000};       // Send timeout in ms
    int recv_buffer_size{65536};      // SO_RCVBUF
    int send_buffer_size{65536};      // SO_SNDBUF

    constexpr SocketOptions() noexcept = default;
};

// ============================================================================
// Connection State
// ============================================================================

/// Transport connection state
enum class ConnectionState : uint8_t {
    Disconnected,
    Connecting,
    Connected,
    Disconnecting,
    Error
};

[[nodiscard]] constexpr std::string_view connection_state_name(ConnectionState state) noexcept {
    switch (state) {
        case ConnectionState::Disconnected:   return "Disconnected";
        case ConnectionState::Connecting:     return "Connecting";
        case ConnectionState::Connected:      return "Connected";
        case ConnectionState::Disconnecting:  return "Disconnecting";
        case ConnectionState::Error:          return "Error";
    }
    return "Unknown";
}

// ============================================================================
// Ring Buffer for Network I/O
// ============================================================================

/// Lock-free ring buffer for efficient I/O
template <size_t Size>
class RingBuffer {
public:
    static_assert((Size & (Size - 1)) == 0, "Size must be power of 2");

    constexpr RingBuffer() noexcept : head_{0}, tail_{0} {}

    /// Write data to buffer
    [[nodiscard]] size_t write(std::span<const char> data) noexcept {
        size_t available = Size - size();
        size_t to_write = std::min(data.size(), available);

        for (size_t i = 0; i < to_write; ++i) {
            buffer_[(tail_ + i) & (Size - 1)] = data[i];
        }
        tail_ += to_write;

        return to_write;
    }

    /// Read data from buffer
    [[nodiscard]] size_t read(std::span<char> dest) noexcept {
        size_t available = size();
        size_t to_read = std::min(dest.size(), available);

        for (size_t i = 0; i < to_read; ++i) {
            dest[i] = buffer_[(head_ + i) & (Size - 1)];
        }
        head_ += to_read;

        return to_read;
    }

    /// Peek at data without consuming
    [[nodiscard]] size_t peek(std::span<char> dest) const noexcept {
        size_t available = size();
        size_t to_peek = std::min(dest.size(), available);

        for (size_t i = 0; i < to_peek; ++i) {
            dest[i] = buffer_[(head_ + i) & (Size - 1)];
        }

        return to_peek;
    }

    /// Skip bytes (consume without copying)
    void skip(size_t count) noexcept {
        size_t available = size();
        head_ += std::min(count, available);
    }

    /// Get number of bytes in buffer
    [[nodiscard]] size_t size() const noexcept {
        return tail_ - head_;
    }

    /// Get available space
    [[nodiscard]] size_t available() const noexcept {
        return Size - size();
    }

    /// Check if buffer is empty
    [[nodiscard]] bool empty() const noexcept {
        return head_ == tail_;
    }

    /// Check if buffer is full
    [[nodiscard]] bool full() const noexcept {
        return size() == Size;
    }

    /// Clear buffer
    void clear() noexcept {
        head_ = 0;
        tail_ = 0;
    }

    /// Get contiguous read span (for zero-copy operations)
    [[nodiscard]] std::span<const char> read_span() const noexcept {
        size_t start = head_ & (Size - 1);
        size_t end = tail_ & (Size - 1);

        if (start < end) {
            return std::span<const char>{buffer_.data() + start, end - start};
        } else if (size() > 0) {
            // Wraps around - return first segment
            return std::span<const char>{buffer_.data() + start, Size - start};
        }
        return {};
    }

    /// Get contiguous write span (for zero-copy operations)
    [[nodiscard]] std::span<char> write_span() noexcept {
        size_t end = tail_ & (Size - 1);
        size_t avail = available();

        if (end + avail <= Size) {
            return std::span<char>{buffer_.data() + end, avail};
        } else {
            // Wraps around - return first segment
            return std::span<char>{buffer_.data() + end, Size - end};
        }
    }

    /// Commit bytes written to write_span
    void commit_write(size_t count) noexcept {
        tail_ += std::min(count, available());
    }

private:
    std::array<char, Size> buffer_;
    size_t head_;
    size_t tail_;
};

// ============================================================================
// Buffer Manager
// ============================================================================

/// Manages receive and send buffers for a connection
class BufferManager {
public:
    static constexpr size_t BUFFER_SIZE = 65536;

    BufferManager() noexcept = default;

    /// Get receive buffer
    [[nodiscard]] RingBuffer<BUFFER_SIZE>& recv_buffer() noexcept {
        return recv_buffer_;
    }

    /// Get send buffer
    [[nodiscard]] RingBuffer<BUFFER_SIZE>& send_buffer() noexcept {
        return send_buffer_;
    }

    /// Reset both buffers
    void reset() noexcept {
        recv_buffer_.clear();
        send_buffer_.clear();
    }

private:
    RingBuffer<BUFFER_SIZE> recv_buffer_;
    RingBuffer<BUFFER_SIZE> send_buffer_;
};

} // namespace nfx
