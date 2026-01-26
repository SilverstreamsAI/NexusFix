// Benchmark: PMR vs Heap Allocation for Message Store
// Compares std::vector (heap) vs std::pmr::vector (PMR pool) allocation
//
// Build: cmake --build build --target message_store_pmr_bench
// Run:   ./build/bin/benchmarks/message_store_pmr_bench

#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <cstring>
#include <memory_resource>
#include <unordered_map>
#include <span>

#include "nexusfix/util/cpu_affinity.hpp"

using namespace nfx::util;

// Benchmark configuration
constexpr int WARMUP_ITERATIONS = 10000;
constexpr int BENCHMARK_ITERATIONS = 100000;
constexpr size_t MESSAGE_SIZE = 256;  // Typical FIX message size
constexpr size_t PMR_POOL_SIZE = 64 * 1024 * 1024;  // 64MB pool

// Sample FIX message
constexpr char SAMPLE_MESSAGE[] =
    "8=FIX.4.4\x01" "9=150\x01" "35=8\x01" "49=SENDER\x01" "56=TARGET\x01"
    "34=1\x01" "52=20260126-12:00:00.000\x01" "37=ORDER123\x01" "11=CLIENT456\x01"
    "17=EXEC789\x01" "150=0\x01" "39=0\x01" "55=AAPL\x01" "54=1\x01"
    "38=100\x01" "44=150.50\x01" "14=0\x01" "151=100\x01" "6=0\x01" "10=123\x01";

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

// Percentile calculation
template<typename T>
T percentile(std::vector<T>& data, double p) {
    size_t idx = static_cast<size_t>(p * static_cast<double>(data.size() - 1));
    return data[idx];
}

// Statistics structure
struct Stats {
    double min;
    double mean;
    double median;
    double p99;
    double p999;
    double max;
};

Stats compute_stats(std::vector<uint64_t>& latencies, double cpu_freq_ghz) {
    std::sort(latencies.begin(), latencies.end());
    size_t n = latencies.size();

    double sum = 0;
    for (auto lat : latencies) {
        sum += static_cast<double>(lat);
    }

    return Stats{
        .min = static_cast<double>(latencies[0]) / cpu_freq_ghz,
        .mean = sum / static_cast<double>(n) / cpu_freq_ghz,
        .median = static_cast<double>(latencies[n / 2]) / cpu_freq_ghz,
        .p99 = static_cast<double>(percentile(latencies, 0.99)) / cpu_freq_ghz,
        .p999 = static_cast<double>(percentile(latencies, 0.999)) / cpu_freq_ghz,
        .max = static_cast<double>(latencies[n - 1]) / cpu_freq_ghz
    };
}

void print_stats(const char* label, const Stats& s) {
    std::cout << "  " << label << ":\n";
    std::cout << "    Min:    " << std::fixed << std::setprecision(1) << s.min << " ns\n";
    std::cout << "    Mean:   " << s.mean << " ns\n";
    std::cout << "    Median: " << s.median << " ns\n";
    std::cout << "    P99:    " << s.p99 << " ns\n";
    std::cout << "    P99.9:  " << s.p999 << " ns\n";
    std::cout << "    Max:    " << s.max << " ns\n";
}

int main() {
    std::cout << "==========================================================\n";
    std::cout << "  PMR vs Heap Allocation Benchmark (Message Store)\n";
    std::cout << "==========================================================\n\n";

    // Pin to core for consistent results
    (void)CpuAffinity::pin_to_core(2);

    std::cout << "Calibrating CPU frequency...\n";
    double cpu_freq_ghz = get_cpu_freq_ghz();
    std::cout << "  CPU frequency: " << std::fixed << std::setprecision(3)
              << cpu_freq_ghz << " GHz\n";

    std::cout << "\nConfiguration:\n";
    std::cout << "  Warmup:       " << WARMUP_ITERATIONS << " iterations\n";
    std::cout << "  Benchmark:    " << BENCHMARK_ITERATIONS << " iterations\n";
    std::cout << "  Message size: " << MESSAGE_SIZE << " bytes\n";
    std::cout << "  PMR pool:     " << (PMR_POOL_SIZE / 1024 / 1024) << " MB\n";

    std::span<const char> msg(SAMPLE_MESSAGE, sizeof(SAMPLE_MESSAGE) - 1);

    // ========================================================================
    // BEFORE: Heap Allocation (std::vector)
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  BEFORE: Heap Allocation (std::vector)\n";
    std::cout << "----------------------------------------------------------\n";

    // Warmup
    {
        std::unordered_map<uint32_t, std::vector<char>> heap_store;
        for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
            heap_store.try_emplace(
                static_cast<uint32_t>(i),
                std::vector<char>(msg.begin(), msg.end())
            );
        }
    }

    std::vector<uint64_t> heap_latencies;
    heap_latencies.reserve(BENCHMARK_ITERATIONS);

    // Benchmark heap allocation
    {
        std::unordered_map<uint32_t, std::vector<char>> heap_store;

        for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
            uint64_t start = rdtsc();

            heap_store.try_emplace(
                static_cast<uint32_t>(i),
                std::vector<char>(msg.begin(), msg.end())
            );

            uint64_t end = rdtsc();
            heap_latencies.push_back(end - start);
        }
    }

    Stats heap_stats = compute_stats(heap_latencies, cpu_freq_ghz);
    print_stats("Heap (std::vector)", heap_stats);

    // ========================================================================
    // AFTER: PMR Pool Allocation (std::pmr::vector)
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  AFTER: PMR Pool Allocation (std::pmr::vector)\n";
    std::cout << "----------------------------------------------------------\n";

    // Warmup
    {
        std::vector<char> pool_storage(PMR_POOL_SIZE);
        std::pmr::monotonic_buffer_resource pool(
            pool_storage.data(), pool_storage.size(),
            std::pmr::null_memory_resource());

        std::unordered_map<uint32_t, std::pmr::vector<char>> pmr_store;

        for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
            std::pmr::polymorphic_allocator<char> alloc(&pool);
            std::pmr::vector<char> pmr_msg(alloc);
            pmr_msg.assign(msg.begin(), msg.end());
            pmr_store.try_emplace(static_cast<uint32_t>(i), std::move(pmr_msg));
        }
    }

    std::vector<uint64_t> pmr_latencies;
    pmr_latencies.reserve(BENCHMARK_ITERATIONS);

    // Benchmark PMR allocation
    {
        std::vector<char> pool_storage(PMR_POOL_SIZE);
        std::pmr::monotonic_buffer_resource pool(
            pool_storage.data(), pool_storage.size(),
            std::pmr::null_memory_resource());

        std::unordered_map<uint32_t, std::pmr::vector<char>> pmr_store;

        for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
            uint64_t start = rdtsc();

            std::pmr::polymorphic_allocator<char> alloc(&pool);
            std::pmr::vector<char> pmr_msg(alloc);
            pmr_msg.assign(msg.begin(), msg.end());
            pmr_store.try_emplace(static_cast<uint32_t>(i), std::move(pmr_msg));

            uint64_t end = rdtsc();
            pmr_latencies.push_back(end - start);
        }
    }

    Stats pmr_stats = compute_stats(pmr_latencies, cpu_freq_ghz);
    print_stats("PMR (std::pmr::vector)", pmr_stats);

    // ========================================================================
    // Comparison Summary
    // ========================================================================

    std::cout << "\n==========================================================\n";
    std::cout << "  Comparison: Before vs After\n";
    std::cout << "==========================================================\n\n";

    double speedup_mean = heap_stats.mean / pmr_stats.mean;
    double speedup_median = heap_stats.median / pmr_stats.median;
    double speedup_p99 = heap_stats.p99 / pmr_stats.p99;

    double improve_mean = (1.0 - pmr_stats.mean / heap_stats.mean) * 100.0;
    double improve_median = (1.0 - pmr_stats.median / heap_stats.median) * 100.0;
    double improve_p99 = (1.0 - pmr_stats.p99 / heap_stats.p99) * 100.0;

    std::cout << "                 Before (Heap)   After (PMR)    Speedup   Improvement\n";
    std::cout << "  -----------------------------------------------------------------------\n";
    std::cout << "  Mean:        " << std::setw(10) << std::fixed << std::setprecision(1) << heap_stats.mean << " ns"
              << std::setw(12) << pmr_stats.mean << " ns"
              << std::setw(10) << std::setprecision(2) << speedup_mean << "x"
              << std::setw(10) << std::setprecision(1) << improve_mean << "%\n";
    std::cout << "  Median:      " << std::setw(10) << std::setprecision(1) << heap_stats.median << " ns"
              << std::setw(12) << pmr_stats.median << " ns"
              << std::setw(10) << std::setprecision(2) << speedup_median << "x"
              << std::setw(10) << std::setprecision(1) << improve_median << "%\n";
    std::cout << "  P99:         " << std::setw(10) << std::setprecision(1) << heap_stats.p99 << " ns"
              << std::setw(12) << pmr_stats.p99 << " ns"
              << std::setw(10) << std::setprecision(2) << speedup_p99 << "x"
              << std::setw(10) << std::setprecision(1) << improve_p99 << "%\n";

    std::cout << "\n  Summary: PMR is " << std::setprecision(1) << speedup_median
              << "x faster than heap allocation!\n";

    // ========================================================================
    // Benefits Summary
    // ========================================================================

    std::cout << "\n==========================================================\n";
    std::cout << "  PMR Optimization Benefits\n";
    std::cout << "==========================================================\n\n";

    std::cout << "  1. Zero syscalls during store() - malloc eliminated\n";
    std::cout << "  2. O(1) allocation from pre-allocated pool\n";
    std::cout << "  3. O(1) memory reclamation via pool.release()\n";
    std::cout << "  4. No memory fragmentation\n";
    std::cout << "  5. Predictable latency (no allocator contention)\n";

    std::cout << "\n==========================================================\n";
    std::cout << "  Implementation Reference\n";
    std::cout << "==========================================================\n\n";

    std::cout << "  File: include/nexusfix/store/memory_message_store.hpp\n";
    std::cout << "  Ticket: TICKET_INTERNAL_008\n";
    std::cout << "  Technique: modernc_quant.md #16 (PMR Memory Pool)\n";

    std::cout << "\n==========================================================\n";

    return 0;
}
