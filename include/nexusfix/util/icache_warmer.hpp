/*
    NexusFIX Instruction Cache Warmer

    Pre-warms CPU instruction cache before market open to minimize
    cold-start latency spikes.

    Why this matters:
    - First execution of code path may suffer I-Cache misses (~100+ ns)
    - Subsequent executions hit L1 I-Cache (~1-4 cycles)
    - P99 cold-start latency can be 10x worse than warm latency
    - Financial market open is critical time when every nanosecond counts

    Usage:
        // Call before market open (e.g., during pre-trading session)
        nfx::util::warm_icache();

    Implementation:
    - Executes parser hot paths with realistic message data
    - Uses atomic fence to prevent compiler from optimizing away
    - Configurable iteration count for different cache sizes
*/

#pragma once

#include <atomic>
#include <cstdint>
#include <span>
#include <string_view>

#include "nexusfix/parser/runtime_parser.hpp"
#include "nexusfix/parser/simd_scanner.hpp"

namespace nfx::util {

// ============================================================================
// Warmup Configuration
// ============================================================================

/// Default number of warmup iterations (1000 is typically sufficient)
inline constexpr size_t DEFAULT_WARMUP_ITERATIONS = 1000;

/// Warmup message templates (representative FIX 4.4 messages)
namespace warmup_messages {

/// ExecutionReport (35=8) - most common message type
inline constexpr std::string_view EXECUTION_REPORT =
    "8=FIX.4.4\x01" "9=200\x01" "35=8\x01" "49=BROKER\x01" "56=CLIENT\x01"
    "34=12345\x01" "52=20240115-09:30:00.123456\x01" "37=ORD001\x01"
    "11=CLORD001\x01" "17=EXEC001\x01" "150=0\x01" "39=0\x01"
    "55=AAPL\x01" "54=1\x01" "38=1000\x01" "44=150.50\x01"
    "32=500\x01" "31=150.45\x01" "151=500\x01" "14=500\x01"
    "6=150.475\x01" "60=20240115-09:30:00.123000\x01" "10=123\x01";

/// NewOrderSingle (35=D)
inline constexpr std::string_view NEW_ORDER_SINGLE =
    "8=FIX.4.4\x01" "9=150\x01" "35=D\x01" "49=CLIENT\x01" "56=BROKER\x01"
    "34=100\x01" "52=20240115-09:29:59.999999\x01" "11=CLORD002\x01"
    "55=MSFT\x01" "54=2\x01" "38=500\x01" "40=2\x01" "44=400.00\x01"
    "59=0\x01" "60=20240115-09:29:59.999000\x01" "10=456\x01";

/// Heartbeat (35=0) - frequent admin message
inline constexpr std::string_view HEARTBEAT =
    "8=FIX.4.4\x01" "9=60\x01" "35=0\x01" "49=BROKER\x01" "56=CLIENT\x01"
    "34=99\x01" "52=20240115-09:30:30.000000\x01" "112=TEST1\x01" "10=789\x01";

/// Logon (35=A) - session establishment
inline constexpr std::string_view LOGON =
    "8=FIX.4.4\x01" "9=100\x01" "35=A\x01" "49=CLIENT\x01" "56=BROKER\x01"
    "34=1\x01" "52=20240115-09:00:00.000000\x01" "98=0\x01" "108=30\x01"
    "141=Y\x01" "10=012\x01";

} // namespace warmup_messages

// ============================================================================
// Warmup Statistics
// ============================================================================

/// Statistics from warmup process
struct WarmupStats {
    size_t iterations;           // Number of warmup iterations completed
    size_t messages_parsed;      // Total messages parsed
    size_t parse_errors;         // Parse errors encountered (should be 0)
    uint64_t total_cycles;       // Total CPU cycles consumed (if available)

    [[nodiscard]] constexpr bool success() const noexcept {
        return parse_errors == 0;
    }
};

// ============================================================================
// Warmup Functions
// ============================================================================

/// Warm instruction cache by parsing representative messages
/// This forces hot path code into L1 I-Cache before critical operations
///
/// @param iterations Number of warmup iterations (default: 1000)
/// @return Statistics about the warmup process
[[nodiscard]] inline WarmupStats warm_icache(
    size_t iterations = DEFAULT_WARMUP_ITERATIONS) noexcept
{
    WarmupStats stats{};
    stats.iterations = iterations;

    // Message data as spans
    const std::array<std::span<const char>, 4> messages = {
        std::span<const char>{warmup_messages::EXECUTION_REPORT.data(),
                              warmup_messages::EXECUTION_REPORT.size()},
        std::span<const char>{warmup_messages::NEW_ORDER_SINGLE.data(),
                              warmup_messages::NEW_ORDER_SINGLE.size()},
        std::span<const char>{warmup_messages::HEARTBEAT.data(),
                              warmup_messages::HEARTBEAT.size()},
        std::span<const char>{warmup_messages::LOGON.data(),
                              warmup_messages::LOGON.size()}
    };

    // Warmup loop
    for (size_t i = 0; i < iterations; ++i) {
        for (const auto& msg_data : messages) {
            // Parse message (exercises hot path)
            auto result = ParsedMessage::parse(msg_data);

            // Track statistics
            ++stats.messages_parsed;
            if (!result.has_value()) [[unlikely]] {
                ++stats.parse_errors;
            }

            // Prevent compiler from optimizing away the parse
            // atomic_signal_fence ensures the parse result is "used"
            std::atomic_signal_fence(std::memory_order_seq_cst);
        }
    }

    return stats;
}

/// Warm SIMD scanning paths specifically
/// Useful if only SIMD scanning is on critical path
[[nodiscard]] inline WarmupStats warm_simd_scanner(
    size_t iterations = DEFAULT_WARMUP_ITERATIONS) noexcept
{
    WarmupStats stats{};
    stats.iterations = iterations;

    const std::span<const char> data{
        warmup_messages::EXECUTION_REPORT.data(),
        warmup_messages::EXECUTION_REPORT.size()
    };

    for (size_t i = 0; i < iterations; ++i) {
        // Warm SOH scanning (primary field delimiter)
        [[maybe_unused]] auto soh_result = simd::scan_soh(data);
        std::atomic_signal_fence(std::memory_order_seq_cst);

        // Warm equals finding
        [[maybe_unused]] auto eq_pos = simd::find_equals(data, 0);
        std::atomic_signal_fence(std::memory_order_seq_cst);

        // Warm SOH finding
        [[maybe_unused]] auto soh_pos = simd::find_soh(data, 0);
        std::atomic_signal_fence(std::memory_order_seq_cst);

        // Warm counting
        [[maybe_unused]] auto count = simd::count_soh(data);
        std::atomic_signal_fence(std::memory_order_seq_cst);

        stats.messages_parsed += 4;  // 4 operations per iteration
    }

    return stats;
}

/// Warm all parser components (comprehensive warmup)
/// Call this before market open for best P99 latency
[[nodiscard]] inline WarmupStats warm_all(
    size_t iterations = DEFAULT_WARMUP_ITERATIONS) noexcept
{
    WarmupStats stats{};

    // Warm parser (includes SIMD paths)
    auto parser_stats = warm_icache(iterations);
    stats.iterations += parser_stats.iterations;
    stats.messages_parsed += parser_stats.messages_parsed;
    stats.parse_errors += parser_stats.parse_errors;

    // Additional SIMD warmup for edge cases
    auto simd_stats = warm_simd_scanner(iterations / 10);
    stats.iterations += simd_stats.iterations;
    stats.messages_parsed += simd_stats.messages_parsed;

    return stats;
}

// ============================================================================
// RDTSC-based Warmup (with timing)
// ============================================================================

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)

/// Read Time Stamp Counter (for accurate cycle counting)
inline uint64_t rdtsc_warmup() noexcept {
    uint64_t lo, hi;
#if defined(_MSC_VER)
    lo = __rdtsc();
    hi = 0;
#else
    __asm__ volatile ("lfence; rdtsc" : "=a"(lo), "=d"(hi));
#endif
    return (hi << 32) | lo;
}

/// Warm instruction cache with cycle counting
/// Useful for benchmarking cold vs warm latency
[[nodiscard]] inline WarmupStats warm_icache_timed(
    size_t iterations = DEFAULT_WARMUP_ITERATIONS) noexcept
{
    uint64_t start_cycles = rdtsc_warmup();
    WarmupStats stats = warm_icache(iterations);
    uint64_t end_cycles = rdtsc_warmup();

    stats.total_cycles = end_cycles - start_cycles;
    return stats;
}

#else

/// Fallback for non-x86 platforms (no cycle counting)
[[nodiscard]] inline WarmupStats warm_icache_timed(
    size_t iterations = DEFAULT_WARMUP_ITERATIONS) noexcept
{
    return warm_icache(iterations);
}

#endif // x86/x64

} // namespace nfx::util
