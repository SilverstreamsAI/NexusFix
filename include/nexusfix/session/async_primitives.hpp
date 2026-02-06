#pragma once

#include <atomic>
#include <chrono>
#include <coroutine>
#include <optional>
#include <vector>

#include "nexusfix/session/coroutine.hpp"

namespace nfx {

// ============================================================================
// AsyncMutex - CAS-based Non-blocking Mutex for Coroutines
// ============================================================================

/// Lock-free async mutex using intrusive waiter list.
/// State encoding:
///   nullptr           = unlocked
///   &locked_sentinel_ = locked, no waiters
///   other pointer     = locked, head of waiter linked list
class AsyncMutex {
public:
    AsyncMutex() noexcept : state_{nullptr} {}

    AsyncMutex(const AsyncMutex&) = delete;
    AsyncMutex& operator=(const AsyncMutex&) = delete;

    /// RAII scoped lock returned by co_await scoped_lock()
    class ScopedLock {
    public:
        explicit ScopedLock(AsyncMutex& mutex) noexcept : mutex_{&mutex} {}

        ScopedLock(ScopedLock&& other) noexcept : mutex_{other.mutex_} {
            other.mutex_ = nullptr;
        }

        ScopedLock& operator=(ScopedLock&& other) noexcept {
            if (this != &other) {
                if (mutex_) mutex_->unlock();
                mutex_ = other.mutex_;
                other.mutex_ = nullptr;
            }
            return *this;
        }

        ~ScopedLock() {
            if (mutex_) mutex_->unlock();
        }

        ScopedLock(const ScopedLock&) = delete;
        ScopedLock& operator=(const ScopedLock&) = delete;

    private:
        AsyncMutex* mutex_;
    };

    /// Awaiter returned by scoped_lock()
    class LockAwaiter {
    public:
        explicit LockAwaiter(AsyncMutex& mutex) noexcept : mutex_{mutex} {}

        bool await_ready() noexcept {
            // Fast path: try CAS from unlocked (nullptr) -> locked_no_waiters
            void* expected = nullptr;
            return mutex_.state_.compare_exchange_strong(
                expected, mutex_.locked_sentinel(),
                std::memory_order_acquire,
                std::memory_order_relaxed);
        }

        bool await_suspend(std::coroutine_handle<> handle) noexcept {
            handle_ = handle;

            void* old_state = mutex_.state_.load(std::memory_order_relaxed);
            while (true) {
                if (old_state == nullptr) {
                    // Mutex became unlocked, try to acquire
                    if (mutex_.state_.compare_exchange_weak(
                            old_state, mutex_.locked_sentinel(),
                            std::memory_order_acquire,
                            std::memory_order_relaxed)) {
                        return false;  // Acquired - don't suspend
                    }
                    continue;
                }

                // Mutex is locked, enqueue ourselves
                next_ = (old_state == mutex_.locked_sentinel())
                    ? nullptr
                    : static_cast<LockAwaiter*>(old_state);

                if (mutex_.state_.compare_exchange_weak(
                        old_state, static_cast<void*>(this),
                        std::memory_order_release,
                        std::memory_order_relaxed)) {
                    return true;  // Enqueued - suspend
                }
            }
        }

        ScopedLock await_resume() noexcept {
            return ScopedLock{mutex_};
        }

    private:
        friend class AsyncMutex;
        AsyncMutex& mutex_;
        std::coroutine_handle<> handle_{};
        LockAwaiter* next_{nullptr};
    };

    /// Acquire lock with RAII scope
    [[nodiscard]] LockAwaiter scoped_lock() noexcept {
        return LockAwaiter{*this};
    }

    /// Unlock the mutex, resuming next waiter if any
    void unlock() noexcept {
        void* old_state = state_.load(std::memory_order_relaxed);

        // Fast path: no waiters
        if (old_state == locked_sentinel()) {
            if (state_.compare_exchange_strong(
                    old_state, nullptr,
                    std::memory_order_release,
                    std::memory_order_relaxed)) {
                return;
            }
        }

        // Slow path: pop and resume next waiter
        while (true) {
            old_state = state_.load(std::memory_order_acquire);
            if (old_state == locked_sentinel()) {
                if (state_.compare_exchange_weak(
                        old_state, nullptr,
                        std::memory_order_release,
                        std::memory_order_relaxed)) {
                    return;
                }
                continue;
            }

            auto* waiter = static_cast<LockAwaiter*>(old_state);
            void* new_state = waiter->next_
                ? static_cast<void*>(waiter->next_)
                : locked_sentinel();

            if (state_.compare_exchange_weak(
                    old_state, new_state,
                    std::memory_order_acq_rel,
                    std::memory_order_relaxed)) {
                waiter->handle_.resume();
                return;
            }
        }
    }

private:
    void* locked_sentinel() noexcept {
        return static_cast<void*>(&locked_sentinel_tag_);
    }

    alignas(8) std::atomic<void*> state_;
    alignas(8) char locked_sentinel_tag_{};  // Address used as sentinel for "locked, no waiters"
};

// ============================================================================
// Event - Binary Signal for Coroutines
// ============================================================================

/// One-shot or resettable binary event.
/// State encoding:
///   nullptr          = not set, no waiters
///   &set_sentinel_   = set
///   other pointer    = not set, head of waiter linked list
class Event {
public:
    Event() noexcept : state_{nullptr} {}

    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;

    /// Set the event, resuming all waiters
    void set() noexcept {
        void* old_state = state_.exchange(set_sentinel(), std::memory_order_acq_rel);
        if (old_state != nullptr && old_state != set_sentinel()) {
            // old_state is head of waiter list - resume all
            auto* waiter = static_cast<EventAwaiter*>(old_state);
            while (waiter) {
                auto* next = waiter->next_;
                waiter->handle_.resume();
                waiter = next;
            }
        }
    }

    /// Reset event to unset state
    void reset() noexcept {
        void* expected = set_sentinel();
        state_.compare_exchange_strong(
            expected, nullptr,
            std::memory_order_relaxed,
            std::memory_order_relaxed);
    }

    /// Check if event is set
    [[nodiscard]] bool is_set() const noexcept {
        return state_.load(std::memory_order_acquire) == const_set_sentinel();
    }

    class EventAwaiter {
    public:
        explicit EventAwaiter(Event& event) noexcept : event_{event} {}

        bool await_ready() noexcept {
            return event_.state_.load(std::memory_order_acquire) == event_.set_sentinel();
        }

        bool await_suspend(std::coroutine_handle<> handle) noexcept {
            handle_ = handle;
            void* old_state = event_.state_.load(std::memory_order_relaxed);

            while (true) {
                if (old_state == event_.set_sentinel()) {
                    return false;  // Already set, don't suspend
                }

                // Enqueue into waiter list (nullptr means empty list)
                next_ = (old_state == nullptr)
                    ? nullptr
                    : static_cast<EventAwaiter*>(old_state);

                if (event_.state_.compare_exchange_weak(
                        old_state, static_cast<void*>(this),
                        std::memory_order_release,
                        std::memory_order_relaxed)) {
                    return true;
                }
            }
        }

        void await_resume() noexcept {}

    private:
        friend class Event;
        Event& event_;
        std::coroutine_handle<> handle_{};
        EventAwaiter* next_{nullptr};
    };

    EventAwaiter operator co_await() noexcept {
        return EventAwaiter{*this};
    }

private:
    void* set_sentinel() noexcept {
        return static_cast<void*>(&set_sentinel_tag_);
    }

    const void* const_set_sentinel() const noexcept {
        return static_cast<const void*>(&set_sentinel_tag_);
    }

    std::atomic<void*> state_;
    char set_sentinel_tag_{};  // Address used as sentinel for "set"
};

// ============================================================================
// WhenAll - Run Multiple Tasks to Completion
// ============================================================================

/// Run all tasks concurrently, resume parent when last one completes
inline Task<void> when_all(std::vector<Task<void>> tasks) {
    if (tasks.empty()) co_return;

    struct WhenAllState {
        std::atomic<size_t> remaining;
        std::coroutine_handle<> continuation{};
    };

    WhenAllState state;
    state.remaining.store(tasks.size(), std::memory_order_relaxed);

    // Driver coroutine wrapper for each task
    auto make_driver = [&state](Task<void> task) -> Task<void> {
        co_await task;
        if (state.remaining.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            if (state.continuation) {
                state.continuation.resume();
            }
        }
    };

    std::vector<Task<void>> drivers;
    drivers.reserve(tasks.size());
    for (auto& t : tasks) {
        drivers.push_back(make_driver(std::move(t)));
    }

    // Start all drivers
    for (auto& d : drivers) {
        d.resume();
    }

    // Suspend until all complete
    struct WhenAllAwaiter {
        WhenAllState& state;

        bool await_ready() noexcept {
            return state.remaining.load(std::memory_order_acquire) == 0;
        }

        void await_suspend(std::coroutine_handle<> handle) noexcept {
            state.continuation = handle;
            if (state.remaining.load(std::memory_order_acquire) == 0) {
                handle.resume();
            }
        }

        void await_resume() noexcept {}
    };

    co_await WhenAllAwaiter{state};
}

// ============================================================================
// WhenAny - Return Index of First Completing Task
// ============================================================================

/// Run tasks concurrently, return index of first completer
inline Task<size_t> when_any(std::vector<Task<void>> tasks) {
    if (tasks.empty()) co_return 0;

    struct WhenAnyState {
        std::atomic<bool> done{false};
        std::atomic<size_t> winner_index{0};
        std::coroutine_handle<> continuation{};
    };

    WhenAnyState state;

    auto make_driver = [&state](Task<void> task, size_t index) -> Task<void> {
        co_await task;
        bool expected = false;
        if (state.done.compare_exchange_strong(
                expected, true,
                std::memory_order_acq_rel,
                std::memory_order_relaxed)) {
            state.winner_index.store(index, std::memory_order_relaxed);
            if (state.continuation) {
                state.continuation.resume();
            }
        }
    };

    std::vector<Task<void>> drivers;
    drivers.reserve(tasks.size());
    for (size_t i = 0; i < tasks.size(); ++i) {
        drivers.push_back(make_driver(std::move(tasks[i]), i));
    }

    // Start all drivers
    for (auto& d : drivers) {
        d.resume();
    }

    // Suspend until one completes
    struct WhenAnyAwaiter {
        WhenAnyState& state;

        bool await_ready() noexcept {
            return state.done.load(std::memory_order_acquire);
        }

        void await_suspend(std::coroutine_handle<> handle) noexcept {
            state.continuation = handle;
            if (state.done.load(std::memory_order_acquire)) {
                handle.resume();
            }
        }

        size_t await_resume() noexcept {
            return state.winner_index.load(std::memory_order_relaxed);
        }
    };

    co_return co_await WhenAnyAwaiter{state};
}

// ============================================================================
// SleepAwaitable - Cooperative Sleep
// ============================================================================

/// Awaitable that checks a deadline using steady_clock.
/// await_ready returns true if deadline has already passed.
/// await_suspend stores the handle for external polling/resume.
class SleepAwaitable {
public:
    using Clock = std::chrono::steady_clock;

    explicit SleepAwaitable(std::chrono::milliseconds duration) noexcept
        : deadline_{Clock::now() + duration} {}

    bool await_ready() noexcept {
        return Clock::now() >= deadline_;
    }

    void await_suspend(std::coroutine_handle<> handle) noexcept {
        handle_ = handle;
        // In cooperative scheduling, immediately resume to allow polling
        handle.resume();
    }

    void await_resume() noexcept {}

    [[nodiscard]] Clock::time_point deadline() const noexcept { return deadline_; }
    [[nodiscard]] std::coroutine_handle<> handle() const noexcept { return handle_; }

private:
    Clock::time_point deadline_;
    std::coroutine_handle<> handle_{};
};

/// Create a cooperative sleep awaitable
[[nodiscard]] inline SleepAwaitable sleep_for(std::chrono::milliseconds duration) noexcept {
    return SleepAwaitable{duration};
}

// ============================================================================
// with_timeout - Race Operation Against Deadline
// ============================================================================

/// Run a task with a timeout. Returns the result if completed in time,
/// or std::nullopt if the timeout expired.
template <typename T>
Task<std::optional<T>> with_timeout(Task<T> operation, std::chrono::milliseconds timeout) {
    using Clock = std::chrono::steady_clock;
    auto deadline = Clock::now() + timeout;

    auto timeout_task = [deadline]() -> Task<void> {
        while (Clock::now() < deadline) {
            co_await Yield{};
        }
    }();

    std::optional<T> result;
    auto op_wrapper = [&result](Task<T> op) -> Task<void> {
        result = co_await op;
    }(std::move(operation));

    std::vector<Task<void>> tasks;
    tasks.push_back(std::move(op_wrapper));
    tasks.push_back(std::move(timeout_task));

    size_t winner = co_await when_any(std::move(tasks));

    if (winner == 0) {
        co_return result;
    }
    co_return std::nullopt;
}

/// Void specialization: returns true if completed, false if timed out
inline Task<bool> with_timeout(Task<void> operation, std::chrono::milliseconds timeout) {
    using Clock = std::chrono::steady_clock;
    auto deadline = Clock::now() + timeout;

    auto timeout_task = [deadline]() -> Task<void> {
        while (Clock::now() < deadline) {
            co_await Yield{};
        }
    }();

    std::vector<Task<void>> tasks;
    tasks.push_back(std::move(operation));
    tasks.push_back(std::move(timeout_task));

    size_t winner = co_await when_any(std::move(tasks));
    co_return winner == 0;
}

} // namespace nfx
