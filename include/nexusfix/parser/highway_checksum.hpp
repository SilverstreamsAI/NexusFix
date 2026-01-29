/*
    NexusFIX Highway Checksum Calculator

    Portable SIMD checksum using Google Highway.
    Supports x86 (SSE4, AVX2, AVX-512), ARM (NEON, SVE), RISC-V, WASM.
*/

#pragma once

#include <cstdint>
#include <cstddef>
#include <span>
#include <string_view>

#include "nexusfix/platform/platform.hpp"

#if defined(NFX_HAS_HIGHWAY) && NFX_HAS_HIGHWAY

#include "hwy/highway.h"

namespace nfx::parser {

namespace hn = hwy::HWY_NAMESPACE;

/// Highway-based checksum calculation (static dispatch for simplicity)
[[nodiscard]] NFX_HOT
inline uint8_t checksum_highway(const char* data, size_t len) noexcept {
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data);

    const hn::ScalableTag<uint8_t> d8;
    const hn::ScalableTag<uint64_t> d64;
    const size_t N = hn::Lanes(d8);

    // Use 64-bit accumulator to avoid overflow
    auto sum64 = hn::Zero(d64);

    size_t i = 0;

    // Main SIMD loop
    for (; i + N <= len; i += N) {
        const auto chunk = hn::LoadU(d8, ptr + i);
        // SumsOf8: sum groups of 8 bytes into 64-bit lanes
        sum64 = hn::Add(sum64, hn::SumsOf8(chunk));
    }

    // Horizontal sum across all lanes
    uint64_t total = hn::ReduceSum(d64, sum64);

    // Process remaining bytes (scalar tail)
    for (; i < len; ++i) {
        total += ptr[i];
    }

    return static_cast<uint8_t>(total & 0xFF);
}

/// Highway checksum for string_view
[[nodiscard]] NFX_HOT
inline uint8_t checksum_highway(std::string_view data) noexcept {
    return checksum_highway(data.data(), data.size());
}

/// Highway checksum for span
[[nodiscard]] NFX_HOT
inline uint8_t checksum_highway(std::span<const char> data) noexcept {
    return checksum_highway(data.data(), data.size());
}

}  // namespace nfx::parser

#endif  // NFX_HAS_HIGHWAY
