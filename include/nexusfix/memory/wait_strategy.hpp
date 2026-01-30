/*
    NexusFIX Wait Strategies

    LMAX Disruptor-inspired wait strategies for lock-free queues.
    Trade-off between latency and CPU usage.

    | Strategy      | Latency  | CPU Usage | Use Case                |
    |---------------|----------|-----------|-------------------------|
    | BusySpinWait  | Lowest   | 100%      | HFT hot path            |
    | YieldingWait  | Low      | High      | Active trading          |
    | SleepingWait  | Medium   | Low       | Background processing   |
    | BackoffWait   | Adaptive | Variable  | General purpose         |
*/

#pragma once

#include <atomic>
#include <chrono>
#include <thread>
#include <cstdint>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif

namespace nfx::memory {

// ============================================================================
// Busy Spin Wait Strategy
// ============================================================================

/// Lowest latency, highest CPU usage
/// Uses CPU pause instruction to reduce power and allow other hyperthreads
struct BusySpinWait {
    static constexpr const char* name() noexcept { return "BusySpinWait"; }

    static void wait() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
        _mm_pause();
#elif defined(__aarch64__)
        asm volatile("yield" ::: "memory");
#else
        // Fallback: compiler barrier
        std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
    }

    /// Wait until condition is met
    template<typename Predicate>
    static void wait_until(Predicate&& pred) noexcept {
        while (!pred()) {
            wait();
        }
    }

    /// Wait for sequence to reach target
    static void wait_for_sequence(
        const std::atomic<size_t>& sequence,
        size_t target) noexcept
    {
        while (sequence.load(std::memory_order_acquire) < target) {
            wait();
        }
    }
};

// ============================================================================
// Yielding Wait Strategy
// ============================================================================

/// Low latency, gives up CPU to other threads
/// Good when multiple threads share same core
struct YieldingWait {
    static constexpr const char* name() noexcept { return "YieldingWait"; }

    static void wait() noexcept {
        std::this_thread::yield();
    }

    template<typename Predicate>
    static void wait_until(Predicate&& pred) noexcept {
        while (!pred()) {
            wait();
        }
    }

    static void wait_for_sequence(
        const std::atomic<size_t>& sequence,
        size_t target) noexcept
    {
        while (sequence.load(std::memory_order_acquire) < target) {
            wait();
        }
    }
};

// ============================================================================
// Sleeping Wait Strategy
// ============================================================================

/// Medium latency, low CPU usage
/// Good for background tasks where latency is not critical
template<uint32_t SleepMicroseconds = 1>
struct SleepingWait {
    static constexpr const char* name() noexcept { return "SleepingWait"; }

    static void wait() noexcept {
        std::this_thread::sleep_for(
            std::chrono::microseconds(SleepMicroseconds));
    }

    template<typename Predicate>
    static void wait_until(Predicate&& pred) noexcept {
        while (!pred()) {
            wait();
        }
    }

    static void wait_for_sequence(
        const std::atomic<size_t>& sequence,
        size_t target) noexcept
    {
        while (sequence.load(std::memory_order_acquire) < target) {
            wait();
        }
    }
};

// ============================================================================
// Exponential Backoff Wait Strategy
// ============================================================================

/// Adaptive strategy: starts with spin, escalates to yield, then sleep
/// Good general-purpose strategy that adapts to contention level
template<uint32_t SpinIterations = 100,
         uint32_t YieldIterations = 10,
         uint32_t MaxSleepMicroseconds = 1000>
struct BackoffWait {
    static constexpr const char* name() noexcept { return "BackoffWait"; }

    // State for tracking backoff level
    struct State {
        uint32_t spin_count = 0;
        uint32_t yield_count = 0;
        uint32_t sleep_us = 1;
    };

    /// Single wait iteration with state tracking
    static void wait(State& state) noexcept {
        if (state.spin_count < SpinIterations) {
            // Phase 1: Spin
            state.spin_count++;
#if defined(__x86_64__) || defined(_M_X64)
            _mm_pause();
#elif defined(__aarch64__)
            asm volatile("yield" ::: "memory");
#endif
        } else if (state.yield_count < YieldIterations) {
            // Phase 2: Yield
            state.yield_count++;
            std::this_thread::yield();
        } else {
            // Phase 3: Sleep with exponential backoff
            std::this_thread::sleep_for(
                std::chrono::microseconds(state.sleep_us));
            if (state.sleep_us < MaxSleepMicroseconds) {
                state.sleep_us *= 2;
            }
        }
    }

    /// Reset state after successful operation
    static void reset(State& state) noexcept {
        state.spin_count = 0;
        state.yield_count = 0;
        state.sleep_us = 1;
    }

    /// Stateless wait (uses local state)
    static void wait() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
        _mm_pause();
#elif defined(__aarch64__)
        asm volatile("yield" ::: "memory");
#endif
    }

    template<typename Predicate>
    static void wait_until(Predicate&& pred) noexcept {
        State state{};
        while (!pred()) {
            wait(state);
        }
    }

    static void wait_for_sequence(
        const std::atomic<size_t>& sequence,
        size_t target) noexcept
    {
        State state{};
        while (sequence.load(std::memory_order_acquire) < target) {
            wait(state);
        }
    }
};

// ============================================================================
// Wait Strategy Concept
// ============================================================================

/// Concept for wait strategies
template<typename T>
concept WaitStrategy = requires(T) {
    { T::wait() } noexcept;
    { T::name() } noexcept -> std::same_as<const char*>;
};

// Static assertions
static_assert(WaitStrategy<BusySpinWait>);
static_assert(WaitStrategy<YieldingWait>);
static_assert(WaitStrategy<SleepingWait<>>);
static_assert(WaitStrategy<BackoffWait<>>);

} // namespace nfx::memory
