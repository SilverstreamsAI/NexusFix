/*
    NexusFIX Lock-Free SPSC Queue

    Inspired by Quill logging library's approach:
    - Single-Producer Single-Consumer (no lock contention)
    - Cache-line aligned to avoid false sharing
    - Power-of-2 size for fast modulo (bitwise AND)
    - Memory order optimized for minimal synchronization

    Use case: Hot path message passing between threads
    - Application thread (producer) pushes messages
    - Background thread (consumer) processes them
*/

#pragma once

#include <atomic>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>
#include <new>

namespace nfx::memory {

// ============================================================================
// Cache Line Constants
// ============================================================================

#ifdef __cpp_lib_hardware_interference_size
    inline constexpr size_t CACHE_LINE_SIZE = std::hardware_destructive_interference_size;
#else
    inline constexpr size_t CACHE_LINE_SIZE = 64;
#endif

// ============================================================================
// SPSC Queue
// ============================================================================

/// Lock-free Single-Producer Single-Consumer queue
/// @tparam T Element type (must be trivially copyable for best performance)
/// @tparam Capacity Must be power of 2
template<typename T, size_t Capacity>
class SPSCQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    static_assert(Capacity >= 2, "Capacity must be at least 2");

public:
    SPSCQueue() noexcept = default;

    // Non-copyable, non-movable
    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;
    SPSCQueue(SPSCQueue&&) = delete;
    SPSCQueue& operator=(SPSCQueue&&) = delete;

    // ========================================================================
    // Producer Interface (single thread only)
    // ========================================================================

    /// Try to push an element (producer only)
    /// @return true if successful, false if queue is full
    [[nodiscard]] bool try_push(const T& item) noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t next_head = (head + 1) & mask_;

        // Check if queue is full
        if (next_head == cached_tail_) {
            // Refresh cached tail
            cached_tail_ = tail_.load(std::memory_order_acquire);
            if (next_head == cached_tail_) {
                return false;  // Queue is full
            }
        }

        // Store the item
        buffer_[head] = item;

        // Publish the item
        head_.store(next_head, std::memory_order_release);
        return true;
    }

    /// Try to push an element (move version)
    [[nodiscard]] bool try_push(T&& item) noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t next_head = (head + 1) & mask_;

        if (next_head == cached_tail_) {
            cached_tail_ = tail_.load(std::memory_order_acquire);
            if (next_head == cached_tail_) {
                return false;
            }
        }

        buffer_[head] = std::move(item);
        head_.store(next_head, std::memory_order_release);
        return true;
    }

    /// Push with spin wait (blocks until successful)
    void push(const T& item) noexcept {
        while (!try_push(item)) {
            // Spin - could add pause instruction here
            #if defined(__x86_64__) || defined(_M_X64)
            asm volatile("pause" ::: "memory");
            #endif
        }
    }

    /// Emplace an element in-place
    template<typename... Args>
    [[nodiscard]] bool try_emplace(Args&&... args) noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t next_head = (head + 1) & mask_;

        if (next_head == cached_tail_) {
            cached_tail_ = tail_.load(std::memory_order_acquire);
            if (next_head == cached_tail_) {
                return false;
            }
        }

        std::construct_at(&buffer_[head], std::forward<Args>(args)...);
        head_.store(next_head, std::memory_order_release);
        return true;
    }

    // ========================================================================
    // Consumer Interface (single thread only)
    // ========================================================================

    /// Try to pop an element (consumer only)
    /// @return The element if available, nullopt if queue is empty
    [[nodiscard]] std::optional<T> try_pop() noexcept {
        const size_t tail = tail_.load(std::memory_order_relaxed);

        // Check if queue is empty
        if (tail == cached_head_) {
            // Refresh cached head
            cached_head_ = head_.load(std::memory_order_acquire);
            if (tail == cached_head_) {
                return std::nullopt;  // Queue is empty
            }
        }

        // Load the item
        T item = std::move(buffer_[tail]);

        // Mark slot as consumed
        const size_t next_tail = (tail + 1) & mask_;
        tail_.store(next_tail, std::memory_order_release);

        return item;
    }

    /// Pop with output parameter (avoids optional overhead)
    /// @return true if successful, false if queue is empty
    [[nodiscard]] bool try_pop(T& item) noexcept {
        const size_t tail = tail_.load(std::memory_order_relaxed);

        if (tail == cached_head_) {
            cached_head_ = head_.load(std::memory_order_acquire);
            if (tail == cached_head_) {
                return false;
            }
        }

        item = std::move(buffer_[tail]);
        const size_t next_tail = (tail + 1) & mask_;
        tail_.store(next_tail, std::memory_order_release);

        return true;
    }

    /// Pop with spin wait (blocks until successful)
    [[nodiscard]] T pop() noexcept {
        T item;
        while (!try_pop(item)) {
            #if defined(__x86_64__) || defined(_M_X64)
            asm volatile("pause" ::: "memory");
            #endif
        }
        return item;
    }

    /// Peek at the front element without removing it
    [[nodiscard]] const T* front() const noexcept {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        const size_t head = head_.load(std::memory_order_acquire);

        if (tail == head) {
            return nullptr;  // Queue is empty
        }

        return &buffer_[tail];
    }

    // ========================================================================
    // Status Queries (thread-safe)
    // ========================================================================

    /// Check if queue is empty
    [[nodiscard]] bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

    /// Get approximate size (may be stale)
    [[nodiscard]] size_t size_approx() const noexcept {
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t tail = tail_.load(std::memory_order_acquire);
        return (head - tail) & mask_;
    }

    /// Get capacity
    [[nodiscard]] static constexpr size_t capacity() noexcept {
        return Capacity - 1;  // One slot reserved for full detection
    }

    /// Check if queue is full
    [[nodiscard]] bool full() const noexcept {
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t tail = tail_.load(std::memory_order_acquire);
        return ((head + 1) & mask_) == tail;
    }

private:
    static constexpr size_t mask_ = Capacity - 1;

    // Cache line padded to avoid false sharing
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> head_{0};
    alignas(CACHE_LINE_SIZE) size_t cached_tail_{0};  // Producer's cached view of tail

    alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail_{0};
    alignas(CACHE_LINE_SIZE) size_t cached_head_{0};  // Consumer's cached view of head

    alignas(CACHE_LINE_SIZE) std::array<T, Capacity> buffer_{};
};

// ============================================================================
// Bounded SPSC Queue (with byte limit)
// ============================================================================

/// SPSC queue with both count and byte limits
/// Useful for message queues where message sizes vary
template<typename T, size_t MaxMessages, size_t MaxBytes>
class BoundedSPSCQueue {
    static_assert((MaxMessages & (MaxMessages - 1)) == 0, "MaxMessages must be power of 2");

public:
    BoundedSPSCQueue() noexcept = default;

    /// Try to push with size tracking
    [[nodiscard]] bool try_push(const T& item, size_t item_bytes) noexcept {
        size_t current_bytes = bytes_used_.load(std::memory_order_relaxed);
        if (current_bytes + item_bytes > MaxBytes) {
            return false;
        }

        if (!queue_.try_push(item)) {
            return false;
        }

        bytes_used_.fetch_add(item_bytes, std::memory_order_relaxed);
        return true;
    }

    /// Try to pop with size tracking
    [[nodiscard]] bool try_pop(T& item, size_t item_bytes) noexcept {
        if (!queue_.try_pop(item)) {
            return false;
        }

        bytes_used_.fetch_sub(item_bytes, std::memory_order_relaxed);
        return true;
    }

    [[nodiscard]] bool empty() const noexcept { return queue_.empty(); }
    [[nodiscard]] size_t size_approx() const noexcept { return queue_.size_approx(); }
    [[nodiscard]] size_t bytes_used() const noexcept {
        return bytes_used_.load(std::memory_order_relaxed);
    }

private:
    SPSCQueue<T, MaxMessages> queue_;
    std::atomic<size_t> bytes_used_{0};
};

} // namespace nfx::memory
