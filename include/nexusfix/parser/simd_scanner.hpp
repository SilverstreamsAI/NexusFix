#pragma once

// Suppress MSVC warning C4324: structure was padded due to alignment specifier
// This is expected behavior for cache-line aligned structures
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324)
#endif

#include <span>
#include <cstdint>
#include <cstddef>
#include <array>
#include <memory>  // std::assume_aligned

#include "nexusfix/platform/platform.hpp"
#include "nexusfix/util/compiler.hpp"
#include "nexusfix/interfaces/i_message.hpp"
#include "nexusfix/memory/buffer_pool.hpp"  // For nfx::CACHE_LINE_SIZE

// SIMD feature detection
#if defined(NFX_HAS_SIMD) && NFX_HAS_SIMD
    #include <immintrin.h>
    #define NFX_SIMD_AVAILABLE 1

    // AVX-512 requires both F (foundation) and BW (byte/word) extensions
    #if defined(__AVX512F__) && defined(__AVX512BW__)
        #define NFX_AVX512_AVAILABLE 1
    #else
        #define NFX_AVX512_AVAILABLE 0
    #endif
#else
    #define NFX_SIMD_AVAILABLE 0
    #define NFX_AVX512_AVAILABLE 0
#endif

namespace nfx::simd {

// ============================================================================
// Constants
// ============================================================================

inline constexpr size_t AVX2_REGISTER_SIZE = 32;   // 256 bits = 32 bytes
inline constexpr size_t AVX512_REGISTER_SIZE = 64; // 512 bits = 64 bytes
inline constexpr size_t MAX_SOH_POSITIONS = 256;   // Max fields in typical message

// Use global cache line size from nfx namespace
using nfx::CACHE_LINE_SIZE;

// ============================================================================
// SOH Position Result
// ============================================================================

/// Result of SOH scanning - positions of all field delimiters
/// Aligned to cache line to prevent false sharing in multi-threaded scenarios
struct alignas(CACHE_LINE_SIZE) SohPositions {
    std::array<uint16_t, MAX_SOH_POSITIONS> positions;
    size_t count;

    constexpr SohPositions() noexcept : positions{}, count{0} {}

    [[nodiscard]] constexpr bool empty() const noexcept { return count == 0; }
    [[nodiscard]] constexpr size_t size() const noexcept { return count; }

    [[nodiscard]] constexpr uint16_t operator[](size_t i) const noexcept {
        return positions[i];
    }

    constexpr void push(uint16_t pos) noexcept {
        if (count < MAX_SOH_POSITIONS) [[likely]] {
            positions[count++] = pos;
        }
    }

    constexpr void clear() noexcept { count = 0; }
};

// ============================================================================
// Scalar Scanner (Fallback)
// ============================================================================

/// Scalar SOH scanner for non-SIMD platforms or small buffers
[[nodiscard]] NFX_HOT
inline SohPositions scan_soh_scalar(std::span<const char> data) noexcept {
    SohPositions result;

    for (size_t i = 0; i < data.size() && result.count < MAX_SOH_POSITIONS; ++i) {
        if (data[i] == fix::SOH) [[unlikely]] {
            result.push(static_cast<uint16_t>(i));
        }
    }

    return result;
}

/// Find next SOH position starting from offset (scalar)
[[nodiscard]] NFX_HOT
inline size_t find_soh_scalar(
    std::span<const char> data,
    size_t start = 0) noexcept
{
    for (size_t i = start; i < data.size(); ++i) {
        if (data[i] == fix::SOH) [[unlikely]] {
            return i;
        }
    }
    return data.size();  // Not found
}

/// Find '=' position starting from offset (scalar)
[[nodiscard]] NFX_HOT
inline size_t find_equals_scalar(
    std::span<const char> data,
    size_t start = 0) noexcept
{
    for (size_t i = start; i < data.size(); ++i) {
        if (data[i] == fix::EQUALS) [[unlikely]] {
            return i;
        }
    }
    return data.size();  // Not found
}

// ============================================================================
// AVX2 SIMD Scanner
// ============================================================================

#if NFX_SIMD_AVAILABLE

/// AVX2-accelerated SOH scanner (processes 32 bytes at a time)
[[nodiscard]] NFX_HOT
inline SohPositions scan_soh_avx2(std::span<const char> data) noexcept {
    SohPositions result;

    const __m256i soh_vec = _mm256_set1_epi8(fix::SOH);
    const size_t simd_end = data.size() & ~(AVX2_REGISTER_SIZE - 1);
    const char* __restrict ptr = data.data();

    // Process 32-byte chunks with AVX2
    for (size_t i = 0; i < simd_end && result.count < MAX_SOH_POSITIONS - 32; i += AVX2_REGISTER_SIZE) {
        // Load 32 bytes (unaligned load is fine on modern CPUs)
        __m256i chunk = _mm256_loadu_si256(
            reinterpret_cast<const __m256i*>(ptr + i));

        // Compare with SOH
        __m256i cmp = _mm256_cmpeq_epi8(chunk, soh_vec);

        // Get bitmask of matches
        uint32_t mask = static_cast<uint32_t>(_mm256_movemask_epi8(cmp));

        // Extract positions from bitmask (branch-free loop with early exit)
        while (mask != 0) [[likely]] {
            int bit = __builtin_ctz(mask);  // Find lowest set bit
            result.push(static_cast<uint16_t>(i + bit));
            mask &= mask - 1;  // Clear lowest set bit (branch-free)
        }
    }

    // Handle remaining bytes with scalar code
    for (size_t i = simd_end; i < data.size() && result.count < MAX_SOH_POSITIONS; ++i) {
        if (ptr[i] == fix::SOH) [[unlikely]] {
            result.push(static_cast<uint16_t>(i));
        }
    }

    return result;
}

/// AVX2-accelerated find next SOH
[[nodiscard]] NFX_HOT
inline size_t find_soh_avx2(
    std::span<const char> data,
    size_t start = 0) noexcept
{
    if (start >= data.size()) [[unlikely]] return data.size();

    const __m256i soh_vec = _mm256_set1_epi8(fix::SOH);
    const char* __restrict ptr = data.data();
    size_t i = start;

    // Align to 32-byte boundary for optimal performance
    while (i < data.size() && (reinterpret_cast<uintptr_t>(ptr + i) & 31) != 0) {
        if (ptr[i] == fix::SOH) [[unlikely]] return i;
        ++i;
    }

    // SIMD loop with alignment hint for compiler optimization
    const size_t simd_end = data.size() & ~(AVX2_REGISTER_SIZE - 1);
    while (i < simd_end) [[likely]] {
        // Hint to compiler that pointer is 32-byte aligned
        const char* aligned_ptr = std::assume_aligned<AVX2_REGISTER_SIZE>(ptr + i);
        __m256i chunk = _mm256_load_si256(
            reinterpret_cast<const __m256i*>(aligned_ptr));
        __m256i cmp = _mm256_cmpeq_epi8(chunk, soh_vec);
        uint32_t mask = static_cast<uint32_t>(_mm256_movemask_epi8(cmp));

        if (mask != 0) [[unlikely]] {
            return i + __builtin_ctz(mask);
        }
        i += AVX2_REGISTER_SIZE;
    }

    // Scalar tail
    while (i < data.size()) {
        if (ptr[i] == fix::SOH) [[unlikely]] return i;
        ++i;
    }

    return data.size();
}

/// AVX2-accelerated find '='
[[nodiscard]] NFX_HOT
inline size_t find_equals_avx2(
    std::span<const char> data,
    size_t start = 0) noexcept
{
    if (start >= data.size()) [[unlikely]] return data.size();

    const __m256i eq_vec = _mm256_set1_epi8(fix::EQUALS);
    const char* __restrict ptr = data.data();
    size_t i = start;

    // Handle unaligned prefix
    while (i < data.size() && (reinterpret_cast<uintptr_t>(ptr + i) & 31) != 0) {
        if (ptr[i] == fix::EQUALS) [[unlikely]] return i;
        ++i;
    }

    // SIMD loop with alignment hint for compiler optimization
    const size_t simd_end = data.size() & ~(AVX2_REGISTER_SIZE - 1);
    while (i < simd_end) [[likely]] {
        // Hint to compiler that pointer is 32-byte aligned
        const char* aligned_ptr = std::assume_aligned<AVX2_REGISTER_SIZE>(ptr + i);
        __m256i chunk = _mm256_load_si256(
            reinterpret_cast<const __m256i*>(aligned_ptr));
        __m256i cmp = _mm256_cmpeq_epi8(chunk, eq_vec);
        uint32_t mask = static_cast<uint32_t>(_mm256_movemask_epi8(cmp));

        if (mask != 0) [[unlikely]] {
            return i + __builtin_ctz(mask);
        }
        i += AVX2_REGISTER_SIZE;
    }

    // Scalar tail
    while (i < data.size()) {
        if (ptr[i] == fix::EQUALS) [[unlikely]] return i;
        ++i;
    }

    return data.size();
}

/// Count SOH occurrences using AVX2 (for message boundary detection)
[[nodiscard]] NFX_HOT
inline size_t count_soh_avx2(std::span<const char> data) noexcept {
    const __m256i soh_vec = _mm256_set1_epi8(fix::SOH);
    const size_t simd_end = data.size() & ~(AVX2_REGISTER_SIZE - 1);
    const char* __restrict ptr = data.data();

    size_t count = 0;

    for (size_t i = 0; i < simd_end; i += AVX2_REGISTER_SIZE) {
        __m256i chunk = _mm256_loadu_si256(
            reinterpret_cast<const __m256i*>(ptr + i));
        __m256i cmp = _mm256_cmpeq_epi8(chunk, soh_vec);
        uint32_t mask = static_cast<uint32_t>(_mm256_movemask_epi8(cmp));
        count += __builtin_popcount(mask);
    }

    // Scalar tail
    for (size_t i = simd_end; i < data.size(); ++i) {
        if (ptr[i] == fix::SOH) [[unlikely]] ++count;
    }

    return count;
}

#endif  // NFX_SIMD_AVAILABLE

// ============================================================================
// AVX-512 SIMD Scanner (2x throughput vs AVX2)
// ============================================================================

#if NFX_AVX512_AVAILABLE

/// AVX-512 accelerated SOH scanner (processes 64 bytes at a time)
/// Provides ~2x throughput improvement over AVX2 for large buffers
[[nodiscard]] NFX_HOT
inline SohPositions scan_soh_avx512(std::span<const char> data) noexcept {
    SohPositions result;

    const __m512i soh_vec = _mm512_set1_epi8(fix::SOH);
    const size_t simd_end = data.size() & ~(AVX512_REGISTER_SIZE - 1);
    const char* __restrict ptr = data.data();

    // Process 64-byte chunks with AVX-512
    for (size_t i = 0; i < simd_end && result.count < MAX_SOH_POSITIONS - 64; i += AVX512_REGISTER_SIZE) {
        // Load 64 bytes (unaligned load)
        __m512i chunk = _mm512_loadu_si512(
            reinterpret_cast<const __m512i*>(ptr + i));

        // Compare with SOH - returns 64-bit mask directly (more efficient than AVX2)
        __mmask64 mask = _mm512_cmpeq_epi8_mask(chunk, soh_vec);

        // Extract positions from 64-bit bitmask
        while (mask != 0) [[likely]] {
            int bit = _tzcnt_u64(mask);  // Find lowest set bit (64-bit)
            result.push(static_cast<uint16_t>(i + bit));
            mask &= mask - 1;  // Clear lowest set bit
        }
    }

    // Handle remaining bytes with scalar code
    for (size_t i = simd_end; i < data.size() && result.count < MAX_SOH_POSITIONS; ++i) {
        if (ptr[i] == fix::SOH) [[unlikely]] {
            result.push(static_cast<uint16_t>(i));
        }
    }

    return result;
}

/// AVX-512 accelerated find next SOH
[[nodiscard]] NFX_HOT
inline size_t find_soh_avx512(
    std::span<const char> data,
    size_t start = 0) noexcept
{
    if (start >= data.size()) [[unlikely]] return data.size();

    const __m512i soh_vec = _mm512_set1_epi8(fix::SOH);
    const char* __restrict ptr = data.data();
    size_t i = start;

    // Align to 64-byte boundary for optimal performance
    while (i < data.size() && (reinterpret_cast<uintptr_t>(ptr + i) & 63) != 0) {
        if (ptr[i] == fix::SOH) [[unlikely]] return i;
        ++i;
    }

    // SIMD loop (64 bytes per iteration) with alignment hint
    const size_t simd_end = data.size() & ~(AVX512_REGISTER_SIZE - 1);
    while (i < simd_end) [[likely]] {
        // Hint to compiler that pointer is 64-byte aligned
        const char* aligned_ptr = std::assume_aligned<AVX512_REGISTER_SIZE>(ptr + i);
        __m512i chunk = _mm512_load_si512(
            reinterpret_cast<const __m512i*>(aligned_ptr));
        __mmask64 mask = _mm512_cmpeq_epi8_mask(chunk, soh_vec);

        if (mask != 0) [[unlikely]] {
            return i + _tzcnt_u64(mask);
        }
        i += AVX512_REGISTER_SIZE;
    }

    // Scalar tail
    while (i < data.size()) {
        if (ptr[i] == fix::SOH) [[unlikely]] return i;
        ++i;
    }

    return data.size();
}

/// AVX-512 accelerated find '='
[[nodiscard]] NFX_HOT
inline size_t find_equals_avx512(
    std::span<const char> data,
    size_t start = 0) noexcept
{
    if (start >= data.size()) [[unlikely]] return data.size();

    const __m512i eq_vec = _mm512_set1_epi8(fix::EQUALS);
    const char* __restrict ptr = data.data();
    size_t i = start;

    // Handle unaligned prefix
    while (i < data.size() && (reinterpret_cast<uintptr_t>(ptr + i) & 63) != 0) {
        if (ptr[i] == fix::EQUALS) [[unlikely]] return i;
        ++i;
    }

    // SIMD loop with alignment hint
    const size_t simd_end = data.size() & ~(AVX512_REGISTER_SIZE - 1);
    while (i < simd_end) [[likely]] {
        // Hint to compiler that pointer is 64-byte aligned
        const char* aligned_ptr = std::assume_aligned<AVX512_REGISTER_SIZE>(ptr + i);
        __m512i chunk = _mm512_load_si512(
            reinterpret_cast<const __m512i*>(aligned_ptr));
        __mmask64 mask = _mm512_cmpeq_epi8_mask(chunk, eq_vec);

        if (mask != 0) [[unlikely]] {
            return i + _tzcnt_u64(mask);
        }
        i += AVX512_REGISTER_SIZE;
    }

    // Scalar tail
    while (i < data.size()) {
        if (ptr[i] == fix::EQUALS) [[unlikely]] return i;
        ++i;
    }

    return data.size();
}

/// Count SOH occurrences using AVX-512
[[nodiscard]] NFX_HOT
inline size_t count_soh_avx512(std::span<const char> data) noexcept {
    const __m512i soh_vec = _mm512_set1_epi8(fix::SOH);
    const size_t simd_end = data.size() & ~(AVX512_REGISTER_SIZE - 1);
    const char* __restrict ptr = data.data();

    size_t count = 0;

    for (size_t i = 0; i < simd_end; i += AVX512_REGISTER_SIZE) {
        __m512i chunk = _mm512_loadu_si512(
            reinterpret_cast<const __m512i*>(ptr + i));
        __mmask64 mask = _mm512_cmpeq_epi8_mask(chunk, soh_vec);
        count += _mm_popcnt_u64(mask);  // Population count on 64-bit mask
    }

    // Scalar tail
    for (size_t i = simd_end; i < data.size(); ++i) {
        if (ptr[i] == fix::SOH) [[unlikely]] ++count;
    }

    return count;
}

#endif  // NFX_AVX512_AVAILABLE

// ============================================================================
// Unified API (auto-selects SIMD or scalar)
// ============================================================================

/// Scan for all SOH positions (auto-selects best implementation)
/// Priority: AVX-512 > AVX2 > Scalar
[[nodiscard]] NFX_HOT
inline SohPositions scan_soh(std::span<const char> data) noexcept {
#if NFX_AVX512_AVAILABLE
    // Use AVX-512 for buffers >= 128 bytes (2x register size)
    if (data.size() >= 128) [[likely]] {
        NFX_ASSUME(data.size() >= AVX512_REGISTER_SIZE);
        return scan_soh_avx512(data);
    }
#endif
#if NFX_SIMD_AVAILABLE
    // Use AVX2 for buffers >= 64 bytes
    if (data.size() >= 64) [[likely]] {
        NFX_ASSUME(data.size() >= AVX2_REGISTER_SIZE);
        return scan_soh_avx2(data);
    }
#endif
    return scan_soh_scalar(data);
}

/// Find next SOH position (auto-selects best implementation)
/// Priority: AVX-512 > AVX2 > Scalar
[[nodiscard]] NFX_HOT
inline size_t find_soh(
    std::span<const char> data,
    size_t start = 0) noexcept
{
    [[maybe_unused]] const size_t remaining = data.size() - start;
#if NFX_AVX512_AVAILABLE
    if (remaining >= 128) [[likely]] {
        NFX_ASSUME(remaining >= AVX512_REGISTER_SIZE);
        return find_soh_avx512(data, start);
    }
#endif
#if NFX_SIMD_AVAILABLE
    if (remaining >= 64) [[likely]] {
        NFX_ASSUME(remaining >= AVX2_REGISTER_SIZE);
        return find_soh_avx2(data, start);
    }
#endif
    return find_soh_scalar(data, start);
}

/// Find '=' position (auto-selects best implementation)
/// Priority: AVX-512 > AVX2 > Scalar
[[nodiscard]] NFX_HOT
inline size_t find_equals(
    std::span<const char> data,
    size_t start = 0) noexcept
{
    [[maybe_unused]] const size_t remaining = data.size() - start;
#if NFX_AVX512_AVAILABLE
    if (remaining >= 128) [[likely]] {
        NFX_ASSUME(remaining >= AVX512_REGISTER_SIZE);
        return find_equals_avx512(data, start);
    }
#endif
#if NFX_SIMD_AVAILABLE
    if (remaining >= 64) [[likely]] {
        NFX_ASSUME(remaining >= AVX2_REGISTER_SIZE);
        return find_equals_avx2(data, start);
    }
#endif
    return find_equals_scalar(data, start);
}

/// Count SOH occurrences
/// Priority: AVX-512 > AVX2 > Scalar
[[nodiscard]] NFX_HOT
inline size_t count_soh(std::span<const char> data) noexcept {
#if NFX_AVX512_AVAILABLE
    if (data.size() >= 128) [[likely]] {
        return count_soh_avx512(data);
    }
#endif
#if NFX_SIMD_AVAILABLE
    if (data.size() >= 64) [[likely]] {
        return count_soh_avx2(data);
    }
#endif
    size_t count = 0;
    for (char c : data) {
        if (c == fix::SOH) [[unlikely]] ++count;
    }
    return count;
}

// ============================================================================
// Message Boundary Detection
// ============================================================================

/// Find complete FIX message in buffer (looks for 8=FIX... 10=xxx|)
struct MessageBoundary {
    size_t start;
    size_t end;
    bool complete;

    constexpr MessageBoundary() noexcept
        : start{0}, end{0}, complete{false} {}

    constexpr MessageBoundary(size_t s, size_t e, bool c) noexcept
        : start{s}, end{e}, complete{c} {}

    [[nodiscard]] constexpr size_t size() const noexcept {
        return complete ? end - start : 0;
    }

    [[nodiscard]] constexpr std::span<const char> slice(
        std::span<const char> data) const noexcept
    {
        return complete ? data.subspan(start, end - start) : std::span<const char>{};
    }
};

/// Find message boundary (start: "8=", end: "10=xxx|")
[[nodiscard]] NFX_HOT
inline MessageBoundary find_message_boundary(
    std::span<const char> data,
    size_t start = 0) noexcept
{
    const char* __restrict ptr = data.data();

    // Find "8=FIX"
    size_t msg_start = start;
    while (msg_start + 5 < data.size()) [[likely]] {
        if (ptr[msg_start] == '8' &&
            ptr[msg_start + 1] == '=' &&
            ptr[msg_start + 2] == 'F') [[unlikely]] {
            break;
        }
        // Skip to next SOH and try again
        msg_start = find_soh(data, msg_start);
        if (msg_start < data.size()) ++msg_start;
    }

    if (msg_start + 5 >= data.size()) [[unlikely]] {
        return MessageBoundary{};  // No message start found
    }

    // Find "10=" (checksum field) - must be near end
    // Search backwards from reasonable position
    size_t search_end = std::min(msg_start + fix::MAX_MESSAGE_SIZE, data.size());

    for (size_t i = msg_start + 20; i + 7 < search_end; ++i) [[likely]] {
        // Look for SOH followed by "10="
        if (ptr[i] == fix::SOH &&
            ptr[i + 1] == '1' &&
            ptr[i + 2] == '0' &&
            ptr[i + 3] == '=') [[unlikely]] {
            // Find the final SOH after checksum value
            size_t checksum_end = find_soh(data, i + 4);
            if (checksum_end < search_end) [[likely]] {
                return MessageBoundary{msg_start, checksum_end + 1, true};
            }
        }
    }

    return MessageBoundary{msg_start, 0, false};  // Incomplete message
}

// ============================================================================
// Static Assertions for Struct Layout
// ============================================================================

// Verify SohPositions is cache-line aligned (should be 64 bytes or multiple)
static_assert(alignof(SohPositions) >= CACHE_LINE_SIZE,
    "SohPositions must be cache-line aligned to prevent false sharing");

// MessageBoundary should fit in a cache line
static_assert(sizeof(MessageBoundary) <= CACHE_LINE_SIZE,
    "MessageBoundary should fit within a single cache line");

} // namespace nfx::simd

#ifdef _MSC_VER
#pragma warning(pop)
#endif
