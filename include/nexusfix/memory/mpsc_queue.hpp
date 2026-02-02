/*
    NexusFIX Lock-Free MPSC Queue

    Multi-Producer Single-Consumer queue based on LMAX Disruptor pattern.

    Design:
    - Multiple producers can push concurrently (lock-free)
    - Single consumer pops elements
    - Claim-Publish pattern ensures ordering
    - Cache-line padded to avoid false sharing

    Key Operations:
    1. Producer claims slot (atomic fetch_add)
    2. Producer writes data to claimed slot
    3. Producer waits for previous publishers to complete (ordering)
    4. Producer publishes (makes data visible to consumer)
    5. Consumer waits for published data
    6. Consumer reads and advances tail

    Use cases:
    - Multiple market data feeds -> Single strategy thread
    - Multiple exchange connections -> Session manager
    - Multiple threads -> Single log writer
*/

#pragma once

#include <atomic>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>
#include <new>

#include "wait_strategy.hpp"
#include "nexusfix/util/compiler.hpp"

namespace nfx::memory {

// ============================================================================
// MPSC Queue
// ============================================================================

/// Lock-free Multi-Producer Single-Consumer queue
/// @tparam T Element type
/// @tparam Capacity Must be power of 2
/// @tparam WaitStrategyT Wait strategy for spin loops
template<typename T,
         size_t Capacity,
         typename WaitStrategyT = BusySpinWait>
class MPSCQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    static_assert(Capacity >= 2, "Capacity must be at least 2");
    static_assert(WaitStrategy<WaitStrategyT>, "Invalid wait strategy");

public:
    using value_type = T;
    using wait_strategy = WaitStrategyT;

    MPSCQueue() noexcept {
        // Initialize all sequences to mark slots as empty
        NFX_ASSUME(Capacity >= 2);  // Enforced by static_assert
        for (size_t i = 0; i < Capacity; ++i) {
            sequences_[i].value.store(i, std::memory_order_relaxed);
        }
    }

    // Non-copyable, non-movable
    MPSCQueue(const MPSCQueue&) = delete;
    MPSCQueue& operator=(const MPSCQueue&) = delete;
    MPSCQueue(MPSCQueue&&) = delete;
    MPSCQueue& operator=(MPSCQueue&&) = delete;

    // ========================================================================
    // Producer Interface (multiple threads)
    // ========================================================================

    /// Try to push an element (producer, multiple threads)
    /// @return true if successful, false if queue is full
    [[nodiscard]] bool try_push(const T& item) noexcept {
        size_t head = head_.load(std::memory_order_relaxed);

        for (;;) {
            const size_t slot = head & mask_;
            const size_t seq = sequences_[slot].value.load(std::memory_order_acquire);
            const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(head);

            if (diff == 0) {
                // Slot is available, try to claim it
                if (head_.compare_exchange_weak(head, head + 1,
                        std::memory_order_relaxed)) {
                    // Successfully claimed slot, write data
                    buffer_[slot] = item;
                    // Publish: mark slot as filled
                    sequences_[slot].value.store(head + 1, std::memory_order_release);
                    return true;
                }
                // CAS failed, another producer got there first, retry
            } else if (diff < 0) {
                // Queue is full (consumer hasn't caught up)
                return false;
            } else {
                // Slot was just filled by another producer, reload head
                head = head_.load(std::memory_order_relaxed);
            }
        }
    }

    /// Try to push an element (move version)
    [[nodiscard]] bool try_push(T&& item) noexcept {
        size_t head = head_.load(std::memory_order_relaxed);

        for (;;) {
            const size_t slot = head & mask_;
            const size_t seq = sequences_[slot].value.load(std::memory_order_acquire);
            const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(head);

            if (diff == 0) {
                if (head_.compare_exchange_weak(head, head + 1,
                        std::memory_order_relaxed)) {
                    buffer_[slot] = std::move(item);
                    sequences_[slot].value.store(head + 1, std::memory_order_release);
                    return true;
                }
            } else if (diff < 0) {
                return false;
            } else {
                head = head_.load(std::memory_order_relaxed);
            }
        }
    }

    /// Push with spin wait (blocks until successful)
    void push(const T& item) noexcept {
        while (!try_push(item)) {
            WaitStrategyT::wait();
        }
    }

    /// Emplace an element in-place (producer, multiple threads)
    template<typename... Args>
    [[nodiscard]] bool try_emplace(Args&&... args) noexcept {
        size_t head = head_.load(std::memory_order_relaxed);

        for (;;) {
            const size_t slot = head & mask_;
            const size_t seq = sequences_[slot].value.load(std::memory_order_acquire);
            const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(head);

            if (diff == 0) {
                if (head_.compare_exchange_weak(head, head + 1,
                        std::memory_order_relaxed)) {
                    std::construct_at(&buffer_[slot], std::forward<Args>(args)...);
                    sequences_[slot].value.store(head + 1, std::memory_order_release);
                    return true;
                }
            } else if (diff < 0) {
                return false;
            } else {
                head = head_.load(std::memory_order_relaxed);
            }
        }
    }

    // ========================================================================
    // Consumer Interface (single thread only)
    // ========================================================================

    /// Try to pop an element (consumer, single thread only)
    /// @return The element if available, nullopt if queue is empty
    [[nodiscard]] std::optional<T> try_pop() noexcept {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        const size_t slot = tail & mask_;
        const size_t seq = sequences_[slot].value.load(std::memory_order_acquire);
        const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(tail + 1);

        if (diff < 0) {
            // Queue is empty or slot not yet published
            return std::nullopt;
        }

        // Load the item
        T item = std::move(buffer_[slot]);

        // Mark slot as consumed (ready for next round)
        sequences_[slot].value.store(tail + Capacity, std::memory_order_release);
        tail_.store(tail + 1, std::memory_order_relaxed);

        return item;
    }

    /// Pop with output parameter (avoids optional overhead)
    /// @return true if successful, false if queue is empty
    [[nodiscard]] bool try_pop(T& item) noexcept {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        const size_t slot = tail & mask_;
        const size_t seq = sequences_[slot].value.load(std::memory_order_acquire);
        const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(tail + 1);

        if (diff < 0) {
            return false;
        }

        item = std::move(buffer_[slot]);
        sequences_[slot].value.store(tail + Capacity, std::memory_order_release);
        tail_.store(tail + 1, std::memory_order_relaxed);

        return true;
    }

    /// Pop with spin wait (blocks until successful)
    [[nodiscard]] T pop() noexcept {
        T item;
        while (!try_pop(item)) {
            WaitStrategyT::wait();
        }
        return item;
    }

    // ========================================================================
    // Batch Operations (Consumer only)
    // ========================================================================

    /// Try to pop multiple elements at once
    /// @param out Output iterator
    /// @param max_count Maximum number of elements to pop
    /// @return Number of elements popped
    template<typename OutputIt>
    [[nodiscard]] size_t try_pop_batch(OutputIt out, size_t max_count) noexcept {
        size_t count = 0;
        T item;
        while (count < max_count && try_pop(item)) {
            *out++ = std::move(item);
            ++count;
        }
        return count;
    }

    // ========================================================================
    // Status Queries (thread-safe)
    // ========================================================================

    /// Check if queue is empty (approximate)
    [[nodiscard]] bool empty() const noexcept {
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t tail = tail_.load(std::memory_order_acquire);
        return head == tail;
    }

    /// Get approximate size (may be stale)
    [[nodiscard]] size_t size_approx() const noexcept {
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t tail = tail_.load(std::memory_order_acquire);
        return head - tail;
    }

    /// Get capacity (actual usable slots)
    [[nodiscard]] static constexpr size_t capacity() noexcept {
        return Capacity;
    }

    /// Check if queue is full (approximate)
    [[nodiscard]] bool full() const noexcept {
        return size_approx() >= Capacity;
    }

private:
    static constexpr size_t mask_ = Capacity - 1;

    // Sequence array - each slot has a sequence number indicating its state
    // Sequence == slot_index: slot is empty, ready for write
    // Sequence == slot_index + 1: slot is filled, ready for read
    // Sequence == slot_index + Capacity: slot consumed, ready for next cycle
    struct alignas(CACHE_LINE_SIZE) PaddedSequence {
        std::atomic<size_t> value{0};
    };
    std::array<PaddedSequence, Capacity> sequences_{};

    // Producer side (multiple writers contend here)
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> head_{0};

    // Consumer side (single reader)
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail_{0};

    // Data buffer
    alignas(CACHE_LINE_SIZE) std::array<T, Capacity> buffer_{};
};

// ============================================================================
// Simple MPSC Queue (Alternative Implementation)
// ============================================================================

/// Simpler MPSC queue using atomic head with CAS
/// Less overhead per operation but may have higher contention
template<typename T,
         size_t Capacity,
         typename WaitStrategyT = BusySpinWait>
class SimpleMPSCQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    static_assert(Capacity >= 2, "Capacity must be at least 2");

public:
    SimpleMPSCQueue() noexcept = default;

    // Non-copyable, non-movable
    SimpleMPSCQueue(const SimpleMPSCQueue&) = delete;
    SimpleMPSCQueue& operator=(const SimpleMPSCQueue&) = delete;
    SimpleMPSCQueue(SimpleMPSCQueue&&) = delete;
    SimpleMPSCQueue& operator=(SimpleMPSCQueue&&) = delete;

    /// Try to push an element (producer, multiple threads)
    [[nodiscard]] bool try_push(const T& item) noexcept {
        // Step 1: Claim slot atomically
        const size_t claimed = claim_head_.fetch_add(1, std::memory_order_acq_rel);
        const size_t slot = claimed & mask_;

        // Step 2: Check if we've overrun the consumer
        const size_t tail = tail_.load(std::memory_order_acquire);
        if (claimed - tail >= Capacity) {
            // Queue full - unclaim (best effort, may leave a gap)
            claim_head_.fetch_sub(1, std::memory_order_relaxed);
            return false;
        }

        // Step 3: Write data
        buffer_[slot] = item;

        // Step 4: Wait for previous producers to publish
        // This ensures FIFO ordering
        while (publish_head_.load(std::memory_order_acquire) != claimed) {
            WaitStrategyT::wait();
        }

        // Step 5: Publish
        publish_head_.store(claimed + 1, std::memory_order_release);
        return true;
    }

    /// Try to pop an element (consumer, single thread only)
    [[nodiscard]] bool try_pop(T& item) noexcept {
        const size_t tail = tail_.load(std::memory_order_relaxed);

        // Check if data is available
        if (tail >= publish_head_.load(std::memory_order_acquire)) {
            return false;
        }

        // Read data
        item = std::move(buffer_[tail & mask_]);

        // Advance tail
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool empty() const noexcept {
        return tail_.load(std::memory_order_acquire) >=
               publish_head_.load(std::memory_order_acquire);
    }

    [[nodiscard]] size_t size_approx() const noexcept {
        const size_t pub = publish_head_.load(std::memory_order_acquire);
        const size_t tail = tail_.load(std::memory_order_acquire);
        return pub > tail ? pub - tail : 0;
    }

    [[nodiscard]] static constexpr size_t capacity() noexcept {
        return Capacity;
    }

private:
    static constexpr size_t mask_ = Capacity - 1;

    // Producer side
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> claim_head_{0};
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> publish_head_{0};

    // Consumer side
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail_{0};

    // Data buffer
    alignas(CACHE_LINE_SIZE) std::array<T, Capacity> buffer_{};
};

} // namespace nfx::memory
