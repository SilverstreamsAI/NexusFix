/*
    NexusFIX Benchmark Utilities

    High-precision timing and system utilities for benchmarking:
    - RDTSC cycle counting
    - CPU core affinity binding
    - Real-time scheduling
    - Latency statistics
*/

#pragma once

#include <cstdint>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <chrono>
#include <thread>
#include <iostream>
#include <iomanip>

#ifdef __linux__
#include <sched.h>
#include <pthread.h>
#include <unistd.h>
#endif

namespace nfx::bench {

// ============================================================================
// RDTSC Timing
// ============================================================================

/// Read Time Stamp Counter (basic version)
/// Use for relative measurements within same thread
[[nodiscard]] inline uint64_t rdtsc() noexcept {
    uint64_t lo, hi;
    asm volatile ("lfence; rdtsc" : "=a"(lo), "=d"(hi));
    return (hi << 32) | lo;
}

/// RDTSC with full pipeline serialization
/// More accurate but higher overhead, may cause VM Exit on cloud
[[nodiscard]] inline uint64_t rdtsc_precise() noexcept {
    uint32_t lo, hi;
    asm volatile (
        "cpuid\n\t"
        "rdtsc\n\t"
        : "=a"(lo), "=d"(hi)
        :
        : "%rbx", "%rcx"
    );
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

/// VM-safe RDTSC (use on cloud/virtualized environments)
/// Uses lfence instead of cpuid to avoid VM Exit penalty
[[nodiscard]] inline uint64_t rdtsc_vm_safe() noexcept {
    uint64_t lo, hi;
    asm volatile (
        "lfence\n\t"
        "rdtsc\n\t"
        "lfence\n\t"
        : "=a"(lo), "=d"(hi)
    );
    return (hi << 32) | lo;
}

/// RDTSCP - includes processor ID, partial ordering guarantee
[[nodiscard]] inline uint64_t rdtscp(uint32_t* processor_id = nullptr) noexcept {
    uint32_t lo, hi, aux;
    asm volatile ("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux));
    if (processor_id) *processor_id = aux;
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

/// Compiler barrier - prevent reordering around measurements
inline void compiler_barrier() noexcept {
    asm volatile("" ::: "memory");
}

// ============================================================================
// Cycle to Time Conversion
// ============================================================================

/// Estimate CPU frequency in GHz (sleep-based, may underestimate if CPU throttles)
[[nodiscard]] inline double estimate_cpu_freq_ghz() noexcept {
    using namespace std::chrono;

    auto start_time = steady_clock::now();
    uint64_t start_cycles = rdtsc_vm_safe();

    // Sleep for calibration
    std::this_thread::sleep_for(milliseconds(100));

    uint64_t end_cycles = rdtsc_vm_safe();
    auto end_time = steady_clock::now();

    double elapsed_ns = duration_cast<nanoseconds>(end_time - start_time).count();
    double cycles = static_cast<double>(end_cycles - start_cycles);

    return cycles / elapsed_ns;  // GHz
}

/// Estimate CPU frequency in GHz (busy-wait, more accurate - keeps CPU at full speed)
[[nodiscard]] inline double estimate_cpu_freq_ghz_busy() noexcept {
    using namespace std::chrono;

    auto start_time = steady_clock::now();
    uint64_t start_cycles = rdtsc_vm_safe();

    // Busy-wait to keep CPU active (prevents frequency scaling)
    while (steady_clock::now() - start_time < milliseconds(100)) {
        asm volatile("pause");
    }

    uint64_t end_cycles = rdtsc_vm_safe();
    auto end_time = steady_clock::now();

    double elapsed_ns = duration<double, std::nano>(end_time - start_time).count();
    double cycles = static_cast<double>(end_cycles - start_cycles);

    return cycles / elapsed_ns;  // GHz
}

/// Convert cycles to nanoseconds
[[nodiscard]] inline double cycles_to_ns(uint64_t cycles, double freq_ghz) noexcept {
    return static_cast<double>(cycles) / freq_ghz;
}

// ============================================================================
// CPU Affinity
// ============================================================================

/// Bind current thread to a specific CPU core
/// Returns true on success
[[nodiscard]] inline bool bind_to_core(int core_id) noexcept {
#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
#else
    (void)core_id;
    return false;
#endif
}

/// Get the current CPU core
[[nodiscard]] inline int get_current_core() noexcept {
#ifdef __linux__
    return sched_getcpu();
#else
    return -1;
#endif
}

/// Get number of available CPU cores
[[nodiscard]] inline int get_num_cores() noexcept {
#ifdef __linux__
    return static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
#else
    return 1;
#endif
}

// ============================================================================
// Real-time Scheduling
// ============================================================================

/// Set real-time scheduling for current thread
/// Requires CAP_SYS_NICE or root
[[nodiscard]] inline bool set_realtime_priority(int priority = 99) noexcept {
#ifdef __linux__
    struct sched_param param;
    param.sched_priority = priority;
    return sched_setscheduler(0, SCHED_FIFO, &param) == 0;
#else
    (void)priority;
    return false;
#endif
}

/// Setup benchmark thread with affinity and priority
[[nodiscard]] inline bool setup_benchmark_thread(int core_id, int priority = 99) noexcept {
    if (!bind_to_core(core_id)) return false;
    return set_realtime_priority(priority);
}

// ============================================================================
// Latency Statistics
// ============================================================================

/// Latency statistics calculator
struct LatencyStats {
    double min_ns{0};
    double max_ns{0};
    double mean_ns{0};
    double stddev_ns{0};
    double p50_ns{0};   // Median
    double p90_ns{0};
    double p99_ns{0};
    double p999_ns{0};
    size_t count{0};

    /// Compute statistics from a vector of cycle counts
    void compute(std::vector<uint64_t>& cycles, double freq_ghz) {
        if (cycles.empty()) return;

        count = cycles.size();

        // Sort for percentiles
        std::sort(cycles.begin(), cycles.end());

        // Convert to ns
        auto to_ns = [freq_ghz](uint64_t c) { return cycles_to_ns(c, freq_ghz); };

        min_ns = to_ns(cycles.front());
        max_ns = to_ns(cycles.back());

        // Mean
        double sum = 0;
        for (auto c : cycles) sum += to_ns(c);
        mean_ns = sum / static_cast<double>(count);

        // Standard deviation
        double sq_sum = 0;
        for (auto c : cycles) {
            double diff = to_ns(c) - mean_ns;
            sq_sum += diff * diff;
        }
        stddev_ns = std::sqrt(sq_sum / static_cast<double>(count));

        // Percentiles
        p50_ns = to_ns(cycles[count / 2]);
        p90_ns = to_ns(cycles[count * 90 / 100]);
        p99_ns = to_ns(cycles[count * 99 / 100]);
        p999_ns = to_ns(cycles[count * 999 / 1000]);
    }

    /// Compute statistics from a vector of nanoseconds
    void compute_from_ns(std::vector<double>& samples) {
        if (samples.empty()) return;

        count = samples.size();
        std::sort(samples.begin(), samples.end());

        min_ns = samples.front();
        max_ns = samples.back();

        mean_ns = std::accumulate(samples.begin(), samples.end(), 0.0) /
                  static_cast<double>(count);

        double sq_sum = 0;
        for (auto s : samples) {
            double diff = s - mean_ns;
            sq_sum += diff * diff;
        }
        stddev_ns = std::sqrt(sq_sum / static_cast<double>(count));

        p50_ns = samples[count / 2];
        p90_ns = samples[count * 90 / 100];
        p99_ns = samples[count * 99 / 100];
        p999_ns = samples[count * 999 / 1000];
    }
};

// ============================================================================
// Scoped Timer
// ============================================================================

/// RAII timer using RDTSC
class ScopedTimer {
public:
    explicit ScopedTimer(uint64_t& output) noexcept
        : output_(output), start_(rdtsc_vm_safe()) {}

    ~ScopedTimer() {
        output_ = rdtsc_vm_safe() - start_;
    }

    // Non-copyable, non-movable
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;

private:
    uint64_t& output_;
    uint64_t start_;
};

/// RAII timer using chrono
class ScopedChronoTimer {
public:
    using Clock = std::chrono::steady_clock;

    explicit ScopedChronoTimer(std::chrono::nanoseconds& output) noexcept
        : output_(output), start_(Clock::now()) {}

    ~ScopedChronoTimer() {
        output_ = Clock::now() - start_;
    }

    ScopedChronoTimer(const ScopedChronoTimer&) = delete;
    ScopedChronoTimer& operator=(const ScopedChronoTimer&) = delete;

private:
    std::chrono::nanoseconds& output_;
    Clock::time_point start_;
};

// ============================================================================
// Warmup Utilities
// ============================================================================

/// Warm up instruction cache by running function multiple times
template<typename Func>
inline void warmup_icache(Func&& func, size_t iterations = 10000) {
    for (size_t i = 0; i < iterations; ++i) {
        compiler_barrier();
        func();
        compiler_barrier();
    }
}

/// Warm up data cache by touching memory
inline void warmup_dcache(void* data, size_t size) {
    volatile char* p = static_cast<volatile char*>(data);
    for (size_t i = 0; i < size; i += 64) {  // Cache line stride
        (void)p[i];  // Read to bring into cache
    }
}

// ============================================================================
// Comparison Output Utilities
// ============================================================================

/// Print before/after comparison with delta percentage
/// @param label Operation name
/// @param before_ns Before optimization latency (ns)
/// @param after_ns After optimization latency (ns)
/// @param width Column width for formatting
inline void print_comparison(const char* label,
                            double before_ns,
                            double after_ns,
                            int width = 12) {
    double delta_pct = (before_ns - after_ns) / before_ns * 100.0;
    std::cout << std::setw(30) << std::left << label
              << std::setw(width) << std::fixed << std::setprecision(1) << before_ns
              << std::setw(width) << after_ns
              << std::setw(width) << std::showpos << delta_pct << "%"
              << std::noshowpos << "\n";
}

/// Print comparison using LatencyStats
inline void print_comparison(const char* label,
                            const LatencyStats& before,
                            const LatencyStats& after) {
    print_comparison(label, before.mean_ns, after.mean_ns);
}

/// Print comparison header
inline void print_comparison_header(const char* before_label = "Before",
                                   const char* after_label = "After") {
    std::cout << std::setw(30) << std::left << "Operation"
              << std::setw(12) << before_label
              << std::setw(12) << after_label
              << std::setw(12) << "Delta\n";
    std::cout << std::string(66, '-') << "\n";
}

} // namespace nfx::bench
