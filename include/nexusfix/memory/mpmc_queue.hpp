/*
    NexusFIX Lock-Free MPMC Queue

    Multi-Producer Multi-Consumer queue using Rigtorp-style turn-based
    slot synchronization.

    Design:
    - Each slot has an atomic turn counter
    - Producers claim ticket via head_.fetch_add(1), wait for slot turn
    - Consumers claim ticket via tail_.fetch_add(1), wait for slot turn
    - Per-slot cache-line alignment eliminates false sharing

    Turn protocol:
    - slot.turn == (ticket / Capacity) * 2       -> slot ready for write
    - slot.turn == (ticket / Capacity) * 2 + 1   -> slot ready for read

    Use cases:
    - Multiple strategy threads -> Multiple order routers
    - Work-stealing patterns across consumer threads
    - Fan-out from market data to multiple processors
*/

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <optional>
#include <type_traits>

#include "wait_strategy.hpp"
#include "nexusfix/util/compiler.hpp"

namespace nfx::memory {

// Cache line size constant (same as spsc_queue.hpp / buffer_pool.hpp)
inline constexpr size_t MPMC_CACHE_LINE = 64;

// ============================================================================
// MPMC Queue
// ============================================================================

/// Lock-free Multi-Producer Multi-Consumer queue
/// @tparam T Element type
/// @tparam Capacity Must be power of 2
/// @tparam WaitStrategyT Wait strategy for spin loops
template<typename T,
         size_t Capacity,
         typename WaitStrategyT = BusySpinWait>
class MPMCQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    static_assert(Capacity >= 2, "Capacity must be at least 2");
    static_assert(WaitStrategy<WaitStrategyT>, "Invalid wait strategy");

    // Turn-based slot: each slot tracks its readiness via an atomic turn counter
    struct Slot {
        alignas(MPMC_CACHE_LINE) std::atomic<size_t> turn{0};
        alignas(alignof(T)) std::byte storage[sizeof(T)];

        T* ptr() noexcept {
            return std::launder(reinterpret_cast<T*>(storage));
        }

        const T* ptr() const noexcept {
            return std::launder(reinterpret_cast<const T*>(storage));
        }
    };

public:
    using value_type = T;
    using wait_strategy = WaitStrategyT;

    MPMCQueue() noexcept = default;

    ~MPMCQueue() noexcept {
        // Destroy any remaining elements for non-trivially-destructible types
        if constexpr (!std::is_trivially_destructible_v<T>) {
            size_t head = head_.load(std::memory_order_relaxed);
            size_t tail = tail_.load(std::memory_order_relaxed);
            while (tail != head) {
                const size_t slot_idx = tail & mask_;
                slots_[slot_idx].ptr()->~T();
                ++tail;
            }
        }
    }

    // Non-copyable, non-movable
    MPMCQueue(const MPMCQueue&) = delete;
    MPMCQueue& operator=(const MPMCQueue&) = delete;
    MPMCQueue(MPMCQueue&&) = delete;
    MPMCQueue& operator=(MPMCQueue&&) = delete;

    // ========================================================================
    // Producer Interface (multiple threads)
    // ========================================================================

    /// Try to push an element (non-blocking)
    /// @return true if successful, false if queue is full
    [[nodiscard]] bool try_push(const T& item) noexcept {
        size_t head = head_.load(std::memory_order_relaxed);

        for (;;) {
            const size_t slot_idx = head & mask_;
            const size_t turn = slots_[slot_idx].turn.load(std::memory_order_acquire);
            const size_t expected_turn = (head / Capacity) * 2;

            if (turn == expected_turn) {
                // Slot is ready for write, try to claim it
                if (head_.compare_exchange_weak(head, head + 1,
                        std::memory_order_relaxed)) {
                    // Successfully claimed, write data
                    std::construct_at(slots_[slot_idx].ptr(), item);
                    // Publish: advance turn to signal consumer
                    slots_[slot_idx].turn.store(expected_turn + 1, std::memory_order_release);
                    return true;
                }
                // CAS failed, head was updated by another producer, retry
            } else if (turn < expected_turn) {
                // Slot not yet consumed from previous cycle - queue is full
                return false;
            } else {
                // Another producer already claimed this slot, reload head
                head = head_.load(std::memory_order_relaxed);
            }
        }
    }

    /// Try to push an element (move version, non-blocking)
    [[nodiscard]] bool try_push(T&& item) noexcept {
        size_t head = head_.load(std::memory_order_relaxed);

        for (;;) {
            const size_t slot_idx = head & mask_;
            const size_t turn = slots_[slot_idx].turn.load(std::memory_order_acquire);
            const size_t expected_turn = (head / Capacity) * 2;

            if (turn == expected_turn) {
                if (head_.compare_exchange_weak(head, head + 1,
                        std::memory_order_relaxed)) {
                    std::construct_at(slots_[slot_idx].ptr(), std::move(item));
                    slots_[slot_idx].turn.store(expected_turn + 1, std::memory_order_release);
                    return true;
                }
            } else if (turn < expected_turn) {
                return false;
            } else {
                head = head_.load(std::memory_order_relaxed);
            }
        }
    }

    /// Emplace an element in-place (non-blocking)
    template<typename... Args>
    [[nodiscard]] bool try_emplace(Args&&... args) noexcept {
        size_t head = head_.load(std::memory_order_relaxed);

        for (;;) {
            const size_t slot_idx = head & mask_;
            const size_t turn = slots_[slot_idx].turn.load(std::memory_order_acquire);
            const size_t expected_turn = (head / Capacity) * 2;

            if (turn == expected_turn) {
                if (head_.compare_exchange_weak(head, head + 1,
                        std::memory_order_relaxed)) {
                    std::construct_at(slots_[slot_idx].ptr(), std::forward<Args>(args)...);
                    slots_[slot_idx].turn.store(expected_turn + 1, std::memory_order_release);
                    return true;
                }
            } else if (turn < expected_turn) {
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

    /// Push with spin wait (move version, blocks until successful)
    void push(T&& item) noexcept {
        while (!try_push(std::move(item))) {
            WaitStrategyT::wait();
        }
    }

    // ========================================================================
    // Consumer Interface (multiple threads)
    // ========================================================================

    /// Try to pop an element (non-blocking)
    /// @return The element if available, nullopt if queue is empty
    [[nodiscard]] std::optional<T> try_pop() noexcept {
        size_t tail = tail_.load(std::memory_order_relaxed);

        for (;;) {
            const size_t slot_idx = tail & mask_;
            const size_t turn = slots_[slot_idx].turn.load(std::memory_order_acquire);
            const size_t expected_turn = (tail / Capacity) * 2 + 1;

            if (turn == expected_turn) {
                // Slot is ready for read, try to claim it
                if (tail_.compare_exchange_weak(tail, tail + 1,
                        std::memory_order_relaxed)) {
                    // Successfully claimed, read data
                    T item = std::move(*slots_[slot_idx].ptr());
                    slots_[slot_idx].ptr()->~T();
                    // Advance turn to signal producer for next cycle
                    slots_[slot_idx].turn.store(expected_turn + 1, std::memory_order_release);
                    return item;
                }
                // CAS failed, tail was updated by another consumer, retry
            } else if (turn < expected_turn) {
                // Slot not yet written - queue is empty
                return std::nullopt;
            } else {
                // Another consumer already claimed this slot, reload tail
                tail = tail_.load(std::memory_order_relaxed);
            }
        }
    }

    /// Try to pop with output parameter (non-blocking)
    /// @return true if successful, false if queue is empty
    [[nodiscard]] bool try_pop(T& item) noexcept {
        size_t tail = tail_.load(std::memory_order_relaxed);

        for (;;) {
            const size_t slot_idx = tail & mask_;
            const size_t turn = slots_[slot_idx].turn.load(std::memory_order_acquire);
            const size_t expected_turn = (tail / Capacity) * 2 + 1;

            if (turn == expected_turn) {
                if (tail_.compare_exchange_weak(tail, tail + 1,
                        std::memory_order_relaxed)) {
                    item = std::move(*slots_[slot_idx].ptr());
                    slots_[slot_idx].ptr()->~T();
                    slots_[slot_idx].turn.store(expected_turn + 1, std::memory_order_release);
                    return true;
                }
            } else if (turn < expected_turn) {
                return false;
            } else {
                tail = tail_.load(std::memory_order_relaxed);
            }
        }
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
    // Status Queries (thread-safe, approximate)
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

    /// Get capacity
    [[nodiscard]] static constexpr size_t capacity() noexcept {
        return Capacity;
    }

    /// Check if queue is full (approximate)
    [[nodiscard]] bool full() const noexcept {
        return size_approx() >= Capacity;
    }

private:
    static constexpr size_t mask_ = Capacity - 1;

    // Slot array - each slot has turn counter + storage
    std::array<Slot, Capacity> slots_{};

    // Producer side (multiple writers contend here)
    alignas(MPMC_CACHE_LINE) std::atomic<size_t> head_{0};

    // Consumer side (multiple readers contend here)
    alignas(MPMC_CACHE_LINE) std::atomic<size_t> tail_{0};
};

} // namespace nfx::memory
