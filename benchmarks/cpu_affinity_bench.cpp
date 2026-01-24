// Benchmark: CPU Core Pinning Performance
// Compares latency jitter with and without CPU affinity
//
// Build: cmake --build build && ./build/bin/benchmarks/cpu_affinity_bench

#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>
#include <thread>
#include <atomic>
#include <cstring>

#include "nexusfix/util/cpu_affinity.hpp"

// Benchmark configuration
constexpr int WARMUP_ITERATIONS = 10000;
constexpr int BENCHMARK_ITERATIONS = 100000;
constexpr int NUM_RUNS = 5;

// RDTSC for precise timing
inline uint64_t rdtsc() {
    uint64_t lo, hi;
    asm volatile("rdtscp" : "=a"(lo), "=d"(hi) :: "rcx");
    return (hi << 32) | lo;
}

// Get CPU frequency
double get_cpu_freq_ghz() {
    auto start = std::chrono::steady_clock::now();
    uint64_t start_tsc = rdtsc();

    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(100)) {
        asm volatile("pause");
    }

    auto end = std::chrono::steady_clock::now();
    uint64_t end_tsc = rdtsc();

    double elapsed_ns = std::chrono::duration<double, std::nano>(end - start).count();
    return static_cast<double>(end_tsc - start_tsc) / elapsed_ns;
}

struct BenchmarkResult {
    double mean_ns;
    double median_ns;
    double p99_ns;
    double p999_ns;
    double min_ns;
    double max_ns;
    double stddev_ns;
};

BenchmarkResult calculate_stats(std::vector<uint64_t>& latencies, double cpu_freq_ghz) {
    std::sort(latencies.begin(), latencies.end());

    size_t n = latencies.size();
    std::vector<double> ns_latencies(n);
    for (size_t i = 0; i < n; ++i) {
        ns_latencies[i] = static_cast<double>(latencies[i]) / cpu_freq_ghz;
    }

    double sum = std::accumulate(ns_latencies.begin(), ns_latencies.end(), 0.0);
    double mean = sum / n;

    // Calculate standard deviation
    double sq_sum = 0.0;
    for (double v : ns_latencies) {
        sq_sum += (v - mean) * (v - mean);
    }
    double stddev = std::sqrt(sq_sum / n);

    return BenchmarkResult{
        .mean_ns = mean,
        .median_ns = ns_latencies[n / 2],
        .p99_ns = ns_latencies[static_cast<size_t>(n * 0.99)],
        .p999_ns = ns_latencies[static_cast<size_t>(n * 0.999)],
        .min_ns = ns_latencies.front(),
        .max_ns = ns_latencies.back(),
        .stddev_ns = stddev
    };
}

// Simulate FIX message processing workload
struct FIXWorkload {
    char buffer[256];
    volatile uint64_t checksum{0};

    void process() noexcept {
        // Simulate: parse header, validate checksum, lookup session
        uint64_t sum = 0;
        for (int i = 0; i < 64; ++i) {
            sum += buffer[i];
        }
        checksum = sum;

        // Memory fence to ensure work is complete
        asm volatile("" ::: "memory");
    }
};

// Benchmark function - measures latency of simulated FIX processing
BenchmarkResult benchmark_processing(double cpu_freq_ghz, int iterations) {
    std::vector<uint64_t> latencies;
    latencies.reserve(iterations);

    FIXWorkload workload;
    std::memset(workload.buffer, 'A', sizeof(workload.buffer));

    for (int i = 0; i < iterations; ++i) {
        uint64_t start = rdtsc();
        workload.process();
        uint64_t end = rdtsc();

        latencies.push_back(end - start);
    }

    return calculate_stats(latencies, cpu_freq_ghz);
}

void print_result(const char* name, const BenchmarkResult& r) {
    std::cout << std::setw(25) << std::left << name
              << std::setw(10) << std::fixed << std::setprecision(1) << r.mean_ns
              << std::setw(10) << r.median_ns
              << std::setw(10) << r.p99_ns
              << std::setw(10) << r.p999_ns
              << std::setw(10) << r.max_ns
              << std::setw(10) << r.stddev_ns << "\n";
}

int main() {
    using namespace nfx::util;

    std::cout << "==========================================================\n";
    std::cout << "  CPU Affinity Benchmark: Core Pinning Impact on Latency\n";
    std::cout << "==========================================================\n\n";

    std::cout << "Calibrating CPU frequency...\n";
    double cpu_freq_ghz = get_cpu_freq_ghz();
    std::cout << "  CPU frequency: " << std::fixed << std::setprecision(3)
              << cpu_freq_ghz << " GHz\n";

    std::cout << "\nSystem information:\n";
    std::cout << "  Total cores: " << CpuAffinity::core_count() << "\n";
    std::cout << "  Current core: " << CpuAffinity::current_core() << "\n";

    auto initial_affinity = CpuAffinity::get_affinity();
    std::cout << "  Initial affinity: ";
    if (initial_affinity.empty()) {
        std::cout << "all cores";
    } else {
        for (size_t i = 0; i < initial_affinity.size(); ++i) {
            if (i > 0) std::cout << ",";
            std::cout << initial_affinity[i];
        }
    }
    std::cout << "\n";

    std::cout << "\nConfiguration:\n";
    std::cout << "  Warmup:       " << WARMUP_ITERATIONS << " iterations\n";
    std::cout << "  Benchmark:    " << BENCHMARK_ITERATIONS << " iterations\n";
    std::cout << "  Runs:         " << NUM_RUNS << "\n";

    // Warmup
    std::cout << "\nWarming up...\n";
    benchmark_processing(cpu_freq_ghz, WARMUP_ITERATIONS);

    // ========================================================================
    // Benchmark WITHOUT core pinning
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  WITHOUT Core Pinning (OS can migrate thread)\n";
    std::cout << "----------------------------------------------------------\n";
    std::cout << std::setw(25) << "Metric"
              << std::setw(10) << "Mean"
              << std::setw(10) << "Median"
              << std::setw(10) << "P99"
              << std::setw(10) << "P99.9"
              << std::setw(10) << "Max"
              << std::setw(10) << "StdDev\n";
    std::cout << std::string(85, '-') << "\n";

    std::vector<BenchmarkResult> unpinned_results;
    for (int run = 0; run < NUM_RUNS; ++run) {
        unpinned_results.push_back(benchmark_processing(cpu_freq_ghz, BENCHMARK_ITERATIONS));
    }

    // Average unpinned results
    BenchmarkResult unpinned_avg{};
    for (const auto& r : unpinned_results) {
        unpinned_avg.mean_ns += r.mean_ns;
        unpinned_avg.median_ns += r.median_ns;
        unpinned_avg.p99_ns += r.p99_ns;
        unpinned_avg.p999_ns += r.p999_ns;
        unpinned_avg.max_ns += r.max_ns;
        unpinned_avg.stddev_ns += r.stddev_ns;
    }
    unpinned_avg.mean_ns /= NUM_RUNS;
    unpinned_avg.median_ns /= NUM_RUNS;
    unpinned_avg.p99_ns /= NUM_RUNS;
    unpinned_avg.p999_ns /= NUM_RUNS;
    unpinned_avg.max_ns /= NUM_RUNS;
    unpinned_avg.stddev_ns /= NUM_RUNS;

    print_result("Unpinned (average)", unpinned_avg);

    // ========================================================================
    // Benchmark WITH core pinning
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  WITH Core Pinning (thread locked to single core)\n";
    std::cout << "----------------------------------------------------------\n";

    // Pin to a core (use core 2 if available, otherwise 0)
    int target_core = CpuAffinity::core_count() > 2 ? 2 : 0;
    auto pin_result = CpuAffinity::pin_to_core(target_core);

    if (!pin_result.success) {
        std::cout << "WARNING: Failed to pin to core " << target_core
                  << " (error: " << pin_result.error_code << ")\n";
        std::cout << "Running pinned benchmark on current core anyway...\n";
    } else {
        std::cout << "Pinned to core: " << pin_result.core_id << "\n";
    }

    std::cout << std::setw(25) << "Metric"
              << std::setw(10) << "Mean"
              << std::setw(10) << "Median"
              << std::setw(10) << "P99"
              << std::setw(10) << "P99.9"
              << std::setw(10) << "Max"
              << std::setw(10) << "StdDev\n";
    std::cout << std::string(85, '-') << "\n";

    std::vector<BenchmarkResult> pinned_results;
    for (int run = 0; run < NUM_RUNS; ++run) {
        pinned_results.push_back(benchmark_processing(cpu_freq_ghz, BENCHMARK_ITERATIONS));
    }

    // Average pinned results
    BenchmarkResult pinned_avg{};
    for (const auto& r : pinned_results) {
        pinned_avg.mean_ns += r.mean_ns;
        pinned_avg.median_ns += r.median_ns;
        pinned_avg.p99_ns += r.p99_ns;
        pinned_avg.p999_ns += r.p999_ns;
        pinned_avg.max_ns += r.max_ns;
        pinned_avg.stddev_ns += r.stddev_ns;
    }
    pinned_avg.mean_ns /= NUM_RUNS;
    pinned_avg.median_ns /= NUM_RUNS;
    pinned_avg.p99_ns /= NUM_RUNS;
    pinned_avg.p999_ns /= NUM_RUNS;
    pinned_avg.max_ns /= NUM_RUNS;
    pinned_avg.stddev_ns /= NUM_RUNS;

    print_result("Pinned (average)", pinned_avg);

    // ========================================================================
    // Session-based affinity test
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Session-Based Core Assignment (hash-based pinning)\n";
    std::cout << "----------------------------------------------------------\n";

    // Test session hash distribution
    const char* sessions[][2] = {
        {"SENDER1", "TARGET1"},
        {"SENDER2", "TARGET2"},
        {"CLIENT_A", "EXCHANGE"},
        {"BROKER_1", "CLEARING"},
    };

    auto config = CpuAffinityConfig::default_config();
    SessionCoreMapper mapper(config);

    std::cout << "Available cores for FIX: ";
    for (size_t i = 0; i < config.allowed_cores.size(); ++i) {
        if (i > 0) std::cout << ",";
        std::cout << config.allowed_cores[i];
    }
    std::cout << "\n\n";

    std::cout << std::setw(25) << "Session"
              << std::setw(15) << "Assigned Core\n";
    std::cout << std::string(40, '-') << "\n";

    for (const auto& session : sessions) {
        int core = mapper.core_for_session(session[0], session[1]);
        std::cout << std::setw(12) << session[0] << " <-> " << std::setw(10) << session[1]
                  << std::setw(15) << core << "\n";
    }

    // ========================================================================
    // Summary
    // ========================================================================

    std::cout << "\n==========================================================\n";
    std::cout << "  Summary\n";
    std::cout << "==========================================================\n\n";

    double p99_improvement = (unpinned_avg.p99_ns - pinned_avg.p99_ns) / unpinned_avg.p99_ns * 100;
    double p999_improvement = (unpinned_avg.p999_ns - pinned_avg.p999_ns) / unpinned_avg.p999_ns * 100;
    double max_improvement = (unpinned_avg.max_ns - pinned_avg.max_ns) / unpinned_avg.max_ns * 100;
    double stddev_improvement = (unpinned_avg.stddev_ns - pinned_avg.stddev_ns) / unpinned_avg.stddev_ns * 100;

    std::cout << "Latency Comparison:\n";
    std::cout << "  Metric       Unpinned      Pinned      Improvement\n";
    std::cout << "  ------------------------------------------------\n";
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "  Mean:        " << std::setw(8) << unpinned_avg.mean_ns << " ns   "
              << std::setw(8) << pinned_avg.mean_ns << " ns   "
              << std::setprecision(1) << std::showpos
              << (unpinned_avg.mean_ns - pinned_avg.mean_ns) / unpinned_avg.mean_ns * 100 << "%\n";
    std::cout << std::noshowpos;
    std::cout << "  P99:         " << std::setw(8) << unpinned_avg.p99_ns << " ns   "
              << std::setw(8) << pinned_avg.p99_ns << " ns   "
              << std::showpos << p99_improvement << "%\n";
    std::cout << std::noshowpos;
    std::cout << "  P99.9:       " << std::setw(8) << unpinned_avg.p999_ns << " ns   "
              << std::setw(8) << pinned_avg.p999_ns << " ns   "
              << std::showpos << p999_improvement << "%\n";
    std::cout << std::noshowpos;
    std::cout << "  Max:         " << std::setw(8) << unpinned_avg.max_ns << " ns   "
              << std::setw(8) << pinned_avg.max_ns << " ns   "
              << std::showpos << max_improvement << "%\n";
    std::cout << std::noshowpos;
    std::cout << "  StdDev:      " << std::setw(8) << unpinned_avg.stddev_ns << " ns   "
              << std::setw(8) << pinned_avg.stddev_ns << " ns   "
              << std::showpos << stddev_improvement << "%\n";

    std::cout << std::noshowpos << "\nWhy CPU Core Pinning Reduces Latency:\n";
    std::cout << "  - Eliminates OS scheduler migrations between cores\n";
    std::cout << "  - Keeps L1/L2 cache hot (no cold cache on migration)\n";
    std::cout << "  - Prevents NUMA cross-node memory access\n";
    std::cout << "  - Reduces interrupt interference (isolate from IRQ cores)\n";

    std::cout << "\n==========================================================\n";

    return 0;
}
