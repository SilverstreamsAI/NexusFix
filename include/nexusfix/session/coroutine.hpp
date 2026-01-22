#pragma once

#include <coroutine>
#include <optional>
#include <variant>
#include <exception>
#include <utility>

#include "nexusfix/types/error.hpp"

namespace nfx {

// ============================================================================
// Task<T> - Lazy Coroutine for Async Operations
// ============================================================================

/// Lazy coroutine that produces a value of type T
template <typename T = void>
class Task {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
        std::optional<T> result;
        std::exception_ptr exception;
        std::coroutine_handle<> continuation;

        Task get_return_object() noexcept {
            return Task{handle_type::from_promise(*this)};
        }

        std::suspend_always initial_suspend() noexcept { return {}; }

        auto final_suspend() noexcept {
            struct FinalAwaiter {
                bool await_ready() noexcept { return false; }
                std::coroutine_handle<> await_suspend(handle_type h) noexcept {
                    if (h.promise().continuation) {
                        return h.promise().continuation;
                    }
                    return std::noop_coroutine();
                }
                void await_resume() noexcept {}
            };
            return FinalAwaiter{};
        }

        void return_value(T value) noexcept {
            result = std::move(value);
        }

        void unhandled_exception() noexcept {
            exception = std::current_exception();
        }
    };

    Task() noexcept : handle_{nullptr} {}

    explicit Task(handle_type h) noexcept : handle_{h} {}

    Task(Task&& other) noexcept : handle_{other.handle_} {
        other.handle_ = nullptr;
    }

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_) handle_.destroy();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    ~Task() {
        if (handle_) handle_.destroy();
    }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    /// Check if task is valid
    [[nodiscard]] explicit operator bool() const noexcept {
        return handle_ != nullptr;
    }

    /// Check if task is done
    [[nodiscard]] bool done() const noexcept {
        return handle_ && handle_.done();
    }

    /// Resume the coroutine
    void resume() {
        if (handle_ && !handle_.done()) {
            handle_.resume();
        }
    }

    /// Get the result (blocks until done)
    [[nodiscard]] T get() {
        while (!done()) {
            resume();
        }
        if (handle_.promise().exception) {
            std::rethrow_exception(handle_.promise().exception);
        }
        return std::move(*handle_.promise().result);
    }

    /// Awaiter for co_await
    auto operator co_await() noexcept {
        struct Awaiter {
            handle_type handle;

            bool await_ready() noexcept {
                return handle.done();
            }

            std::coroutine_handle<> await_suspend(std::coroutine_handle<> caller) noexcept {
                handle.promise().continuation = caller;
                return handle;
            }

            T await_resume() {
                if (handle.promise().exception) {
                    std::rethrow_exception(handle.promise().exception);
                }
                return std::move(*handle.promise().result);
            }
        };
        return Awaiter{handle_};
    }

private:
    handle_type handle_;
};

// ============================================================================
// Task<void> Specialization
// ============================================================================

template <>
class Task<void> {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
        std::exception_ptr exception;
        std::coroutine_handle<> continuation;

        Task get_return_object() noexcept {
            return Task{handle_type::from_promise(*this)};
        }

        std::suspend_always initial_suspend() noexcept { return {}; }

        auto final_suspend() noexcept {
            struct FinalAwaiter {
                bool await_ready() noexcept { return false; }
                std::coroutine_handle<> await_suspend(handle_type h) noexcept {
                    if (h.promise().continuation) {
                        return h.promise().continuation;
                    }
                    return std::noop_coroutine();
                }
                void await_resume() noexcept {}
            };
            return FinalAwaiter{};
        }

        void return_void() noexcept {}

        void unhandled_exception() noexcept {
            exception = std::current_exception();
        }
    };

    Task() noexcept : handle_{nullptr} {}
    explicit Task(handle_type h) noexcept : handle_{h} {}

    Task(Task&& other) noexcept : handle_{other.handle_} {
        other.handle_ = nullptr;
    }

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_) handle_.destroy();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    ~Task() {
        if (handle_) handle_.destroy();
    }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    [[nodiscard]] explicit operator bool() const noexcept { return handle_ != nullptr; }
    [[nodiscard]] bool done() const noexcept { return handle_ && handle_.done(); }

    void resume() {
        if (handle_ && !handle_.done()) {
            handle_.resume();
        }
    }

    void get() {
        while (!done()) {
            resume();
        }
        if (handle_.promise().exception) {
            std::rethrow_exception(handle_.promise().exception);
        }
    }

    auto operator co_await() noexcept {
        struct Awaiter {
            handle_type handle;

            bool await_ready() noexcept { return handle.done(); }

            std::coroutine_handle<> await_suspend(std::coroutine_handle<> caller) noexcept {
                handle.promise().continuation = caller;
                return handle;
            }

            void await_resume() {
                if (handle.promise().exception) {
                    std::rethrow_exception(handle.promise().exception);
                }
            }
        };
        return Awaiter{handle_};
    }

private:
    handle_type handle_;
};

// ============================================================================
// Generator<T> - Lazy Sequence Generator
// ============================================================================

/// Generator coroutine for producing sequences of values
template <typename T>
class Generator {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
        T current_value;
        std::exception_ptr exception;

        Generator get_return_object() noexcept {
            return Generator{handle_type::from_promise(*this)};
        }

        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        std::suspend_always yield_value(T value) noexcept {
            current_value = std::move(value);
            return {};
        }

        void return_void() noexcept {}

        void unhandled_exception() noexcept {
            exception = std::current_exception();
        }
    };

    Generator() noexcept : handle_{nullptr} {}
    explicit Generator(handle_type h) noexcept : handle_{h} {}

    Generator(Generator&& other) noexcept : handle_{other.handle_} {
        other.handle_ = nullptr;
    }

    Generator& operator=(Generator&& other) noexcept {
        if (this != &other) {
            if (handle_) handle_.destroy();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    ~Generator() {
        if (handle_) handle_.destroy();
    }

    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;

    /// Iterator for range-based for loops
    class Iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        Iterator() noexcept : handle_{nullptr} {}
        explicit Iterator(handle_type h) noexcept : handle_{h} {}

        T& operator*() noexcept {
            return handle_.promise().current_value;
        }

        Iterator& operator++() {
            handle_.resume();
            if (handle_.done()) {
                if (handle_.promise().exception) {
                    std::rethrow_exception(handle_.promise().exception);
                }
            }
            return *this;
        }

        bool operator==(const Iterator& other) const noexcept {
            if (!handle_ && !other.handle_) return true;
            if (!handle_ || !other.handle_) return false;
            return handle_.done() == other.handle_.done();
        }

        bool operator!=(const Iterator& other) const noexcept {
            return !(*this == other);
        }

    private:
        handle_type handle_;
    };

    Iterator begin() {
        if (handle_) {
            handle_.resume();
            if (handle_.done()) {
                return Iterator{};
            }
        }
        return Iterator{handle_};
    }

    Iterator end() noexcept {
        return Iterator{};
    }

    /// Get next value (returns nullopt when done)
    [[nodiscard]] std::optional<T> next() {
        if (!handle_ || handle_.done()) {
            return std::nullopt;
        }
        handle_.resume();
        if (handle_.done()) {
            return std::nullopt;
        }
        return handle_.promise().current_value;
    }

private:
    handle_type handle_;
};

// ============================================================================
// Awaitable Utilities
// ============================================================================

/// Suspend and yield control
struct Yield {
    bool await_ready() noexcept { return false; }
    void await_suspend(std::coroutine_handle<>) noexcept {}
    void await_resume() noexcept {}
};

/// Suspend with a condition
template <typename Pred>
struct SuspendIf {
    Pred predicate;

    bool await_ready() noexcept { return !predicate(); }
    void await_suspend(std::coroutine_handle<>) noexcept {}
    void await_resume() noexcept {}
};

/// Create a suspend_if awaitable
template <typename Pred>
SuspendIf<Pred> suspend_if(Pred pred) {
    return SuspendIf<Pred>{std::move(pred)};
}

// ============================================================================
// Result-Returning Awaitables
// ============================================================================

/// Awaitable that returns a result
template <typename T>
struct ReadyAwaitable {
    T value;

    bool await_ready() noexcept { return true; }
    void await_suspend(std::coroutine_handle<>) noexcept {}
    T await_resume() noexcept { return std::move(value); }
};

/// Create a ready awaitable
template <typename T>
ReadyAwaitable<T> ready(T value) {
    return ReadyAwaitable<T>{std::move(value)};
}

} // namespace nfx
