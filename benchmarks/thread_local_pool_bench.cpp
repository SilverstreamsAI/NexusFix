// Benchmark: Thread-Local Object Pool Performance
// Compares pool allocation vs heap allocation
//
// Build: cmake --build build && ./build/bin/benchmarks/thread_local_pool_bench

#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <cstring>

#include "nexusfix/util/thread_local_pool.hpp"
#include "nexusfix/util/cpu_affinity.hpp"

using namespace nfx::util;

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

// Percentile calculation
template<typename T>
T percentile(std::vector<T>& data, double p) {
    size_t idx = static_cast<size_t>(p * static_cast<double>(data.size() - 1));
    return data[idx];
}

int main() {
    std::cout << "==========================================================\n";
    std::cout << "  Thread-Local Object Pool Benchmark\n";
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
    std::cout << "  Runs:         " << NUM_RUNS << "\n";
    std::cout << "  Buffer size:  " << MessageBuffer::MAX_SIZE << " bytes\n";
    std::cout << "  Pool capacity:" << MessageBufferPool::capacity() << " buffers\n";

    // ========================================================================
    // Warmup
    // ========================================================================

    std::cout << "\nWarming up...\n";
    for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
        auto buf = PooledMessageBuffer::acquire();
        if (buf) {
            buf->set("test", 4);
        }
    }

    // ========================================================================
    // Pool Allocation Benchmark
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Pool Allocation (acquire + release)\n";
    std::cout << "----------------------------------------------------------\n";

    std::vector<uint64_t> pool_latencies;
    pool_latencies.reserve(BENCHMARK_ITERATIONS);

    auto& pool = MessageBufferPool::instance();
    pool.reset_stats();

    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        uint64_t start = rdtsc();
        MessageBuffer* buf = pool.acquire();
        if (buf) {
            buf->set("8=FIX.4.4|9=100|35=D|", 21);  // Simulate message copy
            pool.release(buf);
        }
        uint64_t end = rdtsc();
        pool_latencies.push_back(end - start);
    }

    std::sort(pool_latencies.begin(), pool_latencies.end());

    double pool_median = static_cast<double>(pool_latencies[BENCHMARK_ITERATIONS / 2]) / cpu_freq_ghz;
    double pool_mean = static_cast<double>(std::accumulate(pool_latencies.begin(), pool_latencies.end(), 0ULL)) /
                       static_cast<double>(BENCHMARK_ITERATIONS) / cpu_freq_ghz;
    double pool_p99 = static_cast<double>(percentile(pool_latencies, 0.99)) / cpu_freq_ghz;
    double pool_p999 = static_cast<double>(percentile(pool_latencies, 0.999)) / cpu_freq_ghz;

    std::cout << "  Mean:   " << std::fixed << std::setprecision(1) << pool_mean << " ns\n";
    std::cout << "  Median: " << pool_median << " ns\n";
    std::cout << "  P99:    " << pool_p99 << " ns\n";
    std::cout << "  P99.9:  " << pool_p999 << " ns\n";

    auto stats = pool.stats();
    std::cout << "\n  Pool stats:\n";
    std::cout << "    Acquires:   " << stats.acquires << "\n";
    std::cout << "    Releases:   " << stats.releases << "\n";
    std::cout << "    Exhausted:  " << stats.pool_exhausted << "\n";

    // ========================================================================
    // Heap Allocation Benchmark
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Heap Allocation (new + delete)\n";
    std::cout << "----------------------------------------------------------\n";

    std::vector<uint64_t> heap_latencies;
    heap_latencies.reserve(BENCHMARK_ITERATIONS);

    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        uint64_t start = rdtsc();
        auto* buf = new MessageBuffer{};
        buf->set("8=FIX.4.4|9=100|35=D|", 21);
        delete buf;
        uint64_t end = rdtsc();
        heap_latencies.push_back(end - start);
    }

    std::sort(heap_latencies.begin(), heap_latencies.end());

    double heap_median = static_cast<double>(heap_latencies[BENCHMARK_ITERATIONS / 2]) / cpu_freq_ghz;
    double heap_mean = static_cast<double>(std::accumulate(heap_latencies.begin(), heap_latencies.end(), 0ULL)) /
                       static_cast<double>(BENCHMARK_ITERATIONS) / cpu_freq_ghz;
    double heap_p99 = static_cast<double>(percentile(heap_latencies, 0.99)) / cpu_freq_ghz;
    double heap_p999 = static_cast<double>(percentile(heap_latencies, 0.999)) / cpu_freq_ghz;

    std::cout << "  Mean:   " << std::fixed << std::setprecision(1) << heap_mean << " ns\n";
    std::cout << "  Median: " << heap_median << " ns\n";
    std::cout << "  P99:    " << heap_p99 << " ns\n";
    std::cout << "  P99.9:  " << heap_p999 << " ns\n";

    // ========================================================================
    // PooledPtr RAII Benchmark
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  PooledPtr RAII (auto acquire/release)\n";
    std::cout << "----------------------------------------------------------\n";

    std::vector<uint64_t> raii_latencies;
    raii_latencies.reserve(BENCHMARK_ITERATIONS);

    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        uint64_t start = rdtsc();
        {
            auto buf = PooledMessageBuffer::acquire();
            if (buf) {
                buf->set("8=FIX.4.4|9=100|35=D|", 21);
            }
        }  // Auto-release here
        uint64_t end = rdtsc();
        raii_latencies.push_back(end - start);
    }

    std::sort(raii_latencies.begin(), raii_latencies.end());

    double raii_median = static_cast<double>(raii_latencies[BENCHMARK_ITERATIONS / 2]) / cpu_freq_ghz;
    double raii_mean = static_cast<double>(std::accumulate(raii_latencies.begin(), raii_latencies.end(), 0ULL)) /
                       static_cast<double>(BENCHMARK_ITERATIONS) / cpu_freq_ghz;
    double raii_p99 = static_cast<double>(percentile(raii_latencies, 0.99)) / cpu_freq_ghz;

    std::cout << "  Mean:   " << std::fixed << std::setprecision(1) << raii_mean << " ns\n";
    std::cout << "  Median: " << raii_median << " ns\n";
    std::cout << "  P99:    " << raii_p99 << " ns\n";

    // ========================================================================
    // Comparison Summary
    // ========================================================================

    std::cout << "\n==========================================================\n";
    std::cout << "  Comparison Summary\n";
    std::cout << "==========================================================\n\n";

    double speedup_median = heap_median / pool_median;
    double speedup_mean = heap_mean / pool_mean;
    double speedup_p99 = heap_p99 / pool_p99;

    std::cout << "                    Pool        Heap        Speedup\n";
    std::cout << "  --------------------------------------------------------\n";
    std::cout << "  Mean:        " << std::setw(8) << std::fixed << std::setprecision(1) << pool_mean << " ns"
              << std::setw(10) << heap_mean << " ns"
              << std::setw(10) << std::setprecision(1) << speedup_mean << "x\n";
    std::cout << "  Median:      " << std::setw(8) << pool_median << " ns"
              << std::setw(10) << heap_median << " ns"
              << std::setw(10) << speedup_median << "x\n";
    std::cout << "  P99:         " << std::setw(8) << pool_p99 << " ns"
              << std::setw(10) << heap_p99 << " ns"
              << std::setw(10) << speedup_p99 << "x\n";

    std::cout << "\n  Pool allocation is " << std::setprecision(0) << speedup_median
              << "x faster than heap allocation!\n";

    // ========================================================================
    // Large Buffer Pool
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Large Buffer Pool (64KB buffers)\n";
    std::cout << "----------------------------------------------------------\n";

    std::vector<uint64_t> large_latencies;
    large_latencies.reserve(10000);

    for (int i = 0; i < 10000; ++i) {
        uint64_t start = rdtsc();
        auto buf = PooledLargeBuffer::acquire();
        if (buf) {
            buf->set("test", 4);
        }
        uint64_t end = rdtsc();
        large_latencies.push_back(end - start);
    }

    std::sort(large_latencies.begin(), large_latencies.end());
    double large_median = static_cast<double>(large_latencies[5000]) / cpu_freq_ghz;

    std::cout << "  Median: " << std::fixed << std::setprecision(1) << large_median << " ns\n";
    std::cout << "  Capacity: " << LargeBufferPool::capacity() << " buffers\n";

    // ========================================================================
    // Summary
    // ========================================================================

    std::cout << "\n==========================================================\n";
    std::cout << "  Implementation Summary\n";
    std::cout << "==========================================================\n\n";

    std::cout << "Components Added:\n";
    std::cout << "  1. ThreadLocalPool<T, Capacity> - Generic pool template\n";
    std::cout << "  2. PooledPtr<T> - RAII wrapper with auto-release\n";
    std::cout << "  3. MessageBuffer - 4KB aligned buffer for FIX messages\n";
    std::cout << "  4. LargeBuffer - 64KB buffer for batch operations\n";
    std::cout << "  5. MessageBufferPool / LargeBufferPool type aliases\n";

    std::cout << "\nKey Features:\n";
    std::cout << "  - Zero contention (thread-local)\n";
    std::cout << "  - Cache-line aligned objects\n";
    std::cout << "  - Lock-free acquire/release\n";
    std::cout << "  - Automatic heap fallback when exhausted\n";
    std::cout << "  - RAII support with PooledPtr\n";

    std::cout << "\nUsage:\n";
    std::cout << "  // Direct pool access:\n";
    std::cout << "  auto& pool = MessageBufferPool::instance();\n";
    std::cout << "  MessageBuffer* buf = pool.acquire();\n";
    std::cout << "  buf->set(data, len);\n";
    std::cout << "  pool.release(buf);\n";
    std::cout << "\n  // RAII wrapper:\n";
    std::cout << "  auto buf = PooledMessageBuffer::acquire();\n";
    std::cout << "  buf->set(data, len);\n";
    std::cout << "  // Auto-released on scope exit\n";

    std::cout << "\n==========================================================\n";

    return 0;
}
