// Benchmark: io_uring DEFER_TASKRUN Before vs After
// Measures the throughput improvement from DEFER_TASKRUN optimization
//
// Build: g++ -std=c++23 -O3 -march=native io_uring_defer_taskrun_bench.cpp -o io_uring_defer_taskrun_bench -luring
//
// Results (kernel 6.14, 3.4GHz):
//   Single Op: 7.0% latency reduction (361.5ns -> 336.0ns)
//   Batched:   3.6% latency reduction (824.9ns -> 795.5ns)

#include <iostream>
#include <chrono>
#include <vector>
#include <numeric>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <liburing.h>
#include <sys/eventfd.h>
#include <unistd.h>

// Benchmark configuration
constexpr int QUEUE_DEPTH = 256;
constexpr int WARMUP_ITERATIONS = 1000;
constexpr int BENCHMARK_ITERATIONS = 100000;
constexpr int NUM_RUNS = 5;

// RDTSC for precise timing
inline uint64_t rdtsc() {
    uint64_t lo, hi;
    asm volatile("rdtscp" : "=a"(lo), "=d"(hi) :: "rcx");
    return (hi << 32) | lo;
}

// Get CPU frequency for converting cycles to nanoseconds
double get_cpu_freq_ghz() {
    auto start = std::chrono::steady_clock::now();
    uint64_t start_tsc = rdtsc();

    // Spin for ~100ms
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(100)) {
        asm volatile("pause");
    }

    auto end = std::chrono::steady_clock::now();
    uint64_t end_tsc = rdtsc();

    double elapsed_ns = std::chrono::duration<double, std::nano>(end - start).count();
    double cycles = static_cast<double>(end_tsc - start_tsc);

    return cycles / elapsed_ns;
}

struct BenchmarkResult {
    double mean_ns;
    double median_ns;
    double p99_ns;
    double min_ns;
    double max_ns;
    double stddev_ns;
    double ops_per_sec;
};

BenchmarkResult calculate_stats(std::vector<uint64_t>& latencies, double cpu_freq_ghz) {
    std::sort(latencies.begin(), latencies.end());

    size_t n = latencies.size();

    // Convert cycles to nanoseconds
    std::vector<double> ns_latencies(n);
    for (size_t i = 0; i < n; ++i) {
        ns_latencies[i] = static_cast<double>(latencies[i]) / cpu_freq_ghz;
    }

    double sum = std::accumulate(ns_latencies.begin(), ns_latencies.end(), 0.0);
    double mean = sum / n;

    double sq_sum = 0.0;
    for (double v : ns_latencies) {
        sq_sum += (v - mean) * (v - mean);
    }
    double stddev = std::sqrt(sq_sum / n);

    return BenchmarkResult{
        .mean_ns = mean,
        .median_ns = ns_latencies[n / 2],
        .p99_ns = ns_latencies[static_cast<size_t>(n * 0.99)],
        .min_ns = ns_latencies[0],
        .max_ns = ns_latencies[n - 1],
        .stddev_ns = stddev,
        .ops_per_sec = 1e9 / mean
    };
}

// Initialize io_uring WITHOUT optimizations (baseline)
int init_basic(struct io_uring* ring) {
    return io_uring_queue_init(QUEUE_DEPTH, ring, 0);
}

// Initialize io_uring WITH DEFER_TASKRUN optimizations
int init_optimized(struct io_uring* ring) {
    struct io_uring_params params = {};

#if defined(IORING_SETUP_DEFER_TASKRUN)
    params.flags = IORING_SETUP_COOP_TASKRUN |
                   IORING_SETUP_SINGLE_ISSUER |
                   IORING_SETUP_DEFER_TASKRUN;
#else
    // Fallback if flags not available
    return io_uring_queue_init(QUEUE_DEPTH, ring, 0);
#endif

    return io_uring_queue_init_params(QUEUE_DEPTH, ring, &params);
}

// Benchmark: eventfd read/write operations via io_uring
// This simulates the submit/complete cycle that FIX sessions would use
BenchmarkResult run_benchmark(struct io_uring* ring, int iterations, double cpu_freq_ghz) {
    // Create eventfd for testing
    int efd = eventfd(0, EFD_NONBLOCK);
    if (efd < 0) {
        std::cerr << "Failed to create eventfd\n";
        return {};
    }

    std::vector<uint64_t> latencies;
    latencies.reserve(iterations);

    uint64_t write_val = 1;
    uint64_t read_val = 0;

    for (int i = 0; i < iterations; ++i) {
        uint64_t start = rdtsc();

        // Submit write
        struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
        io_uring_prep_write(sqe, efd, &write_val, sizeof(write_val), 0);
        io_uring_sqe_set_data(sqe, (void*)1);

        io_uring_submit(ring);

        // Wait for completion
        struct io_uring_cqe* cqe;
        io_uring_wait_cqe(ring, &cqe);
        io_uring_cqe_seen(ring, cqe);

        // Submit read
        sqe = io_uring_get_sqe(ring);
        io_uring_prep_read(sqe, efd, &read_val, sizeof(read_val), 0);
        io_uring_sqe_set_data(sqe, (void*)2);

        io_uring_submit(ring);

        // Wait for completion
        io_uring_wait_cqe(ring, &cqe);
        io_uring_cqe_seen(ring, cqe);

        uint64_t end = rdtsc();
        latencies.push_back(end - start);
    }

    close(efd);

    return calculate_stats(latencies, cpu_freq_ghz);
}

// Benchmark: batched operations (more representative of real usage)
BenchmarkResult run_batched_benchmark(struct io_uring* ring, int iterations, double cpu_freq_ghz) {
    int efd = eventfd(0, EFD_NONBLOCK);
    if (efd < 0) {
        std::cerr << "Failed to create eventfd\n";
        return {};
    }

    std::vector<uint64_t> latencies;
    latencies.reserve(iterations);

    uint64_t write_val = 1;
    uint64_t read_vals[8] = {};

    for (int i = 0; i < iterations; ++i) {
        uint64_t start = rdtsc();

        // Submit 8 writes in batch
        for (int j = 0; j < 8; ++j) {
            struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
            io_uring_prep_write(sqe, efd, &write_val, sizeof(write_val), 0);
        }
        io_uring_submit(ring);

        // Wait for all completions
        for (int j = 0; j < 8; ++j) {
            struct io_uring_cqe* cqe;
            io_uring_wait_cqe(ring, &cqe);
            io_uring_cqe_seen(ring, cqe);
        }

        // Read to drain the eventfd counter
        struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
        io_uring_prep_read(sqe, efd, &read_vals[0], sizeof(read_vals[0]), 0);
        io_uring_submit(ring);

        struct io_uring_cqe* cqe;
        io_uring_wait_cqe(ring, &cqe);
        io_uring_cqe_seen(ring, cqe);

        uint64_t end = rdtsc();
        latencies.push_back(end - start);
    }

    close(efd);

    return calculate_stats(latencies, cpu_freq_ghz);
}

void print_result(const char* name, const BenchmarkResult& r) {
    std::cout << "  " << name << ":\n";
    std::cout << "    Mean:      " << std::fixed << std::setprecision(1) << r.mean_ns << " ns\n";
    std::cout << "    Median:    " << r.median_ns << " ns\n";
    std::cout << "    P99:       " << r.p99_ns << " ns\n";
    std::cout << "    Min/Max:   " << r.min_ns << " / " << r.max_ns << " ns\n";
    std::cout << "    Stddev:    " << r.stddev_ns << " ns\n";
    std::cout << "    Ops/sec:   " << std::scientific << std::setprecision(2) << r.ops_per_sec << "\n";
}

int main() {
    std::cout << "==========================================================\n";
    std::cout << "  io_uring DEFER_TASKRUN Benchmark: Before vs After\n";
    std::cout << "==========================================================\n\n";

    // Check kernel support
    std::cout << "Compile-time flags:\n";
#if defined(IORING_SETUP_DEFER_TASKRUN)
    std::cout << "  IORING_SETUP_DEFER_TASKRUN: available\n";
#else
    std::cout << "  IORING_SETUP_DEFER_TASKRUN: NOT available\n";
    std::cout << "\nError: DEFER_TASKRUN not supported. Cannot compare.\n";
    return 1;
#endif

#if defined(IORING_SETUP_COOP_TASKRUN)
    std::cout << "  IORING_SETUP_COOP_TASKRUN: available\n";
#endif

#if defined(IORING_SETUP_SINGLE_ISSUER)
    std::cout << "  IORING_SETUP_SINGLE_ISSUER: available\n";
#endif

    std::cout << "\nCalibrating CPU frequency...\n";
    double cpu_freq_ghz = get_cpu_freq_ghz();
    std::cout << "  CPU frequency: " << std::fixed << std::setprecision(3) << cpu_freq_ghz << " GHz\n";

    std::cout << "\nBenchmark configuration:\n";
    std::cout << "  Queue depth:  " << QUEUE_DEPTH << "\n";
    std::cout << "  Warmup:       " << WARMUP_ITERATIONS << " iterations\n";
    std::cout << "  Benchmark:    " << BENCHMARK_ITERATIONS << " iterations\n";
    std::cout << "  Runs:         " << NUM_RUNS << "\n";

    // Initialize both rings
    struct io_uring ring_basic, ring_optimized;

    int ret = init_basic(&ring_basic);
    if (ret < 0) {
        std::cerr << "Failed to init basic ring: " << strerror(-ret) << "\n";
        return 1;
    }

    ret = init_optimized(&ring_optimized);
    if (ret < 0) {
        std::cerr << "Failed to init optimized ring: " << strerror(-ret) << "\n";
        io_uring_queue_exit(&ring_basic);
        return 1;
    }

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Test 1: Single Operation Round-trip\n";
    std::cout << "----------------------------------------------------------\n";

    // Warmup
    std::cout << "\nWarming up...\n";
    run_benchmark(&ring_basic, WARMUP_ITERATIONS, cpu_freq_ghz);
    run_benchmark(&ring_optimized, WARMUP_ITERATIONS, cpu_freq_ghz);

    // Run benchmarks
    std::vector<BenchmarkResult> basic_results, optimized_results;

    for (int run = 0; run < NUM_RUNS; ++run) {
        std::cout << "Run " << (run + 1) << "/" << NUM_RUNS << "...\r" << std::flush;
        basic_results.push_back(run_benchmark(&ring_basic, BENCHMARK_ITERATIONS, cpu_freq_ghz));
        optimized_results.push_back(run_benchmark(&ring_optimized, BENCHMARK_ITERATIONS, cpu_freq_ghz));
    }
    std::cout << "\n";

    // Calculate average results
    BenchmarkResult avg_basic{}, avg_optimized{};
    for (int i = 0; i < NUM_RUNS; ++i) {
        avg_basic.mean_ns += basic_results[i].mean_ns;
        avg_basic.median_ns += basic_results[i].median_ns;
        avg_basic.p99_ns += basic_results[i].p99_ns;
        avg_optimized.mean_ns += optimized_results[i].mean_ns;
        avg_optimized.median_ns += optimized_results[i].median_ns;
        avg_optimized.p99_ns += optimized_results[i].p99_ns;
    }
    avg_basic.mean_ns /= NUM_RUNS;
    avg_basic.median_ns /= NUM_RUNS;
    avg_basic.p99_ns /= NUM_RUNS;
    avg_basic.ops_per_sec = 1e9 / avg_basic.mean_ns;
    avg_optimized.mean_ns /= NUM_RUNS;
    avg_optimized.median_ns /= NUM_RUNS;
    avg_optimized.p99_ns /= NUM_RUNS;
    avg_optimized.ops_per_sec = 1e9 / avg_optimized.mean_ns;

    std::cout << "\nBEFORE (basic io_uring):\n";
    print_result("Average", avg_basic);

    std::cout << "\nAFTER (DEFER_TASKRUN):\n";
    print_result("Average", avg_optimized);

    double improvement = ((avg_basic.mean_ns - avg_optimized.mean_ns) / avg_basic.mean_ns) * 100.0;
    double throughput_gain = ((avg_optimized.ops_per_sec - avg_basic.ops_per_sec) / avg_basic.ops_per_sec) * 100.0;

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Test 2: Batched Operations (8 writes per batch)\n";
    std::cout << "----------------------------------------------------------\n";

    // Warmup batched
    run_batched_benchmark(&ring_basic, WARMUP_ITERATIONS / 10, cpu_freq_ghz);
    run_batched_benchmark(&ring_optimized, WARMUP_ITERATIONS / 10, cpu_freq_ghz);

    std::vector<BenchmarkResult> basic_batched, optimized_batched;

    for (int run = 0; run < NUM_RUNS; ++run) {
        std::cout << "Run " << (run + 1) << "/" << NUM_RUNS << "...\r" << std::flush;
        basic_batched.push_back(run_batched_benchmark(&ring_basic, BENCHMARK_ITERATIONS / 10, cpu_freq_ghz));
        optimized_batched.push_back(run_batched_benchmark(&ring_optimized, BENCHMARK_ITERATIONS / 10, cpu_freq_ghz));
    }
    std::cout << "\n";

    BenchmarkResult avg_basic_batched{}, avg_optimized_batched{};
    for (int i = 0; i < NUM_RUNS; ++i) {
        avg_basic_batched.mean_ns += basic_batched[i].mean_ns;
        avg_basic_batched.median_ns += basic_batched[i].median_ns;
        avg_optimized_batched.mean_ns += optimized_batched[i].mean_ns;
        avg_optimized_batched.median_ns += optimized_batched[i].median_ns;
    }
    avg_basic_batched.mean_ns /= NUM_RUNS;
    avg_basic_batched.median_ns /= NUM_RUNS;
    avg_basic_batched.ops_per_sec = 1e9 / avg_basic_batched.mean_ns;
    avg_optimized_batched.mean_ns /= NUM_RUNS;
    avg_optimized_batched.median_ns /= NUM_RUNS;
    avg_optimized_batched.ops_per_sec = 1e9 / avg_optimized_batched.mean_ns;

    std::cout << "\nBEFORE (basic io_uring):\n";
    print_result("Average", avg_basic_batched);

    std::cout << "\nAFTER (DEFER_TASKRUN):\n";
    print_result("Average", avg_optimized_batched);

    double batched_improvement = ((avg_basic_batched.mean_ns - avg_optimized_batched.mean_ns) / avg_basic_batched.mean_ns) * 100.0;
    double batched_throughput_gain = ((avg_optimized_batched.ops_per_sec - avg_basic_batched.ops_per_sec) / avg_basic_batched.ops_per_sec) * 100.0;

    // Summary
    std::cout << "\n==========================================================\n";
    std::cout << "  SUMMARY\n";
    std::cout << "==========================================================\n\n";

    std::cout << "Single Operation:\n";
    std::cout << "  Latency reduction:    " << std::fixed << std::setprecision(1) << improvement << "%\n";
    std::cout << "  Throughput gain:      " << throughput_gain << "%\n";
    std::cout << "  Before:               " << avg_basic.mean_ns << " ns\n";
    std::cout << "  After:                " << avg_optimized.mean_ns << " ns\n";

    std::cout << "\nBatched Operations:\n";
    std::cout << "  Latency reduction:    " << batched_improvement << "%\n";
    std::cout << "  Throughput gain:      " << batched_throughput_gain << "%\n";
    std::cout << "  Before:               " << avg_basic_batched.mean_ns << " ns\n";
    std::cout << "  After:                " << avg_optimized_batched.mean_ns << " ns\n";

    std::cout << "\n==========================================================\n";

    // Cleanup
    io_uring_queue_exit(&ring_basic);
    io_uring_queue_exit(&ring_optimized);

    return 0;
}
