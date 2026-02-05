#pragma once

/// @file structural_index.hpp
/// @brief simdjson-style two-stage structural indexing for FIX messages
/// @see TICKET_208_SIMDJSON_SIMD_TECHNIQUES.md
///
/// Two-Stage Parsing Pipeline:
///   Stage 1: Build structural index (SIMD-accelerated)
///            - Detect all SOH (0x01) positions
///            - Detect all '=' positions
///            - ~100ns for 500-byte message
///   Stage 2: Extract fields on demand (lazy)
///            - Parse only required tags
///            - ~20ns per accessed field

#include <span>
#include <cstdint>
#include <cstddef>
#include <array>
#include <string_view>
#include <cstring>
#include <mutex>

#include "nexusfix/platform/platform.hpp"
#include "nexusfix/util/compiler.hpp"
#include "nexusfix/interfaces/i_message.hpp"
#include "nexusfix/memory/buffer_pool.hpp"

// SIMD headers
#if defined(NFX_HAS_SIMD) && NFX_HAS_SIMD
    #include <immintrin.h>
#endif

namespace nfx::simd {

// ============================================================================
// Constants
// ============================================================================

/// SIMD register sizes
inline constexpr size_t SIMD_PADDING = 64;  // Allow overread past buffer end
inline constexpr size_t MAX_FIELDS = 256;   // Max fields in structural index

// ============================================================================
// Padded Message Buffer (simdjson-style)
// ============================================================================

/// SIMD-safe buffer with padding for overread protection
/// simdjson requires padding to allow SIMD reads past end without bounds checks
template <size_t MaxSize = 4096>
class alignas(CACHE_LINE_SIZE) PaddedMessageBuffer {
public:
    static constexpr size_t CAPACITY = MaxSize;
    static constexpr size_t TOTAL_SIZE = MaxSize + SIMD_PADDING;

    constexpr PaddedMessageBuffer() noexcept : size_{0} {
        // Zero-initialize padding region
        std::memset(buffer_.data() + MaxSize, 0, SIMD_PADDING);
    }

    /// Set buffer contents (copies data, zeros padding)
    void set(std::span<const char> msg) noexcept {
        size_ = std::min(msg.size(), MaxSize);
        std::memcpy(buffer_.data(), msg.data(), size_);
        // Zero remaining padding to prevent stale data
        if (size_ < TOTAL_SIZE) {
            std::memset(buffer_.data() + size_, 0, TOTAL_SIZE - size_);
        }
    }

    /// Get buffer as span (actual data only, not padding)
    [[nodiscard]] std::span<const char> data() const noexcept {
        return std::span<const char>{buffer_.data(), size_};
    }

    /// Get SIMD-safe pointer (can read up to SIMD_PADDING bytes past size_)
    [[nodiscard]] const char* simd_safe_ptr() const noexcept {
        return buffer_.data();
    }

    /// Get writable buffer
    [[nodiscard]] char* writable_ptr() noexcept {
        return buffer_.data();
    }

    /// Set size after external write
    void set_size(size_t n) noexcept {
        size_ = std::min(n, MaxSize);
    }

    [[nodiscard]] size_t size() const noexcept { return size_; }
    [[nodiscard]] size_t capacity() const noexcept { return MaxSize; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

private:
    alignas(CACHE_LINE_SIZE) std::array<char, TOTAL_SIZE> buffer_{};
    size_t size_;
};

// Common padded buffer sizes
using SmallPaddedBuffer = PaddedMessageBuffer<256>;
using MediumPaddedBuffer = PaddedMessageBuffer<1024>;
using LargePaddedBuffer = PaddedMessageBuffer<4096>;
using JumboPaddedBuffer = PaddedMessageBuffer<65536>;

// ============================================================================
// FIX Structural Index (simdjson-style)
// ============================================================================

/// Pre-computed structural index of FIX message delimiters
/// Stage 1 output: positions of all structural characters
struct alignas(CACHE_LINE_SIZE) FIXStructuralIndex {
    std::array<uint16_t, MAX_FIELDS> soh_positions;    // SOH (0x01) positions
    std::array<uint16_t, MAX_FIELDS> equals_positions; // '=' positions
    uint16_t soh_count;                                // Number of SOH found
    uint16_t equals_count;                             // Number of '=' found
    uint16_t checksum_start;                           // Position of tag 10=
    uint16_t body_length_start;                        // Position of tag 9=
    uint16_t msg_type_start;                           // Position of tag 35=
    uint16_t message_size;                             // Total message size

    constexpr FIXStructuralIndex() noexcept
        : soh_positions{}
        , equals_positions{}
        , soh_count{0}
        , equals_count{0}
        , checksum_start{0}
        , body_length_start{0}
        , msg_type_start{0}
        , message_size{0}
    {}

    /// Get field count (number of tag=value pairs)
    [[nodiscard]] constexpr size_t field_count() const noexcept {
        return soh_count;
    }

    /// Check if index is valid
    [[nodiscard]] constexpr bool valid() const noexcept {
        return soh_count > 0 && equals_count > 0 && soh_count == equals_count;
    }

    /// Get tag boundaries for field at index
    /// Returns [tag_start, tag_end, value_start, value_end]
    [[nodiscard]] constexpr std::array<uint16_t, 4>
    field_bounds(size_t field_idx) const noexcept {
        if (field_idx >= soh_count) [[unlikely]] {
            return {0, 0, 0, 0};
        }

        uint16_t tag_start = (field_idx == 0) ? 0 : soh_positions[field_idx - 1] + 1;
        uint16_t tag_end = equals_positions[field_idx];
        uint16_t value_start = tag_end + 1;
        uint16_t value_end = soh_positions[field_idx];

        return {tag_start, tag_end, value_start, value_end};
    }

    /// Extract tag number at field index (lazy parse)
    [[nodiscard]] int tag_at(std::span<const char> msg, size_t field_idx) const noexcept {
        auto bounds = field_bounds(field_idx);
        if (bounds[0] >= bounds[1]) [[unlikely]] return 0;

        int tag = 0;
        for (uint16_t i = bounds[0]; i < bounds[1]; ++i) {
            char c = msg[i];
            if (c < '0' || c > '9') [[unlikely]] return 0;
            tag = tag * 10 + (c - '0');
        }
        return tag;
    }

    /// Extract value at field index as string_view (zero-copy)
    [[nodiscard]] std::string_view value_at(
        std::span<const char> msg,
        size_t field_idx) const noexcept
    {
        auto bounds = field_bounds(field_idx);
        if (bounds[2] >= bounds[3] || bounds[3] > msg.size()) [[unlikely]] {
            return {};
        }
        return std::string_view{msg.data() + bounds[2],
            static_cast<size_t>(bounds[3] - bounds[2])};
    }

    /// Find field by tag number (linear search through index)
    [[nodiscard]] size_t find_tag(std::span<const char> msg, int target_tag) const noexcept {
        for (size_t i = 0; i < soh_count; ++i) {
            if (tag_at(msg, i) == target_tag) {
                return i;
            }
        }
        return soh_count;  // Not found
    }
};

// ============================================================================
// Scalar Implementation
// ============================================================================

/// Build structural index using scalar code (fallback)
[[nodiscard]] NFX_HOT
inline FIXStructuralIndex build_index_scalar(std::span<const char> data) noexcept {
    FIXStructuralIndex idx;
    idx.message_size = static_cast<uint16_t>(data.size());

    const char* ptr = data.data();
    const size_t len = data.size();

    for (size_t i = 0; i < len && idx.soh_count < MAX_FIELDS; ++i) {
        if (ptr[i] == fix::EQUALS) [[unlikely]] {
            idx.equals_positions[idx.equals_count++] = static_cast<uint16_t>(i);

            // Check for important tags (1-2 digit tags only for speed)
            if (i >= 1 && ptr[i-1] >= '0' && ptr[i-1] <= '9') {
                int tag = ptr[i-1] - '0';
                if (i >= 2 && ptr[i-2] >= '0' && ptr[i-2] <= '9') {
                    tag = (ptr[i-2] - '0') * 10 + tag;
                }
                // Track important tag positions
                if (tag == 9) idx.body_length_start = static_cast<uint16_t>(i - (i >= 2 && ptr[i-2] >= '0' ? 2 : 1));
                else if (tag == 10) idx.checksum_start = static_cast<uint16_t>(i - 2);
                else if (tag == 35) idx.msg_type_start = static_cast<uint16_t>(i - 2);
            }
        }
        else if (ptr[i] == fix::SOH) [[unlikely]] {
            idx.soh_positions[idx.soh_count++] = static_cast<uint16_t>(i);
        }
    }

    return idx;
}

// ============================================================================
// AVX2 Implementation
// ============================================================================

#if defined(NFX_HAS_SIMD) && NFX_HAS_SIMD

/// Extract all set bit positions from 32-bit mask
inline void extract_positions_avx2(
    uint32_t mask,
    size_t offset,
    uint16_t* positions,
    uint16_t& count,
    uint16_t max_count) noexcept
{
    while (mask != 0 && count < max_count) {
        uint32_t bit = __builtin_ctz(mask);  // Trailing zero count
        positions[count++] = static_cast<uint16_t>(offset + bit);
        mask &= mask - 1;  // Clear lowest set bit (branch-free)
    }
}

/// Build structural index using AVX2 (processes 32 bytes at a time)
[[nodiscard]] NFX_HOT
inline FIXStructuralIndex build_index_avx2(std::span<const char> data) noexcept {
    FIXStructuralIndex idx;
    idx.message_size = static_cast<uint16_t>(data.size());

    const __m256i soh_vec = _mm256_set1_epi8(fix::SOH);
    const __m256i eq_vec = _mm256_set1_epi8(fix::EQUALS);
    const size_t simd_end = data.size() & ~31ULL;  // Round down to 32
    const char* __restrict ptr = data.data();

    // Process 32-byte chunks
    for (size_t i = 0; i < simd_end && idx.soh_count < MAX_FIELDS - 32; i += 32) {
        __m256i chunk = _mm256_loadu_si256(
            reinterpret_cast<const __m256i*>(ptr + i));

        // Detect SOH positions
        __m256i soh_cmp = _mm256_cmpeq_epi8(chunk, soh_vec);
        uint32_t soh_mask = static_cast<uint32_t>(_mm256_movemask_epi8(soh_cmp));

        // Detect equals positions
        __m256i eq_cmp = _mm256_cmpeq_epi8(chunk, eq_vec);
        uint32_t eq_mask = static_cast<uint32_t>(_mm256_movemask_epi8(eq_cmp));

        // Extract positions from masks
        extract_positions_avx2(soh_mask, i, idx.soh_positions.data(),
                               idx.soh_count, MAX_FIELDS);
        extract_positions_avx2(eq_mask, i, idx.equals_positions.data(),
                               idx.equals_count, MAX_FIELDS);
    }

    // Handle remaining bytes with scalar code
    for (size_t i = simd_end; i < data.size() && idx.soh_count < MAX_FIELDS; ++i) {
        if (ptr[i] == fix::EQUALS) [[unlikely]] {
            idx.equals_positions[idx.equals_count++] = static_cast<uint16_t>(i);
        }
        else if (ptr[i] == fix::SOH) [[unlikely]] {
            idx.soh_positions[idx.soh_count++] = static_cast<uint16_t>(i);
        }
    }

    // Post-process to find important tags
    for (uint16_t i = 0; i < idx.equals_count && i < 10; ++i) {
        uint16_t eq_pos = idx.equals_positions[i];
        if (eq_pos < 2) continue;

        // Check for 2-digit tags
        if (ptr[eq_pos - 2] >= '0' && ptr[eq_pos - 2] <= '9' &&
            ptr[eq_pos - 1] >= '0' && ptr[eq_pos - 1] <= '9') {
            int tag = (ptr[eq_pos - 2] - '0') * 10 + (ptr[eq_pos - 1] - '0');
            if (tag == 35) idx.msg_type_start = eq_pos - 2;
        }
        // Check for 1-digit tags
        else if (ptr[eq_pos - 1] >= '0' && ptr[eq_pos - 1] <= '9') {
            int tag = ptr[eq_pos - 1] - '0';
            if (tag == 8) continue;  // BeginString
            if (tag == 9) idx.body_length_start = eq_pos - 1;
        }
    }

    // Find checksum tag (near end)
    if (idx.equals_count > 0) {
        size_t end_idx = (idx.equals_count > 5) ? idx.equals_count - 5 : 0;
        for (size_t i = idx.equals_count; i > end_idx; --i) {
            uint16_t eq_pos = idx.equals_positions[i - 1];
            if (eq_pos >= 2 && ptr[eq_pos - 2] == '1' && ptr[eq_pos - 1] == '0') {
                idx.checksum_start = eq_pos - 2;
                break;
            }
        }
    }

    return idx;
}

#endif  // NFX_HAS_SIMD

// ============================================================================
// AVX-512 Implementation
// ============================================================================

#if defined(__AVX512F__) && defined(__AVX512BW__)

/// Extract all set bit positions from 64-bit mask
inline void extract_positions_avx512(
    uint64_t mask,
    size_t offset,
    uint16_t* positions,
    uint16_t& count,
    uint16_t max_count) noexcept
{
    while (mask != 0 && count < max_count) {
        uint64_t bit = _tzcnt_u64(mask);  // Trailing zero count (64-bit)
        positions[count++] = static_cast<uint16_t>(offset + bit);
        mask &= mask - 1;  // Clear lowest set bit
    }
}

/// Build structural index using AVX-512 (processes 64 bytes at a time)
[[nodiscard]] NFX_HOT
inline FIXStructuralIndex build_index_avx512(std::span<const char> data) noexcept {
    FIXStructuralIndex idx;
    idx.message_size = static_cast<uint16_t>(data.size());

    const __m512i soh_vec = _mm512_set1_epi8(fix::SOH);
    const __m512i eq_vec = _mm512_set1_epi8(fix::EQUALS);
    const size_t simd_end = data.size() & ~63ULL;  // Round down to 64
    const char* __restrict ptr = data.data();

    // Process 64-byte chunks
    for (size_t i = 0; i < simd_end && idx.soh_count < MAX_FIELDS - 64; i += 64) {
        __m512i chunk = _mm512_loadu_si512(
            reinterpret_cast<const __m512i*>(ptr + i));

        // Detect SOH positions (returns 64-bit mask directly)
        __mmask64 soh_mask = _mm512_cmpeq_epi8_mask(chunk, soh_vec);

        // Detect equals positions
        __mmask64 eq_mask = _mm512_cmpeq_epi8_mask(chunk, eq_vec);

        // Extract positions from masks
        extract_positions_avx512(soh_mask, i, idx.soh_positions.data(),
                                 idx.soh_count, MAX_FIELDS);
        extract_positions_avx512(eq_mask, i, idx.equals_positions.data(),
                                 idx.equals_count, MAX_FIELDS);
    }

    // Handle remaining bytes with scalar code
    for (size_t i = simd_end; i < data.size() && idx.soh_count < MAX_FIELDS; ++i) {
        if (ptr[i] == fix::EQUALS) [[unlikely]] {
            idx.equals_positions[idx.equals_count++] = static_cast<uint16_t>(i);
        }
        else if (ptr[i] == fix::SOH) [[unlikely]] {
            idx.soh_positions[idx.soh_count++] = static_cast<uint16_t>(i);
        }
    }

    // Post-process to find important tags (same as AVX2)
    for (uint16_t i = 0; i < idx.equals_count && i < 10; ++i) {
        uint16_t eq_pos = idx.equals_positions[i];
        if (eq_pos < 2) continue;

        if (ptr[eq_pos - 2] >= '0' && ptr[eq_pos - 2] <= '9' &&
            ptr[eq_pos - 1] >= '0' && ptr[eq_pos - 1] <= '9') {
            int tag = (ptr[eq_pos - 2] - '0') * 10 + (ptr[eq_pos - 1] - '0');
            if (tag == 35) idx.msg_type_start = eq_pos - 2;
        }
        else if (ptr[eq_pos - 1] >= '0' && ptr[eq_pos - 1] <= '9') {
            int tag = ptr[eq_pos - 1] - '0';
            if (tag == 9) idx.body_length_start = eq_pos - 1;
        }
    }

    if (idx.equals_count > 0) {
        size_t end_idx = (idx.equals_count > 5) ? idx.equals_count - 5 : 0;
        for (size_t i = idx.equals_count; i > end_idx; --i) {
            uint16_t eq_pos = idx.equals_positions[i - 1];
            if (eq_pos >= 2 && ptr[eq_pos - 2] == '1' && ptr[eq_pos - 1] == '0') {
                idx.checksum_start = eq_pos - 2;
                break;
            }
        }
    }

    return idx;
}

#endif  // AVX-512

// ============================================================================
// Runtime SIMD Dispatch (simdjson-style)
// ============================================================================

/// SIMD implementation level
enum class SimdImpl : uint8_t {
    Scalar = 0,
    AVX2 = 1,
    AVX512 = 2
};

/// Get implementation name
[[nodiscard]] inline constexpr const char* simd_impl_name(SimdImpl impl) noexcept {
    switch (impl) {
        case SimdImpl::Scalar: return "Scalar";
        case SimdImpl::AVX2:   return "AVX2";
        case SimdImpl::AVX512: return "AVX-512";
    }
    return "Unknown";
}

namespace detail {

/// Function pointer type for build_index
using BuildIndexFn = FIXStructuralIndex(*)(std::span<const char>) noexcept;

/// Runtime-selected implementation
inline BuildIndexFn g_build_index_fn = nullptr;
inline SimdImpl g_active_impl = SimdImpl::Scalar;
inline bool g_initialized = false;

/// Detect CPU capabilities at runtime using CPUID
[[nodiscard]] inline SimdImpl detect_best_impl() noexcept {
#if NFX_ARCH_X64 || NFX_ARCH_X86
    // Check for AVX-512 support (both F and BW required)
    #if defined(__AVX512F__) && defined(__AVX512BW__)
    if (__builtin_cpu_supports("avx512f") && __builtin_cpu_supports("avx512bw")) {
        return SimdImpl::AVX512;
    }
    #endif

    // Check for AVX2 support
    #if defined(NFX_HAS_SIMD) && NFX_HAS_SIMD
    if (__builtin_cpu_supports("avx2")) {
        return SimdImpl::AVX2;
    }
    #endif
#endif

    return SimdImpl::Scalar;
}

/// Select function pointer based on implementation
[[nodiscard]] inline BuildIndexFn select_build_index_fn(SimdImpl impl) noexcept {
    switch (impl) {
#if defined(__AVX512F__) && defined(__AVX512BW__)
        case SimdImpl::AVX512:
            return build_index_avx512;
#endif
#if defined(NFX_HAS_SIMD) && NFX_HAS_SIMD
        case SimdImpl::AVX2:
            return build_index_avx2;
#endif
        case SimdImpl::Scalar:
        default:
            return build_index_scalar;
    }
}

}  // namespace detail

/// Initialize runtime SIMD dispatch (call once at startup)
inline void init_simd_dispatch() noexcept {
    static std::once_flag flag;
    std::call_once(flag, []() {
        // Auto-detect best implementation
        detail::g_active_impl = detail::detect_best_impl();

        // Check for environment override (for testing)
        if (const char* impl = std::getenv("NFX_SIMD_IMPL")) {
            if (std::strcmp(impl, "scalar") == 0) {
                detail::g_active_impl = SimdImpl::Scalar;
            }
#if defined(NFX_HAS_SIMD) && NFX_HAS_SIMD
            else if (std::strcmp(impl, "avx2") == 0) {
                detail::g_active_impl = SimdImpl::AVX2;
            }
#endif
#if defined(__AVX512F__) && defined(__AVX512BW__)
            else if (std::strcmp(impl, "avx512") == 0) {
                detail::g_active_impl = SimdImpl::AVX512;
            }
#endif
        }

        // Set function pointer
        detail::g_build_index_fn = detail::select_build_index_fn(detail::g_active_impl);
        detail::g_initialized = true;
    });
}

/// Get current active SIMD implementation
[[nodiscard]] inline SimdImpl active_simd_impl() noexcept {
    if (!detail::g_initialized) [[unlikely]] {
        init_simd_dispatch();
    }
    return detail::g_active_impl;
}

/// Build structural index (uses runtime-selected implementation)
[[nodiscard]] NFX_HOT
inline FIXStructuralIndex build_index(std::span<const char> data) noexcept {
    if (!detail::g_initialized) [[unlikely]] {
        init_simd_dispatch();
    }
    return detail::g_build_index_fn(data);
}

// ============================================================================
// FIX Field Accessor (lazy parsing from index)
// ============================================================================

/// Zero-copy field accessor using structural index
/// Stage 2: Extract fields on demand
class IndexedFieldAccessor {
public:
    IndexedFieldAccessor(const FIXStructuralIndex& idx, std::span<const char> msg) noexcept
        : idx_{idx}, msg_{msg} {}

    /// Get number of fields
    [[nodiscard]] size_t field_count() const noexcept {
        return idx_.field_count();
    }

    /// Get tag at field index
    [[nodiscard]] int tag(size_t field_idx) const noexcept {
        return idx_.tag_at(msg_, field_idx);
    }

    /// Get value at field index (zero-copy)
    [[nodiscard]] std::string_view value(size_t field_idx) const noexcept {
        return idx_.value_at(msg_, field_idx);
    }

    /// Find field by tag and return value (zero-copy)
    [[nodiscard]] std::string_view get(int target_tag) const noexcept {
        size_t idx = idx_.find_tag(msg_, target_tag);
        if (idx < idx_.field_count()) {
            return idx_.value_at(msg_, idx);
        }
        return {};
    }

    /// Get value as integer
    [[nodiscard]] int64_t get_int(int target_tag) const noexcept {
        std::string_view v = get(target_tag);
        if (v.empty()) return 0;

        int64_t result = 0;
        bool negative = false;
        size_t i = 0;

        if (v[0] == '-') {
            negative = true;
            i = 1;
        }

        for (; i < v.size(); ++i) {
            char c = v[i];
            if (c < '0' || c > '9') break;
            result = result * 10 + (c - '0');
        }

        return negative ? -result : result;
    }

    /// Get value as char (single character field)
    [[nodiscard]] char get_char(int target_tag) const noexcept {
        std::string_view v = get(target_tag);
        return v.empty() ? '\0' : v[0];
    }

    /// Get message type (tag 35)
    [[nodiscard]] char msg_type() const noexcept {
        return get_char(35);
    }

    /// Get body length (tag 9)
    [[nodiscard]] int body_length() const noexcept {
        return static_cast<int>(get_int(9));
    }

    /// Get checksum (tag 10)
    [[nodiscard]] std::string_view checksum() const noexcept {
        return get(10);
    }

private:
    const FIXStructuralIndex& idx_;
    std::span<const char> msg_;
};

// ============================================================================
// Static Assertions
// ============================================================================

static_assert(alignof(FIXStructuralIndex) == CACHE_LINE_SIZE,
    "FIXStructuralIndex must be cache-line aligned");

static_assert(sizeof(FIXStructuralIndex) % CACHE_LINE_SIZE == 0 ||
              sizeof(FIXStructuralIndex) <= CACHE_LINE_SIZE * 20,
    "FIXStructuralIndex size should be reasonable");

}  // namespace nfx::simd
