// Benchmark: C++23 Ranges Utilities Performance
// Compares nfx::util ranges wrappers vs manual implementations
//
// Build: cmake --build build --target ranges_utils_bench
// Run:   ./build/bin/benchmarks/ranges_utils_bench

#include <vector>
#include <numeric>
#include <iostream>
#include <iomanip>

#include "include/benchmark_utils.hpp"
#include "nexusfix/util/ranges_utils.hpp"
#include "nexusfix/util/cpu_affinity.hpp"

using namespace nfx::bench;

// Benchmark configuration
constexpr int WARMUP_ITERATIONS = 10000;
constexpr int BENCHMARK_ITERATIONS = 100000;
constexpr size_t CONTAINER_SIZE = 1000;

int main() {
    std::cout << "==========================================================\n";
    std::cout << "  C++23 Ranges Utilities Benchmark\n";
    std::cout << "==========================================================\n\n";

    // Pin to core for consistent results
    (void)bind_to_core(2);

    std::cout << "Calibrating CPU frequency (busy-wait)...\n";
    double cpu_freq_ghz = estimate_cpu_freq_ghz_busy();
    std::cout << "  CPU frequency: " << std::fixed << std::setprecision(3)
              << cpu_freq_ghz << " GHz\n";

    // Feature detection status
    std::cout << "\nC++23 Feature Detection:\n";
#if defined(__cpp_lib_ranges_contains) && __cpp_lib_ranges_contains >= 202207L
    std::cout << "  ranges::contains:  NATIVE (C++23)\n";
#else
    std::cout << "  ranges::contains:  FALLBACK (find != end)\n";
#endif
#if defined(__cpp_lib_ranges_enumerate) && __cpp_lib_ranges_enumerate >= 202302L
    std::cout << "  views::enumerate:  NATIVE (C++23)\n";
#else
    std::cout << "  views::enumerate:  FALLBACK (zip + iota)\n";
#endif
#if defined(__cpp_lib_ranges_chunk) && __cpp_lib_ranges_chunk >= 202202L
    std::cout << "  views::chunk:      NATIVE (C++23)\n";
#else
    std::cout << "  views::chunk:      FALLBACK\n";
#endif

    std::cout << "\nConfiguration:\n";
    std::cout << "  Container size: " << CONTAINER_SIZE << " elements\n";
    std::cout << "  Warmup:         " << WARMUP_ITERATIONS << " iterations\n";
    std::cout << "  Benchmark:      " << BENCHMARK_ITERATIONS << " iterations\n";

    // Prepare test data
    std::vector<int> data(CONTAINER_SIZE);
    std::iota(data.begin(), data.end(), 1);  // 1, 2, 3, ..., 1000

    // Warm up caches
    warmup_dcache(data.data(), data.size() * sizeof(int));

    // ========================================================================
    // Benchmark 1: contains() vs manual find
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Benchmark 1: contains() vs manual find\n";
    std::cout << "----------------------------------------------------------\n";

    int search_value = 500;  // Middle of container
    volatile bool result = false;

    // Warmup
    warmup_icache([&]() {
        result = nfx::util::contains(data, search_value);
        result = std::find(data.begin(), data.end(), search_value) != data.end();
    }, WARMUP_ITERATIONS);

    // Manual find
    std::vector<uint64_t> manual_cycles;
    manual_cycles.reserve(BENCHMARK_ITERATIONS);
    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        uint64_t cycles;
        {
            ScopedTimer timer(cycles);
            result = std::find(data.begin(), data.end(), search_value) != data.end();
        }
        compiler_barrier();
        manual_cycles.push_back(cycles);
    }
    LatencyStats manual_stats;
    manual_stats.compute(manual_cycles, cpu_freq_ghz);

    // nfx::util::contains
    std::vector<uint64_t> contains_cycles;
    contains_cycles.reserve(BENCHMARK_ITERATIONS);
    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        uint64_t cycles;
        {
            ScopedTimer timer(cycles);
            result = nfx::util::contains(data, search_value);
        }
        compiler_barrier();
        contains_cycles.push_back(cycles);
    }
    LatencyStats contains_stats;
    contains_stats.compute(contains_cycles, cpu_freq_ghz);

    std::cout << "  Manual find() != end():  " << std::fixed << std::setprecision(1)
              << manual_stats.mean_ns << " ns (P99: " << manual_stats.p99_ns << " ns)\n";
    std::cout << "  nfx::util::contains():   " << contains_stats.mean_ns
              << " ns (P99: " << contains_stats.p99_ns << " ns)\n";

    // ========================================================================
    // Benchmark 2: enumerate() iteration
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Benchmark 2: enumerate() vs manual index loop\n";
    std::cout << "----------------------------------------------------------\n";

    volatile size_t sum = 0;

    // Manual index loop
    std::vector<uint64_t> manual_enum_cycles;
    manual_enum_cycles.reserve(BENCHMARK_ITERATIONS);
    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        uint64_t cycles;
        {
            ScopedTimer timer(cycles);
            size_t local_sum = 0;
            for (size_t j = 0; j < data.size(); ++j) {
                local_sum += j + static_cast<size_t>(data[j]);
            }
            sum = local_sum;
        }
        compiler_barrier();
        manual_enum_cycles.push_back(cycles);
    }
    LatencyStats manual_enum_stats;
    manual_enum_stats.compute(manual_enum_cycles, cpu_freq_ghz);

    // nfx::util::enumerate
    std::vector<uint64_t> enumerate_cycles;
    enumerate_cycles.reserve(BENCHMARK_ITERATIONS);
    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        uint64_t cycles;
        {
            ScopedTimer timer(cycles);
            size_t local_sum = 0;
            for (auto [idx, val] : nfx::util::enumerate(data)) {
                local_sum += idx + static_cast<size_t>(val);
            }
            sum = local_sum;
        }
        compiler_barrier();
        enumerate_cycles.push_back(cycles);
    }
    LatencyStats enumerate_stats;
    enumerate_stats.compute(enumerate_cycles, cpu_freq_ghz);

    std::cout << "  Manual for (i < size):   " << std::fixed << std::setprecision(1)
              << manual_enum_stats.mean_ns << " ns (P99: " << manual_enum_stats.p99_ns << " ns)\n";
    std::cout << "  nfx::util::enumerate():  " << enumerate_stats.mean_ns
              << " ns (P99: " << enumerate_stats.p99_ns << " ns)\n";

    // ========================================================================
    // Benchmark 3: chunk() iteration
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Benchmark 3: chunk() batch processing\n";
    std::cout << "----------------------------------------------------------\n";

    constexpr size_t CHUNK_SIZE = 10;

    // Manual chunking
    std::vector<uint64_t> manual_chunk_cycles;
    manual_chunk_cycles.reserve(BENCHMARK_ITERATIONS);
    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        uint64_t cycles;
        {
            ScopedTimer timer(cycles);
            size_t local_sum = 0;
            for (size_t j = 0; j < data.size(); j += CHUNK_SIZE) {
                size_t chunk_end = std::min(j + CHUNK_SIZE, data.size());
                for (size_t k = j; k < chunk_end; ++k) {
                    local_sum += static_cast<size_t>(data[k]);
                }
            }
            sum = local_sum;
        }
        compiler_barrier();
        manual_chunk_cycles.push_back(cycles);
    }
    LatencyStats manual_chunk_stats;
    manual_chunk_stats.compute(manual_chunk_cycles, cpu_freq_ghz);

    // std::views::chunk
    std::vector<uint64_t> chunk_cycles;
    chunk_cycles.reserve(BENCHMARK_ITERATIONS);
    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        uint64_t cycles;
        {
            ScopedTimer timer(cycles);
            size_t local_sum = 0;
            for (auto chunk : data | std::views::chunk(CHUNK_SIZE)) {
                for (auto val : chunk) {
                    local_sum += static_cast<size_t>(val);
                }
            }
            sum = local_sum;
        }
        compiler_barrier();
        chunk_cycles.push_back(cycles);
    }
    LatencyStats chunk_stats;
    chunk_stats.compute(chunk_cycles, cpu_freq_ghz);

    std::cout << "  Manual for (j += CHUNK):  " << std::fixed << std::setprecision(1)
              << manual_chunk_stats.mean_ns << " ns (P99: " << manual_chunk_stats.p99_ns << " ns)\n";
    std::cout << "  std::views::chunk():      " << chunk_stats.mean_ns
              << " ns (P99: " << chunk_stats.p99_ns << " ns)\n";

    // ========================================================================
    // Summary
    // ========================================================================

    std::cout << "\n==========================================================\n";
    std::cout << "  Summary: Manual vs C++23 Ranges\n";
    std::cout << "==========================================================\n\n";

    print_comparison_header("Manual", "Ranges");
    print_comparison("contains()", manual_stats, contains_stats);
    print_comparison("enumerate()", manual_enum_stats, enumerate_stats);
    print_comparison("chunk()", manual_chunk_stats, chunk_stats);

    std::cout << "\nConclusion:\n";
    std::cout << "  - enumerate(): Zero overhead, safe for all code\n";
    std::cout << "  - contains()/chunk(): Some overhead, use in non-hot paths\n";
    std::cout << "  - Keep manual loops for hot paths where every cycle matters\n";

    std::cout << "\n==========================================================\n";

    (void)sum;  // Suppress unused warning
    (void)result;
    return 0;
}
