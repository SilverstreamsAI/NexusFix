#pragma once

#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <span>
#include <array>
#include <new>
#include <bit>

namespace nfx {

// ============================================================================
// Cache Line Constants
// ============================================================================

// Use fixed 64-byte cache line size for ABI stability
// std::hardware_destructive_interference_size is not used due to GCC warning
// about ABI instability. 64 bytes is the correct value for x86-64.
inline constexpr size_t CACHE_LINE_SIZE = 64;

// ============================================================================
// Aligned Buffer for Message Data
// ============================================================================

/// Cache-line aligned buffer for zero-copy message storage
template <size_t N>
struct alignas(CACHE_LINE_SIZE) AlignedBuffer {
    std::array<char, N> data;

    [[nodiscard]] constexpr char* begin() noexcept { return data.data(); }
    [[nodiscard]] constexpr const char* begin() const noexcept { return data.data(); }
    [[nodiscard]] constexpr char* end() noexcept { return data.data() + N; }
    [[nodiscard]] constexpr const char* end() const noexcept { return data.data() + N; }
    [[nodiscard]] constexpr size_t size() const noexcept { return N; }

    [[nodiscard]] constexpr std::span<char> as_span() noexcept {
        return std::span<char>{data};
    }

    [[nodiscard]] constexpr std::span<const char> as_span() const noexcept {
        return std::span<const char>{data};
    }
};

// Common buffer sizes for FIX messages
using SmallBuffer = AlignedBuffer<256>;    // Simple messages (Heartbeat, Logon)
using MediumBuffer = AlignedBuffer<1024>;  // Standard messages (ExecutionReport)
using LargeBuffer = AlignedBuffer<4096>;   // Large messages (with repeating groups)
using JumboBuffer = AlignedBuffer<65536>;  // Maximum FIX message size

// ============================================================================
// Fixed-Size Pool Allocator (Lock-free, O(1) allocation)
// ============================================================================

/// Pool of fixed-size blocks with O(1) allocation (no syscalls on hot path)
template <size_t BlockSize, size_t NumBlocks>
class alignas(CACHE_LINE_SIZE) FixedPool {
public:
    static_assert(BlockSize >= sizeof(void*), "Block must hold a pointer");
    static_assert(std::has_single_bit(BlockSize), "Block size should be power of 2");

    FixedPool() noexcept {
        // Initialize free list
        for (size_t i = 0; i < NumBlocks - 1; ++i) {
            *reinterpret_cast<void**>(&storage_[i * BlockSize]) =
                &storage_[(i + 1) * BlockSize];
        }
        *reinterpret_cast<void**>(&storage_[(NumBlocks - 1) * BlockSize]) = nullptr;
        free_head_ = storage_.data();
    }

    // Non-copyable, non-movable (owns fixed storage)
    FixedPool(const FixedPool&) = delete;
    FixedPool& operator=(const FixedPool&) = delete;
    FixedPool(FixedPool&&) = delete;
    FixedPool& operator=(FixedPool&&) = delete;

    /// Allocate a block (O(1), no syscall)
    [[nodiscard]] void* allocate() noexcept {
        if (free_head_ == nullptr) {
            return nullptr;  // Pool exhausted
        }
        void* block = free_head_;
        free_head_ = *reinterpret_cast<void**>(free_head_);
        ++allocated_count_;
        return block;
    }

    /// Deallocate a block (O(1), no syscall)
    void deallocate(void* ptr) noexcept {
        if (ptr == nullptr) return;
        *reinterpret_cast<void**>(ptr) = free_head_;
        free_head_ = ptr;
        --allocated_count_;
    }

    /// Check if pointer belongs to this pool
    [[nodiscard]] bool owns(const void* ptr) const noexcept {
        const char* p = static_cast<const char*>(ptr);
        return p >= storage_.data() && p < storage_.data() + storage_.size();
    }

    [[nodiscard]] size_t allocated() const noexcept { return allocated_count_; }
    [[nodiscard]] size_t available() const noexcept { return NumBlocks - allocated_count_; }
    [[nodiscard]] static constexpr size_t block_size() noexcept { return BlockSize; }
    [[nodiscard]] static constexpr size_t capacity() noexcept { return NumBlocks; }

private:
    alignas(CACHE_LINE_SIZE) std::array<char, BlockSize * NumBlocks> storage_{};
    void* free_head_{nullptr};
    size_t allocated_count_{0};
};

// ============================================================================
// PMR Memory Resource Wrapper
// ============================================================================

/// Monotonic buffer resource with pre-allocated backing storage
template <size_t Size>
class MonotonicPool : public std::pmr::memory_resource {
public:
    MonotonicPool() noexcept
        : upstream_{std::pmr::null_memory_resource()}
        , resource_{buffer_.data(), buffer_.size(), upstream_} {}

    /// Reset the pool (invalidates all allocations)
    void reset() noexcept {
        resource_.release();
    }

    /// Get the PMR allocator
    [[nodiscard]] std::pmr::polymorphic_allocator<char> allocator() noexcept {
        return std::pmr::polymorphic_allocator<char>{&resource_};
    }

private:
    void* do_allocate(size_t bytes, size_t alignment) override {
        return resource_.allocate(bytes, alignment);
    }

    void do_deallocate(void* p, size_t bytes, size_t alignment) override {
        // Monotonic buffer doesn't deallocate individual blocks
    }

    bool do_is_equal(const memory_resource& other) const noexcept override {
        return this == &other;
    }

    alignas(CACHE_LINE_SIZE) std::array<char, Size> buffer_{};
    std::pmr::memory_resource* upstream_;
    std::pmr::monotonic_buffer_resource resource_;
};

// ============================================================================
// Message Buffer Pool (Tiered allocation)
// ============================================================================

/// Tiered pool for FIX messages of various sizes
class MessagePool {
public:
    static constexpr size_t SMALL_SIZE = 256;
    static constexpr size_t MEDIUM_SIZE = 1024;
    static constexpr size_t LARGE_SIZE = 4096;

    static constexpr size_t SMALL_COUNT = 1024;
    static constexpr size_t MEDIUM_COUNT = 256;
    static constexpr size_t LARGE_COUNT = 64;

    MessagePool() = default;

    // Non-copyable, non-movable
    MessagePool(const MessagePool&) = delete;
    MessagePool& operator=(const MessagePool&) = delete;

    /// Allocate buffer of at least `size` bytes
    [[nodiscard]] std::span<char> allocate(size_t size) noexcept {
        if (size <= SMALL_SIZE) {
            if (void* p = small_pool_.allocate()) {
                return std::span<char>{static_cast<char*>(p), SMALL_SIZE};
            }
        }
        if (size <= MEDIUM_SIZE) {
            if (void* p = medium_pool_.allocate()) {
                return std::span<char>{static_cast<char*>(p), MEDIUM_SIZE};
            }
        }
        if (size <= LARGE_SIZE) {
            if (void* p = large_pool_.allocate()) {
                return std::span<char>{static_cast<char*>(p), LARGE_SIZE};
            }
        }
        // Pool exhausted or size too large
        return {};
    }

    /// Deallocate buffer
    void deallocate(std::span<char> buffer) noexcept {
        void* ptr = buffer.data();
        if (small_pool_.owns(ptr)) {
            small_pool_.deallocate(ptr);
        } else if (medium_pool_.owns(ptr)) {
            medium_pool_.deallocate(ptr);
        } else if (large_pool_.owns(ptr)) {
            large_pool_.deallocate(ptr);
        }
    }

    struct Stats {
        size_t small_allocated;
        size_t small_available;
        size_t medium_allocated;
        size_t medium_available;
        size_t large_allocated;
        size_t large_available;
    };

    [[nodiscard]] Stats stats() const noexcept {
        return Stats{
            .small_allocated = small_pool_.allocated(),
            .small_available = small_pool_.available(),
            .medium_allocated = medium_pool_.allocated(),
            .medium_available = medium_pool_.available(),
            .large_allocated = large_pool_.allocated(),
            .large_available = large_pool_.available()
        };
    }

private:
    FixedPool<SMALL_SIZE, SMALL_COUNT> small_pool_;
    FixedPool<MEDIUM_SIZE, MEDIUM_COUNT> medium_pool_;
    FixedPool<LARGE_SIZE, LARGE_COUNT> large_pool_;
};

// ============================================================================
// RAII Buffer Handle
// ============================================================================

/// RAII wrapper for pooled buffer (returns to pool on destruction)
class PooledBuffer {
public:
    PooledBuffer() noexcept : pool_{nullptr}, buffer_{} {}

    PooledBuffer(MessagePool& pool, std::span<char> buf) noexcept
        : pool_{&pool}, buffer_{buf} {}

    ~PooledBuffer() {
        if (pool_ && !buffer_.empty()) {
            pool_->deallocate(buffer_);
        }
    }

    // Move-only
    PooledBuffer(const PooledBuffer&) = delete;
    PooledBuffer& operator=(const PooledBuffer&) = delete;

    PooledBuffer(PooledBuffer&& other) noexcept
        : pool_{other.pool_}, buffer_{other.buffer_} {
        other.pool_ = nullptr;
        other.buffer_ = {};
    }

    PooledBuffer& operator=(PooledBuffer&& other) noexcept {
        if (this != &other) {
            if (pool_ && !buffer_.empty()) {
                pool_->deallocate(buffer_);
            }
            pool_ = other.pool_;
            buffer_ = other.buffer_;
            other.pool_ = nullptr;
            other.buffer_ = {};
        }
        return *this;
    }

    [[nodiscard]] std::span<char> get() noexcept { return buffer_; }
    [[nodiscard]] std::span<const char> get() const noexcept { return buffer_; }
    [[nodiscard]] char* data() noexcept { return buffer_.data(); }
    [[nodiscard]] const char* data() const noexcept { return buffer_.data(); }
    [[nodiscard]] size_t size() const noexcept { return buffer_.size(); }
    [[nodiscard]] bool empty() const noexcept { return buffer_.empty(); }

    [[nodiscard]] explicit operator bool() const noexcept { return !empty(); }

private:
    MessagePool* pool_;
    std::span<char> buffer_;
};

} // namespace nfx
