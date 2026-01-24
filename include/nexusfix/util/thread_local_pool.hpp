/*
    NexusFIX Thread-Local Object Pool

    High-performance thread-local object pool for hot path allocation.
    Eliminates heap allocation overhead by pre-allocating objects per-thread.

    Key features:
    - Zero-contention: Each thread has its own pool
    - Cache-friendly: Objects are cache-line aligned
    - Lock-free: No synchronization needed
    - Bounded: Fixed capacity prevents memory bloat

    Performance: ~3-5ns acquire/release vs ~50-100ns malloc/free
*/

#pragma once

#include <array>
#include <cstdint>
#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

namespace nfx::util {

// ============================================================================
// Cache Line Constants
// ============================================================================

#ifdef __cpp_lib_hardware_interference_size
    inline constexpr size_t CACHE_LINE_SIZE = std::hardware_destructive_interference_size;
#else
    inline constexpr size_t CACHE_LINE_SIZE = 64;
#endif

// ============================================================================
// Thread-Local Pool
// ============================================================================

/// Thread-local fixed-size object pool
/// @tparam T Object type (must be default constructible)
/// @tparam Capacity Maximum number of pooled objects per thread
template<typename T, size_t Capacity = 64>
class ThreadLocalPool {
    static_assert(Capacity > 0 && Capacity <= 4096, "Capacity must be 1-4096");

public:
    /// Get thread-local pool instance
    static ThreadLocalPool& instance() noexcept {
        thread_local ThreadLocalPool pool;
        return pool;
    }

    /// Acquire an object from the pool
    /// Returns nullptr if pool is exhausted (caller should fallback to heap)
    [[nodiscard]] [[gnu::hot]]
    T* acquire() noexcept {
        if (free_count_ == 0) {
            ++stats_.pool_exhausted;
            return nullptr;  // Pool exhausted
        }

        --free_count_;
        uint16_t idx = free_stack_[free_count_];
        ++stats_.acquires;

        return &objects_[idx];
    }

    /// Release an object back to the pool
    /// @param obj Must be a pointer previously returned by acquire()
    [[gnu::hot]]
    void release(T* obj) noexcept {
        if (!obj) return;

        // Calculate index from pointer
        ptrdiff_t offset = reinterpret_cast<char*>(obj) - reinterpret_cast<char*>(&objects_[0]);
        size_t idx = static_cast<size_t>(offset) / sizeof(T);

        if (idx >= Capacity) {
            ++stats_.invalid_releases;
            return;  // Not from this pool
        }

        if (free_count_ >= Capacity) {
            ++stats_.double_releases;
            return;  // Already full (double release?)
        }

        // Reset object to default state (optional, can be disabled for perf)
        if constexpr (std::is_trivially_destructible_v<T>) {
            // Skip reset for trivial types
        } else {
            obj->~T();
            new (obj) T{};
        }

        free_stack_[free_count_] = static_cast<uint16_t>(idx);
        ++free_count_;
        ++stats_.releases;
    }

    /// Get number of available objects
    [[nodiscard]] size_t available() const noexcept {
        return free_count_;
    }

    /// Get pool capacity
    [[nodiscard]] static constexpr size_t capacity() noexcept {
        return Capacity;
    }

    /// Pool statistics
    struct Stats {
        uint64_t acquires{0};
        uint64_t releases{0};
        uint64_t pool_exhausted{0};
        uint64_t invalid_releases{0};
        uint64_t double_releases{0};
    };

    [[nodiscard]] const Stats& stats() const noexcept {
        return stats_;
    }

    void reset_stats() noexcept {
        stats_ = Stats{};
    }

private:
    ThreadLocalPool() noexcept {
        // Initialize free stack (all objects available)
        for (size_t i = 0; i < Capacity; ++i) {
            free_stack_[i] = static_cast<uint16_t>(i);
        }
        free_count_ = Capacity;
    }

    // Prevent copying
    ThreadLocalPool(const ThreadLocalPool&) = delete;
    ThreadLocalPool& operator=(const ThreadLocalPool&) = delete;

    alignas(CACHE_LINE_SIZE) std::array<T, Capacity> objects_{};
    std::array<uint16_t, Capacity> free_stack_{};
    size_t free_count_{0};
    Stats stats_{};
};

// ============================================================================
// Pooled Pointer (RAII wrapper)
// ============================================================================

/// RAII wrapper for pooled objects - automatically releases on destruction
template<typename T, size_t Capacity = 64>
class PooledPtr {
public:
    using Pool = ThreadLocalPool<T, Capacity>;

    PooledPtr() noexcept : ptr_{nullptr}, from_pool_{false} {}

    /// Acquire from pool, fallback to heap if exhausted
    [[nodiscard]] static PooledPtr acquire() noexcept {
        PooledPtr p;
        p.ptr_ = Pool::instance().acquire();
        if (p.ptr_) {
            p.from_pool_ = true;
        } else {
            // Fallback to heap
            p.ptr_ = new (std::nothrow) T{};
            p.from_pool_ = false;
        }
        return p;
    }

    /// Acquire from pool only (no heap fallback)
    [[nodiscard]] static PooledPtr acquire_pooled_only() noexcept {
        PooledPtr p;
        p.ptr_ = Pool::instance().acquire();
        p.from_pool_ = (p.ptr_ != nullptr);
        return p;
    }

    ~PooledPtr() {
        release();
    }

    // Move-only
    PooledPtr(PooledPtr&& other) noexcept
        : ptr_{other.ptr_}, from_pool_{other.from_pool_} {
        other.ptr_ = nullptr;
        other.from_pool_ = false;
    }

    PooledPtr& operator=(PooledPtr&& other) noexcept {
        if (this != &other) {
            release();
            ptr_ = other.ptr_;
            from_pool_ = other.from_pool_;
            other.ptr_ = nullptr;
            other.from_pool_ = false;
        }
        return *this;
    }

    // No copying
    PooledPtr(const PooledPtr&) = delete;
    PooledPtr& operator=(const PooledPtr&) = delete;

    /// Release object back to pool/heap
    void release() noexcept {
        if (ptr_) {
            if (from_pool_) {
                Pool::instance().release(ptr_);
            } else {
                delete ptr_;
            }
            ptr_ = nullptr;
            from_pool_ = false;
        }
    }

    /// Access operators
    T* get() noexcept { return ptr_; }
    const T* get() const noexcept { return ptr_; }
    T* operator->() noexcept { return ptr_; }
    const T* operator->() const noexcept { return ptr_; }
    T& operator*() noexcept { return *ptr_; }
    const T& operator*() const noexcept { return *ptr_; }

    /// Check validity
    explicit operator bool() const noexcept { return ptr_ != nullptr; }
    [[nodiscard]] bool is_from_pool() const noexcept { return from_pool_; }

private:
    T* ptr_;
    bool from_pool_;
};

// ============================================================================
// FIX Message Buffer Pool
// ============================================================================

/// Pre-sized buffer for FIX messages (typical max ~4KB)
struct alignas(CACHE_LINE_SIZE) MessageBuffer {
    static constexpr size_t MAX_SIZE = 4096;

    char data[MAX_SIZE];
    size_t length{0};

    void set(const char* src, size_t len) noexcept {
        length = (len <= MAX_SIZE) ? len : MAX_SIZE;
        __builtin_memcpy(data, src, length);
    }

    void clear() noexcept {
        length = 0;
    }

    [[nodiscard]] const char* begin() const noexcept { return data; }
    [[nodiscard]] const char* end() const noexcept { return data + length; }
    [[nodiscard]] size_t size() const noexcept { return length; }
    [[nodiscard]] bool empty() const noexcept { return length == 0; }
};

/// Thread-local pool of FIX message buffers
using MessageBufferPool = ThreadLocalPool<MessageBuffer, 128>;
using PooledMessageBuffer = PooledPtr<MessageBuffer, 128>;

// ============================================================================
// Large Buffer Pool (for resend requests, batch operations)
// ============================================================================

/// Large buffer for batch operations (64KB)
struct alignas(CACHE_LINE_SIZE) LargeBuffer {
    static constexpr size_t MAX_SIZE = 65536;

    char data[MAX_SIZE];
    size_t length{0};

    void set(const char* src, size_t len) noexcept {
        length = (len <= MAX_SIZE) ? len : MAX_SIZE;
        __builtin_memcpy(data, src, length);
    }

    void clear() noexcept {
        length = 0;
    }

    [[nodiscard]] size_t size() const noexcept { return length; }
};

/// Thread-local pool of large buffers (smaller capacity due to size)
using LargeBufferPool = ThreadLocalPool<LargeBuffer, 16>;
using PooledLargeBuffer = PooledPtr<LargeBuffer, 16>;

} // namespace nfx::util
