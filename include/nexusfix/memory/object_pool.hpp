/*
    NexusFIX Object Pool with Intrusive Free List

    Modern C++ #80: Object Pools (Free Lists)

    A fixed-size, pre-allocated object pool providing:
    - O(1) allocation and deallocation
    - Zero fragmentation
    - No syscalls on hot path
    - Cache-friendly memory layout

    Uses intrusive free list (objects contain next pointer when free).

    Use cases:
    - FIX message objects
    - Order objects
    - Session state objects
    - Any frequently allocated/deallocated objects

    Thread-safety: NOT thread-safe. Use separate pools per thread.
*/

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <new>
#include <memory>
#include <type_traits>
#include <span>

namespace nfx::memory {

// ============================================================================
// Object Pool Configuration
// ============================================================================

/// Default pool capacity
inline constexpr std::size_t DEFAULT_POOL_CAPACITY = 1024;

// ============================================================================
// Object Pool Implementation
// ============================================================================

/// Fixed-size object pool with O(1) allocation/deallocation
/// @tparam T Object type
/// @tparam Capacity Maximum number of objects
template<typename T, std::size_t Capacity = DEFAULT_POOL_CAPACITY>
class ObjectPool {
    static_assert(sizeof(T) >= sizeof(void*),
        "Object type must be at least pointer-sized for free list");
    static_assert(Capacity > 0, "Pool capacity must be positive");

    // Union for storing either object or next pointer
    union Slot {
        alignas(T) std::byte storage[sizeof(T)];
        Slot* next;

        constexpr Slot() noexcept : next{nullptr} {}
    };

public:
    using value_type = T;
    using size_type = std::size_t;

    /// Construct pool with all slots available
    ObjectPool() noexcept {
        // Build intrusive free list
        for (std::size_t i = 0; i < Capacity - 1; ++i) {
            storage_[i].next = &storage_[i + 1];
        }
        storage_[Capacity - 1].next = nullptr;
        free_list_ = &storage_[0];
        available_ = Capacity;
    }

    ~ObjectPool() {
        // Note: Does not destroy allocated objects
        // User must ensure all objects are deallocated before destruction
    }

    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;
    ObjectPool(ObjectPool&&) = delete;
    ObjectPool& operator=(ObjectPool&&) = delete;

    // ========================================================================
    // Allocation API
    // ========================================================================

    /// Allocate and construct object
    /// @return Pointer to constructed object, nullptr if pool exhausted
    template<typename... Args>
    [[nodiscard]] T* allocate(Args&&... args) noexcept(
        std::is_nothrow_constructible_v<T, Args...>)
    {
        if (!free_list_) [[unlikely]] {
            return nullptr;
        }

        Slot* slot = free_list_;
        free_list_ = slot->next;
        --available_;

        // Construct object in-place
        return std::construct_at(
            reinterpret_cast<T*>(slot->storage),
            std::forward<Args>(args)...
        );
    }

    /// Deallocate and destroy object
    void deallocate(T* ptr) noexcept {
        if (!ptr) [[unlikely]] {
            return;
        }

        // Verify pointer is within pool bounds
        auto* slot = reinterpret_cast<Slot*>(ptr);
        if (slot < &storage_[0] || slot > &storage_[Capacity - 1]) [[unlikely]] {
            return;  // Pointer not from this pool
        }

        // Destroy object
        std::destroy_at(ptr);

        // Add to free list
        slot->next = free_list_;
        free_list_ = slot;
        ++available_;
    }

    /// Allocate raw memory without construction
    /// User must call std::construct_at and std::destroy_at
    [[nodiscard]] void* allocate_raw() noexcept {
        if (!free_list_) [[unlikely]] {
            return nullptr;
        }

        Slot* slot = free_list_;
        free_list_ = slot->next;
        --available_;

        return slot->storage;
    }

    /// Deallocate raw memory (assumes already destroyed)
    void deallocate_raw(void* ptr) noexcept {
        if (!ptr) [[unlikely]] {
            return;
        }

        auto* slot = reinterpret_cast<Slot*>(ptr);
        if (slot < &storage_[0] || slot > &storage_[Capacity - 1]) [[unlikely]] {
            return;
        }

        slot->next = free_list_;
        free_list_ = slot;
        ++available_;
    }

    // ========================================================================
    // Status API
    // ========================================================================

    /// Number of available slots
    [[nodiscard]] std::size_t available() const noexcept {
        return available_;
    }

    /// Number of allocated objects
    [[nodiscard]] std::size_t allocated() const noexcept {
        return Capacity - available_;
    }

    /// Total capacity
    [[nodiscard]] static constexpr std::size_t capacity() noexcept {
        return Capacity;
    }

    /// Check if pool is empty (no available slots)
    [[nodiscard]] bool empty() const noexcept {
        return available_ == 0;
    }

    /// Check if pool is full (all slots available)
    [[nodiscard]] bool full() const noexcept {
        return available_ == Capacity;
    }

    /// Check if pointer belongs to this pool
    [[nodiscard]] bool owns(const T* ptr) const noexcept {
        auto* slot = reinterpret_cast<const Slot*>(ptr);
        return slot >= &storage_[0] && slot <= &storage_[Capacity - 1];
    }

    // ========================================================================
    // Bulk Operations
    // ========================================================================

    /// Reset pool (invalidates all pointers!)
    /// WARNING: Does not call destructors
    void reset() noexcept {
        for (std::size_t i = 0; i < Capacity - 1; ++i) {
            storage_[i].next = &storage_[i + 1];
        }
        storage_[Capacity - 1].next = nullptr;
        free_list_ = &storage_[0];
        available_ = Capacity;
    }

    /// Clear pool (calls destructors on all allocated objects)
    void clear() noexcept {
        // Iterate through all slots, destroy allocated ones
        for (std::size_t i = 0; i < Capacity; ++i) {
            // Check if slot is in free list
            bool is_free = false;
            for (Slot* p = free_list_; p != nullptr; p = p->next) {
                if (p == &storage_[i]) {
                    is_free = true;
                    break;
                }
            }
            if (!is_free) {
                std::destroy_at(reinterpret_cast<T*>(storage_[i].storage));
            }
        }
        reset();
    }

private:
    alignas(64) std::array<Slot, Capacity> storage_;
    Slot* free_list_;
    std::size_t available_;
};

// ============================================================================
// Scoped Pool Allocator
// ============================================================================

/// RAII wrapper for pool-allocated objects
template<typename T, std::size_t Capacity = DEFAULT_POOL_CAPACITY>
class PoolPtr {
public:
    using Pool = ObjectPool<T, Capacity>;

    PoolPtr() noexcept : pool_{nullptr}, ptr_{nullptr} {}

    PoolPtr(Pool& pool, T* ptr) noexcept : pool_{&pool}, ptr_{ptr} {}

    ~PoolPtr() {
        if (ptr_ && pool_) {
            pool_->deallocate(ptr_);
        }
    }

    PoolPtr(const PoolPtr&) = delete;
    PoolPtr& operator=(const PoolPtr&) = delete;

    PoolPtr(PoolPtr&& other) noexcept
        : pool_{other.pool_}, ptr_{other.ptr_} {
        other.pool_ = nullptr;
        other.ptr_ = nullptr;
    }

    PoolPtr& operator=(PoolPtr&& other) noexcept {
        if (this != &other) {
            if (ptr_ && pool_) {
                pool_->deallocate(ptr_);
            }
            pool_ = other.pool_;
            ptr_ = other.ptr_;
            other.pool_ = nullptr;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    [[nodiscard]] T* get() const noexcept { return ptr_; }
    [[nodiscard]] T* operator->() const noexcept { return ptr_; }
    [[nodiscard]] T& operator*() const noexcept { return *ptr_; }
    [[nodiscard]] explicit operator bool() const noexcept { return ptr_ != nullptr; }

    T* release() noexcept {
        T* tmp = ptr_;
        ptr_ = nullptr;
        pool_ = nullptr;
        return tmp;
    }

    void reset() noexcept {
        if (ptr_ && pool_) {
            pool_->deallocate(ptr_);
        }
        ptr_ = nullptr;
        pool_ = nullptr;
    }

private:
    Pool* pool_;
    T* ptr_;
};

/// Create pool-managed object
template<typename T, std::size_t Capacity = DEFAULT_POOL_CAPACITY, typename... Args>
[[nodiscard]] PoolPtr<T, Capacity> make_pooled(
    ObjectPool<T, Capacity>& pool, Args&&... args)
{
    T* ptr = pool.allocate(std::forward<Args>(args)...);
    return PoolPtr<T, Capacity>{pool, ptr};
}

// ============================================================================
// Growing Object Pool (Chunks)
// ============================================================================

/// Object pool that grows by allocating additional chunks
/// Thread-safety: NOT thread-safe
template<typename T, std::size_t ChunkSize = 1024>
class GrowingObjectPool {
    static_assert(sizeof(T) >= sizeof(void*),
        "Object type must be at least pointer-sized");

    union Slot {
        alignas(T) std::byte storage[sizeof(T)];
        Slot* next;
    };

    struct Chunk {
        std::array<Slot, ChunkSize> slots;
        std::unique_ptr<Chunk> next;
    };

public:
    GrowingObjectPool() noexcept = default;

    ~GrowingObjectPool() {
        // Chunks are automatically destroyed via unique_ptr chain
    }

    GrowingObjectPool(const GrowingObjectPool&) = delete;
    GrowingObjectPool& operator=(const GrowingObjectPool&) = delete;

    template<typename... Args>
    [[nodiscard]] T* allocate(Args&&... args) {
        if (!free_list_) {
            grow();
        }

        Slot* slot = free_list_;
        free_list_ = slot->next;
        ++allocated_;

        return std::construct_at(
            reinterpret_cast<T*>(slot->storage),
            std::forward<Args>(args)...
        );
    }

    void deallocate(T* ptr) noexcept {
        if (!ptr) return;

        std::destroy_at(ptr);
        auto* slot = reinterpret_cast<Slot*>(ptr);
        slot->next = free_list_;
        free_list_ = slot;
        --allocated_;
    }

    [[nodiscard]] std::size_t allocated() const noexcept { return allocated_; }
    [[nodiscard]] std::size_t chunks() const noexcept { return chunk_count_; }
    [[nodiscard]] std::size_t capacity() const noexcept { return chunk_count_ * ChunkSize; }

private:
    void grow() {
        auto chunk = std::make_unique<Chunk>();

        // Build free list within new chunk
        for (std::size_t i = 0; i < ChunkSize - 1; ++i) {
            chunk->slots[i].next = &chunk->slots[i + 1];
        }
        chunk->slots[ChunkSize - 1].next = free_list_;
        free_list_ = &chunk->slots[0];

        // Link chunk to chain
        chunk->next = std::move(head_);
        head_ = std::move(chunk);
        ++chunk_count_;
    }

    std::unique_ptr<Chunk> head_;
    Slot* free_list_{nullptr};
    std::size_t allocated_{0};
    std::size_t chunk_count_{0};
};

} // namespace nfx::memory
