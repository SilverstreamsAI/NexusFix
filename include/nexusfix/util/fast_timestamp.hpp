/*
    NexusFIX High-Performance Timestamp Generator

    Optimized for hot path usage:
    - Caches date/hour/minute/second portion
    - Only updates milliseconds on fast path (~10ns)
    - Full update only when second changes (~200ns)

    FIX Timestamp Format: YYYYMMDD-HH:MM:SS.mmm
    Example: 20260122-14:30:45.123
*/

#pragma once

#include <chrono>
#include <cstdint>
#include <string_view>
#include <cstring>

namespace nfx::util {

class FastTimestamp {
public:
    static constexpr size_t TIMESTAMP_LEN = 21;  // "YYYYMMDD-HH:MM:SS.mmm"

    FastTimestamp() noexcept {
        // Initialize buffer with template
        std::memcpy(buffer_, "00000000-00:00:00.000", TIMESTAMP_LEN);
        buffer_[TIMESTAMP_LEN] = '\0';

        // Force initial update
        cached_second_ = std::chrono::sys_seconds{};
    }

    // Get current FIX-formatted timestamp
    // Fast path: ~10ns (milliseconds only)
    // Slow path: ~200ns (full date/time update)
    [[nodiscard]] std::string_view get() noexcept {
        using namespace std::chrono;

        auto now = system_clock::now();
        auto now_sec = floor<seconds>(now);

        // Slow path: second changed, update full timestamp
        if (now_sec != cached_second_) [[unlikely]] {
            update_full(now_sec);
            cached_second_ = now_sec;
        }

        // Fast path: only update milliseconds
        auto ms = duration_cast<milliseconds>(now - now_sec).count();
        update_milliseconds(static_cast<int>(ms));

        return {buffer_, TIMESTAMP_LEN};
    }

    // Get timestamp for a specific time point
    [[nodiscard]] std::string_view get(std::chrono::system_clock::time_point tp) noexcept {
        using namespace std::chrono;

        auto tp_sec = floor<seconds>(tp);

        if (tp_sec != cached_second_) [[unlikely]] {
            update_full(tp_sec);
            cached_second_ = tp_sec;
        }

        auto ms = duration_cast<milliseconds>(tp - tp_sec).count();
        update_milliseconds(static_cast<int>(ms));

        return {buffer_, TIMESTAMP_LEN};
    }

private:
    // Update milliseconds only (fast path)
    // Buffer positions 18, 19, 20 = "mmm"
    void update_milliseconds(int ms) noexcept {
        buffer_[18] = '0' + static_cast<char>(ms / 100);
        buffer_[19] = '0' + static_cast<char>((ms / 10) % 10);
        buffer_[20] = '0' + static_cast<char>(ms % 10);
    }

    // Update full timestamp (slow path)
    void update_full(std::chrono::sys_seconds tp) noexcept {
        using namespace std::chrono;

        // Convert to days and time-of-day
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
        // Positions 18-20 are milliseconds (updated in fast path)
    }

    // Cache line aligned buffer for optimal performance
    alignas(64) char buffer_[32];  // Extra space for alignment padding
    std::chrono::sys_seconds cached_second_;
};

// Global instance for convenience (thread-local for thread safety)
inline thread_local FastTimestamp g_fast_timestamp;

// Convenience function
[[nodiscard]] inline std::string_view fast_timestamp() noexcept {
    return g_fast_timestamp.get();
}

} // namespace nfx::util
