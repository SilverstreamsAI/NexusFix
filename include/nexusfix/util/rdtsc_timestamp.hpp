/*
    NexusFIX RDTSC-Based High-Performance Timestamp Generator

    Inspired by Quill logging library's approach:
    - Uses RDTSC for minimal latency (~10ns vs ~50ns for chrono)
    - Periodic calibration to maintain accuracy
    - Zero syscall on hot path

    FIX Timestamp Format: YYYYMMDD-HH:MM:SS.mmm
    Example: 20260122-14:30:45.123
*/

#pragma once

#include <cstdint>
#include <cstring>
#include <string_view>
#include <chrono>
#include <atomic>
#include <thread>

namespace nfx::util {

// ============================================================================
// RDTSC Primitives (inlined for hot path)
// ============================================================================

namespace detail {

/// RDTSCP - provides ordering guarantee and lower overhead than lfence+rdtsc
[[nodiscard]] inline uint64_t rdtscp() noexcept {
    uint32_t lo, hi;
    asm volatile ("rdtscp" : "=a"(lo), "=d"(hi) :: "rcx");
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

/// Compiler barrier
inline void compiler_barrier() noexcept {
    asm volatile("" ::: "memory");
}

} // namespace detail

// ============================================================================
// RDTSC Clock (calibrated)
// ============================================================================

/// High-performance clock using RDTSC with periodic calibration
class RdtscClock {
public:
    /// Initialize and calibrate the clock
    /// Should be called once at startup
    static void initialize() noexcept {
        calibrate();
    }

    /// Get current nanoseconds since epoch (fast path)
    /// ~10ns latency, no syscall
    [[nodiscard]] static uint64_t now_ns() noexcept {
        uint64_t tsc = detail::rdtscp();
        uint64_t base_ns = base_ns_.load(std::memory_order_relaxed);
        uint64_t base_tsc = base_tsc_.load(std::memory_order_relaxed);
        double freq = freq_ghz_.load(std::memory_order_relaxed);

        // Calculate nanoseconds from TSC delta
        int64_t delta_tsc = static_cast<int64_t>(tsc - base_tsc);
        int64_t delta_ns = static_cast<int64_t>(static_cast<double>(delta_tsc) / freq);

        return base_ns + delta_ns;
    }

    /// Recalibrate (call periodically, e.g., every second)
    /// This syncs with system clock to prevent drift
    static void calibrate() noexcept {
        using namespace std::chrono;

        // Get system time and TSC together
        auto sys_time = system_clock::now();
        uint64_t tsc = detail::rdtscp();

        // Convert to nanoseconds since epoch
        auto ns = duration_cast<nanoseconds>(sys_time.time_since_epoch()).count();

        base_ns_.store(static_cast<uint64_t>(ns), std::memory_order_relaxed);
        base_tsc_.store(tsc, std::memory_order_relaxed);

        // Estimate frequency if not done yet
        if (freq_ghz_.load(std::memory_order_relaxed) == 0.0) {
            estimate_frequency();
        }
    }

    /// Get CPU frequency in GHz
    [[nodiscard]] static double frequency_ghz() noexcept {
        return freq_ghz_.load(std::memory_order_relaxed);
    }

private:
    static void estimate_frequency() noexcept {
        using namespace std::chrono;

        auto start_time = steady_clock::now();
        uint64_t start_tsc = detail::rdtscp();

        // Brief sleep for calibration
        std::this_thread::sleep_for(milliseconds(10));

        uint64_t end_tsc = detail::rdtscp();
        auto end_time = steady_clock::now();

        double elapsed_ns = static_cast<double>(
            duration_cast<nanoseconds>(end_time - start_time).count());
        double cycles = static_cast<double>(end_tsc - start_tsc);

        freq_ghz_.store(cycles / elapsed_ns, std::memory_order_relaxed);
    }

    // Calibration data (atomic for thread safety)
    inline static std::atomic<uint64_t> base_ns_{0};
    inline static std::atomic<uint64_t> base_tsc_{0};
    inline static std::atomic<double> freq_ghz_{0.0};
};

// ============================================================================
// RDTSC Timestamp Generator
// ============================================================================

/// High-performance FIX timestamp generator using RDTSC
/// Hot path: ~10ns (no syscall)
/// Calibration: ~200ns (periodic, once per second)
class RdtscTimestamp {
public:
    static constexpr size_t TIMESTAMP_LEN = 21;  // "YYYYMMDD-HH:MM:SS.mmm"

    RdtscTimestamp() noexcept {
        // Initialize buffer with template
        std::memcpy(buffer_, "00000000-00:00:00.000", TIMESTAMP_LEN);
        buffer_[TIMESTAMP_LEN] = '\0';

        // Force initial calibration
        cached_second_ = 0;

        // Initialize RDTSC clock
        RdtscClock::initialize();
    }

    /// Get current FIX-formatted timestamp
    /// Fast path: ~10ns (milliseconds only, using RDTSC)
    /// Slow path: ~200ns (full date/time update + recalibration)
    [[nodiscard]] std::string_view get() noexcept {
        uint64_t now_ns = RdtscClock::now_ns();
        uint64_t now_sec = now_ns / 1'000'000'000ULL;

        // Slow path: second changed, update full timestamp
        if (now_sec != cached_second_) [[unlikely]] {
            update_full(now_ns);
            cached_second_ = now_sec;

            // Recalibrate once per second to prevent drift
            RdtscClock::calibrate();
        }

        // Fast path: only update milliseconds
        uint64_t ms = (now_ns % 1'000'000'000ULL) / 1'000'000ULL;
        update_milliseconds(static_cast<int>(ms));

        return {buffer_, TIMESTAMP_LEN};
    }

private:
    /// Update milliseconds only (fast path)
    void update_milliseconds(int ms) noexcept {
        buffer_[18] = '0' + static_cast<char>(ms / 100);
        buffer_[19] = '0' + static_cast<char>((ms / 10) % 10);
        buffer_[20] = '0' + static_cast<char>(ms % 10);
    }

    /// Update full timestamp (slow path)
    void update_full(uint64_t ns_since_epoch) noexcept {
        using namespace std::chrono;

        // Convert nanoseconds to time_point
        auto tp = system_clock::time_point{nanoseconds{ns_since_epoch}};
        auto dp = floor<days>(tp);
        auto tod = tp - dp;

        // Convert to year_month_day
        year_month_day ymd{dp};

        int year = static_cast<int>(ymd.year());
        unsigned month = static_cast<unsigned>(ymd.month());
        unsigned day = static_cast<unsigned>(ymd.day());

        // Calculate hours, minutes, seconds
        auto h = duration_cast<hours>(tod);
        auto m = duration_cast<minutes>(tod - h);
        auto s = duration_cast<seconds>(tod - h - m);

        int hour = static_cast<int>(h.count());
        int minute = static_cast<int>(m.count());
        int second = static_cast<int>(s.count());

        // Write year (positions 0-3)
        buffer_[0] = '0' + static_cast<char>(year / 1000);
        buffer_[1] = '0' + static_cast<char>((year / 100) % 10);
        buffer_[2] = '0' + static_cast<char>((year / 10) % 10);
        buffer_[3] = '0' + static_cast<char>(year % 10);

        // Write month (positions 4-5)
        buffer_[4] = '0' + static_cast<char>(month / 10);
        buffer_[5] = '0' + static_cast<char>(month % 10);

        // Write day (positions 6-7)
        buffer_[6] = '0' + static_cast<char>(day / 10);
        buffer_[7] = '0' + static_cast<char>(day % 10);

        // Position 8 is '-' (already set)

        // Write hour (positions 9-10)
        buffer_[9] = '0' + static_cast<char>(hour / 10);
        buffer_[10] = '0' + static_cast<char>(hour % 10);

        // Position 11 is ':' (already set)

        // Write minute (positions 12-13)
        buffer_[12] = '0' + static_cast<char>(minute / 10);
        buffer_[13] = '0' + static_cast<char>(minute % 10);

        // Position 14 is ':' (already set)

        // Write second (positions 15-16)
        buffer_[15] = '0' + static_cast<char>(second / 10);
        buffer_[16] = '0' + static_cast<char>(second % 10);

        // Position 17 is '.' (already set)
    }

    // Cache line aligned buffer for optimal performance
    alignas(64) char buffer_[32];
    uint64_t cached_second_;
};

// ============================================================================
// Global Instance & Convenience Function
// ============================================================================

/// Thread-local RDTSC timestamp generator
inline thread_local RdtscTimestamp g_rdtsc_timestamp;

/// Get current FIX timestamp using RDTSC (fast path: ~10ns)
[[nodiscard]] inline std::string_view rdtsc_timestamp() noexcept {
    return g_rdtsc_timestamp.get();
}

} // namespace nfx::util
