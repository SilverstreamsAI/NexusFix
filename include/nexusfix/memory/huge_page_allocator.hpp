/*
    NexusFIX Huge Page Allocator

    Uses Linux huge pages (2MB/1GB) to reduce TLB misses.
    Critical for low-latency performance with large data structures.

    Requirements:
    - Linux with huge pages enabled
    - Sufficient huge pages reserved: echo 512 > /proc/sys/vm/nr_hugepages
    - Or use Transparent Huge Pages (THP)
*/

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>

#ifdef __linux__
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace nfx::memory {

// ============================================================================
// Huge Page Configuration
// ============================================================================

/// Huge page size options
enum class HugePageSize : size_t {
    Standard = 4096,           // 4KB standard page
    Huge2MB  = 2 * 1024 * 1024, // 2MB huge page
    Huge1GB  = 1024 * 1024 * 1024 // 1GB huge page (requires specific support)
};

// ============================================================================
// Huge Page Allocator
// ============================================================================

/// Allocator that uses huge pages when available
class HugePageAllocator {
public:
    /// Allocate memory with huge pages
    /// Falls back to standard aligned allocation if huge pages unavailable
    [[nodiscard]] static void* allocate(size_t size,
                                        HugePageSize page_size = HugePageSize::Huge2MB) noexcept {
#ifdef __linux__
        int flags = MAP_PRIVATE | MAP_ANONYMOUS;

        // Try huge pages first
        if (page_size == HugePageSize::Huge2MB) {
            flags |= MAP_HUGETLB;
#ifdef MAP_HUGE_2MB
            flags |= MAP_HUGE_2MB;
#endif
        } else if (page_size == HugePageSize::Huge1GB) {
            flags |= MAP_HUGETLB;
#ifdef MAP_HUGE_1GB
            flags |= MAP_HUGE_1GB;
#endif
        }

        // Round up size to page boundary
        size_t aligned_size = align_to_page(size, static_cast<size_t>(page_size));

        void* ptr = mmap(nullptr, aligned_size,
                        PROT_READ | PROT_WRITE,
                        flags, -1, 0);

        if (ptr != MAP_FAILED) {
            return ptr;
        }

        // Fall back to standard allocation with alignment
        return allocate_fallback(size, static_cast<size_t>(HugePageSize::Standard));
#else
        // Non-Linux: use standard aligned allocation
        return allocate_fallback(size, 4096);
#endif
    }

    /// Deallocate memory
    static void deallocate(void* ptr, size_t size,
                          HugePageSize page_size = HugePageSize::Huge2MB) noexcept {
        if (!ptr) return;

#ifdef __linux__
        size_t aligned_size = align_to_page(size, static_cast<size_t>(page_size));
        munmap(ptr, aligned_size);
#else
        std::free(ptr);
#endif
    }

    /// Check if huge pages are available on this system
    [[nodiscard]] static bool is_available() noexcept {
#ifdef __linux__
        // Try to allocate a single huge page
        void* ptr = mmap(nullptr, 2 * 1024 * 1024,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
                        -1, 0);
        if (ptr != MAP_FAILED) {
            munmap(ptr, 2 * 1024 * 1024);
            return true;
        }
        return false;
#else
        return false;
#endif
    }

    /// Get the number of free huge pages
    [[nodiscard]] static size_t free_huge_pages() noexcept {
#ifdef __linux__
        // Read from /proc/meminfo
        // This is simplified - production code should parse the file
        return 0; // TODO: implement proper parsing
#else
        return 0;
#endif
    }

private:
    static size_t align_to_page(size_t size, size_t page_size) noexcept {
        return (size + page_size - 1) & ~(page_size - 1);
    }

    [[nodiscard]] static void* allocate_fallback(size_t size, size_t alignment) noexcept {
        return std::aligned_alloc(alignment, align_to_page(size, alignment));
    }
};

// ============================================================================
// Huge Page STL Allocator Adapter
// ============================================================================

/// STL-compatible allocator using huge pages
template<typename T>
class HugePageStlAllocator {
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using propagate_on_container_move_assignment = std::true_type;
    using is_always_equal = std::true_type;

    HugePageStlAllocator() noexcept = default;

    template<typename U>
    HugePageStlAllocator(const HugePageStlAllocator<U>&) noexcept {}

    [[nodiscard]] T* allocate(std::size_t n) {
        if (n > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw std::bad_array_new_length();
        }

        void* ptr = HugePageAllocator::allocate(n * sizeof(T));
        if (!ptr) {
            throw std::bad_alloc();
        }
        return static_cast<T*>(ptr);
    }

    void deallocate(T* ptr, std::size_t n) noexcept {
        HugePageAllocator::deallocate(ptr, n * sizeof(T));
    }

    template<typename U>
    bool operator==(const HugePageStlAllocator<U>&) const noexcept {
        return true;
    }

    template<typename U>
    bool operator!=(const HugePageStlAllocator<U>&) const noexcept {
        return false;
    }
};

// ============================================================================
// RAII Huge Page Buffer
// ============================================================================

/// RAII wrapper for huge page allocations
class HugePageBuffer {
public:
    explicit HugePageBuffer(size_t size,
                           HugePageSize page_size = HugePageSize::Huge2MB)
        : size_(size)
        , page_size_(page_size)
        , data_(HugePageAllocator::allocate(size, page_size)) {}

    ~HugePageBuffer() {
        if (data_) {
            HugePageAllocator::deallocate(data_, size_, page_size_);
        }
    }

    // Non-copyable
    HugePageBuffer(const HugePageBuffer&) = delete;
    HugePageBuffer& operator=(const HugePageBuffer&) = delete;

    // Movable
    HugePageBuffer(HugePageBuffer&& other) noexcept
        : size_(other.size_)
        , page_size_(other.page_size_)
        , data_(other.data_) {
        other.data_ = nullptr;
        other.size_ = 0;
    }

    HugePageBuffer& operator=(HugePageBuffer&& other) noexcept {
        if (this != &other) {
            if (data_) {
                HugePageAllocator::deallocate(data_, size_, page_size_);
            }
            size_ = other.size_;
            page_size_ = other.page_size_;
            data_ = other.data_;
            other.data_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    [[nodiscard]] void* data() noexcept { return data_; }
    [[nodiscard]] const void* data() const noexcept { return data_; }
    [[nodiscard]] size_t size() const noexcept { return size_; }
    [[nodiscard]] bool valid() const noexcept { return data_ != nullptr; }

    template<typename T>
    [[nodiscard]] T* as() noexcept { return static_cast<T*>(data_); }

    template<typename T>
    [[nodiscard]] const T* as() const noexcept { return static_cast<const T*>(data_); }

private:
    size_t size_;
    HugePageSize page_size_;
    void* data_;
};

} // namespace nfx::memory
