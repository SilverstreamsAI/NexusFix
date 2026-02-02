/*
    NexusFIX Prefetch Utilities

    Hardware prefetching hints for reduced cache misses.
    Use before accessing data in hot loops.

    Prefetch distance should be tuned per platform:
    - Intel Xeon: 16-32 iterations
    - AMD EPYC: 8-16 iterations
    - Apple M-series: 32-64 iterations
*/

#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

namespace nfx::util {

// ============================================================================
// Cache Line Constants
// ============================================================================

/// Standard cache line size (64 bytes on x86-64)
inline constexpr size_t CACHE_LINE_SIZE = 64;

/// L1 cache size estimate (32KB typical)
inline constexpr size_t L1_CACHE_SIZE = 32 * 1024;

/// L2 cache size estimate (256KB typical)
inline constexpr size_t L2_CACHE_SIZE = 256 * 1024;

// ============================================================================
// Prefetch Locality Hints
// ============================================================================

/// Prefetch temporal locality hints
enum class PrefetchLocality {
    None = 0,    // NTA - Non-temporal, minimize cache pollution
    Low = 1,     // T2 - Low temporal locality (L3 cache)
    Medium = 2,  // T1 - Medium temporal locality (L2 cache)
    High = 3     // T0 - High temporal locality (L1 cache)
};

// ============================================================================
// Prefetch Functions
// ============================================================================

/// Prefetch for read with high locality (L1 cache)
/// Use this for data that will be read multiple times
inline void prefetch_read(const void* ptr) noexcept {
    __builtin_prefetch(ptr, 0, 3);  // read=0, locality=3 (L1)
}

/// Prefetch for read with low locality (minimize cache pollution)
/// Use for streaming data that won't be reused
inline void prefetch_read_nta(const void* ptr) noexcept {
    __builtin_prefetch(ptr, 0, 0);  // read=0, locality=0 (NTA)
}

/// Prefetch for write with high locality (L1 cache)
/// Use when you know you'll write to this address soon
inline void prefetch_write(void* ptr) noexcept {
    __builtin_prefetch(ptr, 1, 3);  // write=1, locality=3 (L1)
}

/// Prefetch for write with low locality
inline void prefetch_write_nta(void* ptr) noexcept {
    __builtin_prefetch(ptr, 1, 0);  // write=1, locality=0 (NTA)
}

/// Prefetch with configurable locality
template<PrefetchLocality Locality = PrefetchLocality::High>
inline void prefetch(const void* ptr, bool for_write = false) noexcept {
    __builtin_prefetch(ptr, for_write ? 1 : 0, std::to_underlying(Locality));
}

// ============================================================================
// Batch Prefetch Functions
// ============================================================================

/// Prefetch N cache lines starting from ptr
template<size_t NumCacheLines = 8>
inline void prefetch_range(const void* ptr) noexcept {
    const char* p = static_cast<const char*>(ptr);
    for (size_t i = 0; i < NumCacheLines; ++i) {
        prefetch_read(p + i * CACHE_LINE_SIZE);
    }
}

/// Prefetch ahead by specified number of bytes
inline void prefetch_ahead(const void* base, size_t byte_offset) noexcept {
    prefetch_read(static_cast<const char*>(base) + byte_offset);
}

/// Prefetch ahead by specified number of cache lines
inline void prefetch_ahead_lines(const void* base, size_t line_offset) noexcept {
    prefetch_read(static_cast<const char*>(base) + line_offset * CACHE_LINE_SIZE);
}

// ============================================================================
// Loop Prefetch Helpers
// ============================================================================

/// Default prefetch distance (cache lines ahead)
inline constexpr size_t DEFAULT_PREFETCH_DISTANCE = 8;

/// Prefetch helper for array iteration
/// Call at the start of each loop iteration
template<typename T, size_t Distance = DEFAULT_PREFETCH_DISTANCE>
inline void prefetch_for_iteration(const T* array, size_t current_index, size_t array_size) noexcept {
    size_t prefetch_index = current_index + Distance;
    if (prefetch_index < array_size) {
        prefetch_read(&array[prefetch_index]);
    }
}

/// Prefetch helper that accounts for element size
template<typename T, size_t DistanceBytes = DEFAULT_PREFETCH_DISTANCE * CACHE_LINE_SIZE>
inline void prefetch_elements_ahead(const T* ptr) noexcept {
    constexpr size_t elements_ahead = DistanceBytes / sizeof(T);
    prefetch_read(ptr + elements_ahead);
}

// ============================================================================
// Software Prefetch Barrier
// ============================================================================

/// Memory fence to ensure prefetches are issued
inline void prefetch_fence() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
    asm volatile("" ::: "memory");
#else
    __sync_synchronize();
#endif
}

// ============================================================================
// Cache Line Utilities
// ============================================================================

/// Round up size to cache line boundary
[[nodiscard]] inline constexpr size_t align_to_cache_line(size_t size) noexcept {
    return (size + CACHE_LINE_SIZE - 1) & ~(CACHE_LINE_SIZE - 1);
}

/// Check if pointer is cache-line aligned
[[nodiscard]] inline bool is_cache_aligned(const void* ptr) noexcept {
    return (reinterpret_cast<uintptr_t>(ptr) & (CACHE_LINE_SIZE - 1)) == 0;
}

/// Get cache line offset of pointer
[[nodiscard]] inline size_t cache_line_offset(const void* ptr) noexcept {
    return reinterpret_cast<uintptr_t>(ptr) & (CACHE_LINE_SIZE - 1);
}

} // namespace nfx::util
