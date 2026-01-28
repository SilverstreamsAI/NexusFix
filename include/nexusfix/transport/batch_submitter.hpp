/*
    NexusFIX Batched io_uring Submissions

    Reduces syscall overhead by batching multiple I/O operations
    into a single io_uring_submit() call.

    Performance:
    - Single submit: ~200-300ns per syscall
    - Batched submit: Amortized ~20-30ns per operation

    Typical usage patterns:
    - Batch sends for multiple sessions
    - Batch heartbeats
    - Batch acknowledgments
*/

#pragma once

#include "nexusfix/platform/platform.hpp"
#include "nexusfix/transport/io_uring_transport.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <functional>

namespace nfx {

#if NFX_IO_URING_AVAILABLE

// ============================================================================
// Batch Operation Types
// ============================================================================

/// Operation to be batched
struct BatchOp {
    enum class Type : uint8_t {
        Send,
        Recv,
        SendFixed,   // Using registered buffer
        RecvFixed,   // Using registered buffer
        Nop          // No-op for testing
    };

    Type type;
    int fd;
    union {
        struct {
            const char* data;
            size_t len;
        } send;
        struct {
            char* data;
            size_t len;
        } recv;
        struct {
            uint16_t buf_index;
            size_t len;
            size_t offset;
        } fixed;
    };
    void* user_data;
};

// ============================================================================
// Batch Submitter
// ============================================================================

/// Batches multiple io_uring operations for single-syscall submission
/// @tparam MaxBatchSize Maximum operations per batch
template<size_t MaxBatchSize = 32>
class BatchSubmitter {
public:
    explicit BatchSubmitter(IoUringContext& ctx) noexcept
        : ctx_{ctx}, count_{0} {}

    /// Queue a send operation
    [[nodiscard]] NFX_HOT
    bool queue_send(int fd, std::span<const char> data, void* user_data = nullptr) noexcept {
        if (count_ >= MaxBatchSize) return false;

        auto* sqe = ctx_.get_sqe();
        if (!sqe) return false;

        io_uring_prep_send(sqe, fd, data.data(), data.size(), 0);
        io_uring_sqe_set_data(sqe, user_data);

        ++count_;
        return true;
    }

    /// Queue a recv operation
    [[nodiscard]] NFX_HOT
    bool queue_recv(int fd, std::span<char> buffer, void* user_data = nullptr) noexcept {
        if (count_ >= MaxBatchSize) return false;

        auto* sqe = ctx_.get_sqe();
        if (!sqe) return false;

        io_uring_prep_recv(sqe, fd, buffer.data(), buffer.size(), 0);
        io_uring_sqe_set_data(sqe, user_data);

        ++count_;
        return true;
    }

    /// Queue a send using registered buffer
    [[nodiscard]] NFX_HOT
    bool queue_send_fixed(int fd, uint16_t buf_index, size_t len,
                          size_t offset = 0, void* user_data = nullptr) noexcept {
        if (count_ >= MaxBatchSize) return false;

        auto* sqe = ctx_.get_sqe();
        if (!sqe) return false;

        io_uring_prep_write_fixed(sqe, fd, nullptr, len, offset, buf_index);
        io_uring_sqe_set_data(sqe, user_data);

        ++count_;
        return true;
    }

    /// Queue a recv using registered buffer
    [[nodiscard]] NFX_HOT
    bool queue_recv_fixed(int fd, uint16_t buf_index, size_t len,
                          size_t offset = 0, void* user_data = nullptr) noexcept {
        if (count_ >= MaxBatchSize) return false;

        auto* sqe = ctx_.get_sqe();
        if (!sqe) return false;

        io_uring_prep_read_fixed(sqe, fd, nullptr, len, offset, buf_index);
        io_uring_sqe_set_data(sqe, user_data);

        ++count_;
        return true;
    }

    /// Queue a no-op (for testing/benchmarking)
    [[nodiscard]] NFX_HOT
    bool queue_nop(void* user_data = nullptr) noexcept {
        if (count_ >= MaxBatchSize) return false;

        auto* sqe = ctx_.get_sqe();
        if (!sqe) return false;

        io_uring_prep_nop(sqe);
        io_uring_sqe_set_data(sqe, user_data);

        ++count_;
        return true;
    }

    /// Submit all queued operations in a single syscall
    /// Returns number of operations submitted, or negative error
    [[nodiscard]] NFX_HOT
    int submit() noexcept {
        if (count_ == 0) return 0;

        int ret = ctx_.submit();
        size_t submitted = count_;
        count_ = 0;

        if (ret < 0) return ret;
        return static_cast<int>(submitted);
    }

    /// Submit and wait for all completions
    /// @param completions Output buffer for completion results
    /// @return Number of completions, or negative error
    [[nodiscard]]
    int submit_and_wait(std::span<IoEvent> completions) noexcept {
        int submitted = submit();
        if (submitted <= 0) return submitted;

        int completed = 0;
        while (completed < submitted && static_cast<size_t>(completed) < completions.size()) {
            struct io_uring_cqe* cqe;
            int ret = ctx_.wait(&cqe);
            if (ret < 0) break;

            completions[completed].result = cqe->res;
            completions[completed].user_data = io_uring_cqe_get_data(cqe);
            ctx_.seen(cqe);
            ++completed;
        }

        return completed;
    }

    /// Get number of queued operations
    [[nodiscard]] size_t queued() const noexcept { return count_; }

    /// Check if batch is full
    [[nodiscard]] bool is_full() const noexcept { return count_ >= MaxBatchSize; }

    /// Check if batch is empty
    [[nodiscard]] bool is_empty() const noexcept { return count_ == 0; }

    /// Get maximum batch size
    [[nodiscard]] static constexpr size_t max_size() noexcept { return MaxBatchSize; }

    /// Clear queued operations without submitting
    void clear() noexcept { count_ = 0; }

private:
    IoUringContext& ctx_;
    size_t count_;
};

// ============================================================================
// Auto-Flush Batch Submitter
// ============================================================================

/// Batch submitter that auto-flushes when full or on timeout
/// @tparam MaxBatchSize Maximum operations per batch
template<size_t MaxBatchSize = 32>
class AutoFlushBatchSubmitter {
public:
    explicit AutoFlushBatchSubmitter(IoUringContext& ctx) noexcept
        : batch_{ctx}, total_submitted_{0}, total_flushes_{0} {}

    /// Queue send, auto-flush if batch is full
    [[nodiscard]] NFX_HOT
    bool queue_send(int fd, std::span<const char> data, void* user_data = nullptr) noexcept {
        if (batch_.is_full()) {
            flush();
        }
        return batch_.queue_send(fd, data, user_data);
    }

    /// Queue recv, auto-flush if batch is full
    [[nodiscard]] NFX_HOT
    bool queue_recv(int fd, std::span<char> buffer, void* user_data = nullptr) noexcept {
        if (batch_.is_full()) {
            flush();
        }
        return batch_.queue_recv(fd, buffer, user_data);
    }

    /// Queue no-op, auto-flush if batch is full
    [[nodiscard]] NFX_HOT
    bool queue_nop(void* user_data = nullptr) noexcept {
        if (batch_.is_full()) {
            flush();
        }
        return batch_.queue_nop(user_data);
    }

    /// Flush pending operations
    int flush() noexcept {
        if (batch_.is_empty()) return 0;

        int ret = batch_.submit();
        if (ret > 0) {
            total_submitted_ += static_cast<size_t>(ret);
            ++total_flushes_;
        }
        return ret;
    }

    /// Get statistics
    [[nodiscard]] size_t total_submitted() const noexcept { return total_submitted_; }
    [[nodiscard]] size_t total_flushes() const noexcept { return total_flushes_; }
    [[nodiscard]] double avg_batch_size() const noexcept {
        return total_flushes_ > 0
            ? static_cast<double>(total_submitted_) / static_cast<double>(total_flushes_)
            : 0.0;
    }

    /// Get number of queued operations
    [[nodiscard]] size_t queued() const noexcept { return batch_.queued(); }

private:
    BatchSubmitter<MaxBatchSize> batch_;
    size_t total_submitted_;
    size_t total_flushes_;
};

// ============================================================================
// Scatter-Gather Batch Send
// ============================================================================

/// Batch multiple buffers into a single writev operation
class ScatterGatherSend {
public:
    static constexpr size_t MAX_IOVECS = 16;

    ScatterGatherSend() noexcept : count_{0} {}

    /// Add a buffer to the scatter-gather list
    [[nodiscard]] bool add(std::span<const char> data) noexcept {
        if (count_ >= MAX_IOVECS) return false;

        iovecs_[count_].iov_base = const_cast<char*>(data.data());
        iovecs_[count_].iov_len = data.size();
        ++count_;
        return true;
    }

    /// Submit as a single writev operation
    [[nodiscard]] bool submit(IoUringContext& ctx, int fd, void* user_data = nullptr) noexcept {
        if (count_ == 0) return false;

        auto* sqe = ctx.get_sqe();
        if (!sqe) return false;

        io_uring_prep_writev(sqe, fd, iovecs_.data(), static_cast<unsigned>(count_), 0);
        io_uring_sqe_set_data(sqe, user_data);

        count_ = 0;
        return true;
    }

    /// Get total bytes to send
    [[nodiscard]] size_t total_bytes() const noexcept {
        size_t total = 0;
        for (size_t i = 0; i < count_; ++i) {
            total += iovecs_[i].iov_len;
        }
        return total;
    }

    /// Get number of buffers
    [[nodiscard]] size_t buffer_count() const noexcept { return count_; }

    /// Reset for reuse
    void reset() noexcept { count_ = 0; }

private:
    std::array<struct iovec, MAX_IOVECS> iovecs_;
    size_t count_;
};

// ============================================================================
// Linked Operations (SQE Linking)
// ============================================================================

/// Chain operations so they execute in order
/// If one fails, subsequent linked operations are cancelled
class LinkedOperations {
public:
    explicit LinkedOperations(IoUringContext& ctx) noexcept
        : ctx_{ctx}, count_{0}, last_sqe_{nullptr} {}

    /// Add a send operation to the chain
    [[nodiscard]] bool chain_send(int fd, std::span<const char> data, void* user_data = nullptr) noexcept {
        auto* sqe = ctx_.get_sqe();
        if (!sqe) return false;

        io_uring_prep_send(sqe, fd, data.data(), data.size(), 0);
        io_uring_sqe_set_data(sqe, user_data);

        // Link to previous operation (if any)
        if (last_sqe_) {
            last_sqe_->flags |= IOSQE_IO_LINK;
        }

        last_sqe_ = sqe;
        ++count_;
        return true;
    }

    /// Add a recv operation to the chain
    [[nodiscard]] bool chain_recv(int fd, std::span<char> buffer, void* user_data = nullptr) noexcept {
        auto* sqe = ctx_.get_sqe();
        if (!sqe) return false;

        io_uring_prep_recv(sqe, fd, buffer.data(), buffer.size(), 0);
        io_uring_sqe_set_data(sqe, user_data);

        if (last_sqe_) {
            last_sqe_->flags |= IOSQE_IO_LINK;
        }

        last_sqe_ = sqe;
        ++count_;
        return true;
    }

    /// Submit the linked chain
    [[nodiscard]] int submit() noexcept {
        if (count_ == 0) return 0;

        int ret = ctx_.submit();
        count_ = 0;
        last_sqe_ = nullptr;
        return ret;
    }

    /// Get number of operations in chain
    [[nodiscard]] size_t count() const noexcept { return count_; }

private:
    IoUringContext& ctx_;
    size_t count_;
    struct io_uring_sqe* last_sqe_;
};

#endif // NFX_IO_URING_AVAILABLE

} // namespace nfx
