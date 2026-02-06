#pragma once

// MimallocMemoryResource - std::pmr::memory_resource wrapping mimalloc per-session heaps
//
// Provides O(1) heap destruction for session cleanup, eliminating the need to
// individually free every allocation. Each session gets its own mi_heap_t,
// and destroying the heap releases all memory in a single operation.
//
// Requires: NFX_ENABLE_MIMALLOC=ON in CMake (defines NFX_HAS_MIMALLOC=1)

#if defined(NFX_HAS_MIMALLOC) && NFX_HAS_MIMALLOC

#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <mimalloc.h>

#include "nexusfix/memory/buffer_pool.hpp"  // CACHE_LINE_SIZE

namespace nfx::memory {

// ============================================================================
// Heap Statistics
// ============================================================================

/// Statistics for a MimallocMemoryResource heap
struct MimallocHeapStats {
    size_t bytes_allocated{0};
    size_t allocation_count{0};
    size_t peak_bytes{0};
};

// ============================================================================
// MimallocMemoryResource - PMR wrapper for mimalloc per-session heaps
// ============================================================================

/// std::pmr::memory_resource backed by a dedicated mimalloc heap.
/// Each instance owns a mi_heap_t* created via mi_heap_new().
/// Destruction calls mi_heap_destroy() for O(1) cleanup of all allocations.
///
/// Usage:
///   MimallocMemoryResource resource;
///   std::pmr::polymorphic_allocator<char> alloc(&resource);
///   auto* ptr = alloc.allocate(256);
///   // ... use ptr ...
///   // All memory freed when resource goes out of scope
class MimallocMemoryResource : public std::pmr::memory_resource {
public:
    MimallocMemoryResource() noexcept
        : heap_{mi_heap_new()} {}

    ~MimallocMemoryResource() override {
        if (heap_) {
            mi_heap_destroy(heap_);
        }
    }

    // Non-copyable, non-movable (owns heap)
    MimallocMemoryResource(const MimallocMemoryResource&) = delete;
    MimallocMemoryResource& operator=(const MimallocMemoryResource&) = delete;
    MimallocMemoryResource(MimallocMemoryResource&&) = delete;
    MimallocMemoryResource& operator=(MimallocMemoryResource&&) = delete;

    /// Get the PMR allocator for this heap
    [[nodiscard]] std::pmr::polymorphic_allocator<char> allocator() noexcept {
        return std::pmr::polymorphic_allocator<char>{this};
    }

    /// Get current heap statistics
    [[nodiscard]] MimallocHeapStats stats() const noexcept {
        return MimallocHeapStats{
            .bytes_allocated = bytes_allocated_,
            .allocation_count = allocation_count_,
            .peak_bytes = peak_bytes_
        };
    }

    /// Get total bytes currently allocated
    [[nodiscard]] size_t bytes_allocated() const noexcept { return bytes_allocated_; }

    /// Get total number of active allocations
    [[nodiscard]] size_t allocation_count() const noexcept { return allocation_count_; }

    /// Get peak bytes allocated
    [[nodiscard]] size_t peak_bytes() const noexcept { return peak_bytes_; }

    /// Check if the heap was created successfully
    [[nodiscard]] bool valid() const noexcept { return heap_ != nullptr; }

private:
    void* do_allocate(size_t bytes, size_t alignment) override {
        void* ptr = mi_heap_malloc_aligned(heap_, bytes, alignment);
        if (ptr) [[likely]] {
            bytes_allocated_ += bytes;
            ++allocation_count_;
            if (bytes_allocated_ > peak_bytes_) {
                peak_bytes_ = bytes_allocated_;
            }
        }
        return ptr;
    }

    void do_deallocate(void* p, size_t bytes,
                       [[maybe_unused]] size_t alignment) override {
        if (p) [[likely]] {
            mi_free(p);
            bytes_allocated_ -= bytes;
            --allocation_count_;
        }
    }

    bool do_is_equal(const memory_resource& other) const noexcept override {
        return this == &other;
    }

    mi_heap_t* heap_;
    alignas(CACHE_LINE_SIZE) size_t bytes_allocated_{0};
    size_t allocation_count_{0};
    size_t peak_bytes_{0};
};

// ============================================================================
// SessionHeap - Combined per-session monotonic + mimalloc architecture
// ============================================================================

/// Combined per-session heap: monotonic bump allocation + mimalloc upstream.
/// Hot path allocations use monotonic bump (~10ns). When the initial buffer
/// is exhausted, overflow allocates from the mimalloc heap (~13ns) instead
/// of failing. Session destruction calls mi_heap_destroy() for O(1) cleanup.
///
/// Destruction order guarantees safety:
///   1. pool_ destroyed first (frees overflow chunks via heap_)
///   2. heap_ destroyed last (mi_heap_destroy releases initial_buffer_ + remainder)
///
/// Usage:
///   SessionHeap session(64 * 1024 * 1024);  // 64MB initial buffer
///   auto alloc = session.allocator();
///   auto* ptr = alloc.allocate(256);        // Monotonic bump (~10ns)
///   // ... use ptr ...
///   // All memory freed when session goes out of scope
class SessionHeap : public std::pmr::memory_resource {
public:
    static constexpr size_t DEFAULT_INITIAL_SIZE = 64 * 1024 * 1024;  // 64MB

    explicit SessionHeap(size_t initial_buffer_size = DEFAULT_INITIAL_SIZE) noexcept
        : heap_{}
        , initial_buffer_(static_cast<char*>(
              heap_.allocate(initial_buffer_size, CACHE_LINE_SIZE)))
        , initial_buffer_size_(initial_buffer_size)
        , pool_(initial_buffer_, initial_buffer_size_, &heap_) {}

    // Non-copyable, non-movable
    SessionHeap(const SessionHeap&) = delete;
    SessionHeap& operator=(const SessionHeap&) = delete;
    SessionHeap(SessionHeap&&) = delete;
    SessionHeap& operator=(SessionHeap&&) = delete;

    // Destructor order: pool_ first (frees overflow chunks via heap_),
    // then heap_ (mi_heap_destroy releases initial_buffer_ + remainder)

    /// Reset the monotonic pool (reuse initial buffer, keep heap alive)
    void reset() noexcept { pool_.release(); }

    /// Get allocator for this session heap
    [[nodiscard]] std::pmr::polymorphic_allocator<char> allocator() noexcept {
        return std::pmr::polymorphic_allocator<char>{this};
    }

    /// Access the underlying mimalloc heap stats
    [[nodiscard]] MimallocHeapStats stats() const noexcept { return heap_.stats(); }

    /// Check if the heap was created successfully
    [[nodiscard]] bool valid() const noexcept {
        return heap_.valid() && initial_buffer_ != nullptr;
    }

    /// Get initial buffer size
    [[nodiscard]] size_t initial_buffer_size() const noexcept {
        return initial_buffer_size_;
    }

private:
    void* do_allocate(size_t bytes, size_t alignment) override {
        return pool_.allocate(bytes, alignment);
    }

    void do_deallocate([[maybe_unused]] void* p,
                       [[maybe_unused]] size_t bytes,
                       [[maybe_unused]] size_t alignment) override {
        // Monotonic doesn't reclaim individual allocations.
        // Memory is reclaimed on reset() or destruction.
    }

    bool do_is_equal(const memory_resource& other) const noexcept override {
        return this == &other;
    }

    // Declaration order matters for destruction:
    // heap_ constructed first, destroyed last (releases everything)
    // pool_ constructed last, destroyed first (releases overflow chunks)
    MimallocMemoryResource heap_;
    char* initial_buffer_;
    size_t initial_buffer_size_;
    std::pmr::monotonic_buffer_resource pool_;
};

} // namespace nfx::memory

#endif // NFX_HAS_MIMALLOC
