/*
    NexusFIX Highway SOH Scanner

    Portable SIMD scanner using Google Highway.
    Supports x86 (SSE4, AVX2, AVX-512), ARM (NEON, SVE), RISC-V, WASM.
*/

#pragma once

#include <cstdint>
#include <cstddef>
#include <span>
#include <array>

#include "nexusfix/platform/platform.hpp"
#include "nexusfix/interfaces/i_message.hpp"

#if defined(NFX_HAS_HIGHWAY) && NFX_HAS_HIGHWAY

#include "hwy/highway.h"

namespace nfx::simd {

namespace hn = hwy::HWY_NAMESPACE;

// ============================================================================
// Highway SOH Scanner Implementation (Static Dispatch)
// ============================================================================

/// Find next SOH position using Highway SIMD
[[nodiscard]] NFX_HOT
inline size_t find_soh_highway(std::span<const char> data, size_t start = 0) noexcept {
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data.data());
    const size_t len = data.size();

    const hn::ScalableTag<uint8_t> d;
    const size_t N = hn::Lanes(d);
    const auto soh = hn::Set(d, static_cast<uint8_t>(nfx::fix::SOH));

    size_t i = start;

    // SIMD loop
    for (; i + N <= len; i += N) {
        const auto chunk = hn::LoadU(d, ptr + i);
        const auto eq = hn::Eq(chunk, soh);

        if (!hn::AllFalse(d, eq)) {
            // Found at least one SOH, find first
            const intptr_t idx = hn::FindFirstTrue(d, eq);
            if (idx >= 0) {
                return i + static_cast<size_t>(idx);
            }
        }
    }

    // Scalar tail
    for (; i < len; ++i) {
        if (ptr[i] == static_cast<uint8_t>(nfx::fix::SOH)) {
            return i;
        }
    }

    return len;  // Not found
}

/// Find '=' position using Highway SIMD
[[nodiscard]] NFX_HOT
inline size_t find_equals_highway(std::span<const char> data, size_t start = 0) noexcept {
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data.data());
    const size_t len = data.size();

    const hn::ScalableTag<uint8_t> d;
    const size_t N = hn::Lanes(d);
    const auto eq_char = hn::Set(d, static_cast<uint8_t>(nfx::fix::EQUALS));

    size_t i = start;

    // SIMD loop
    for (; i + N <= len; i += N) {
        const auto chunk = hn::LoadU(d, ptr + i);
        const auto eq = hn::Eq(chunk, eq_char);

        if (!hn::AllFalse(d, eq)) {
            const intptr_t idx = hn::FindFirstTrue(d, eq);
            if (idx >= 0) {
                return i + static_cast<size_t>(idx);
            }
        }
    }

    // Scalar tail
    for (; i < len; ++i) {
        if (ptr[i] == static_cast<uint8_t>(nfx::fix::EQUALS)) {
            return i;
        }
    }

    return len;  // Not found
}

/// Count SOH occurrences using Highway SIMD
[[nodiscard]] NFX_HOT
inline size_t count_soh_highway(std::span<const char> data) noexcept {
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data.data());
    const size_t len = data.size();

    const hn::ScalableTag<uint8_t> d;
    const size_t N = hn::Lanes(d);
    const auto soh = hn::Set(d, static_cast<uint8_t>(nfx::fix::SOH));

    size_t count = 0;
    size_t i = 0;

    // SIMD loop
    for (; i + N <= len; i += N) {
        const auto chunk = hn::LoadU(d, ptr + i);
        const auto eq = hn::Eq(chunk, soh);
        count += hn::CountTrue(d, eq);
    }

    // Scalar tail
    for (; i < len; ++i) {
        if (ptr[i] == static_cast<uint8_t>(nfx::fix::SOH)) {
            ++count;
        }
    }

    return count;
}

/// SOH positions result (Highway version)
struct HwySohPositions {
    std::array<uint16_t, 256> positions{};
    size_t count = 0;

    [[nodiscard]] bool empty() const noexcept { return count == 0; }
    [[nodiscard]] size_t size() const noexcept { return count; }
    [[nodiscard]] uint16_t operator[](size_t i) const noexcept { return positions[i]; }

    void push(uint16_t pos) noexcept {
        if (count < positions.size()) {
            positions[count++] = pos;
        }
    }
};

/// Scan all SOH positions using Highway SIMD
[[nodiscard]] NFX_HOT
inline HwySohPositions scan_soh_highway(std::span<const char> data) noexcept {
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data.data());
    const size_t len = data.size();

    HwySohPositions result;

    const hn::ScalableTag<uint8_t> d;
    const size_t N = hn::Lanes(d);
    const auto soh = hn::Set(d, static_cast<uint8_t>(nfx::fix::SOH));

    size_t i = 0;

    // SIMD loop with scalar extraction
    for (; i + N <= len && result.count < result.positions.size() - N; i += N) {
        const auto chunk = hn::LoadU(d, ptr + i);
        const auto eq = hn::Eq(chunk, soh);

        if (!hn::AllFalse(d, eq)) {
            // Extract positions using scalar loop (simpler, still fast)
            for (size_t j = 0; j < N && result.count < result.positions.size(); ++j) {
                if (ptr[i + j] == static_cast<uint8_t>(nfx::fix::SOH)) {
                    result.push(static_cast<uint16_t>(i + j));
                }
            }
        }
    }

    // Scalar tail
    for (; i < len && result.count < result.positions.size(); ++i) {
        if (ptr[i] == static_cast<uint8_t>(nfx::fix::SOH)) {
            result.push(static_cast<uint16_t>(i));
        }
    }

    return result;
}

}  // namespace nfx::simd

#endif  // NFX_HAS_HIGHWAY
