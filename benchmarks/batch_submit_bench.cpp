// Benchmark: Batched io_uring Submissions
// Compares single vs batched submission overhead
//
// Build: cmake --build build && ./build/bin/benchmarks/batch_submit_bench

#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <numeric>
#include <chrono>

#include "nexusfix/transport/batch_submitter.hpp"
#include "nexusfix/util/cpu_affinity.hpp"

#if !NFX_IO_URING_AVAILABLE

int main() {
    std::cout << "io_uring not available on this system.\n";
    return 0;
}

#else

using namespace nfx;
using namespace nfx::util;

// Benchmark configuration
constexpr int WARMUP_ITERATIONS = 1000;
constexpr int BENCHMARK_ITERATIONS = 10000;
constexpr size_t BATCH_SIZES[] = {1, 4, 8, 16, 32};

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

int main() {
    std::cout << "==========================================================\n";
    std::cout << "  Batched io_uring Submission Benchmark\n";
    std::cout << "==========================================================\n\n";

    // Pin to core for consistent results
    (void)CpuAffinity::pin_to_core(2);

    std::cout << "Calibrating CPU frequency...\n";
    double cpu_freq_ghz = get_cpu_freq_ghz();
    std::cout << "  CPU frequency: " << std::fixed << std::setprecision(3)
              << cpu_freq_ghz << " GHz\n";

    // Initialize io_uring
    IoUringContext ctx;
    auto init_result = ctx.init(256);
    if (!init_result.has_value()) {
        std::cerr << "Failed to initialize io_uring\n";
        return 1;
    }

    std::cout << "\nio_uring Status:\n";
    std::cout << "  Initialized: Yes\n";
    std::cout << "  DEFER_TASKRUN: " << (ctx.is_optimized() ? "Enabled" : "Disabled") << "\n";

    std::cout << "\nConfiguration:\n";
    std::cout << "  Warmup:     " << WARMUP_ITERATIONS << " iterations\n";
    std::cout << "  Benchmark:  " << BENCHMARK_ITERATIONS << " iterations\n";

    // ========================================================================
    // Single Submit Benchmark (NOP operations)
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Single Submit (1 NOP per syscall)\n";
    std::cout << "----------------------------------------------------------\n";

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
        auto* sqe = ctx.get_sqe();
        if (sqe) {
            io_uring_prep_nop(sqe);
            ctx.submit();

            struct io_uring_cqe* cqe;
            ctx.wait(&cqe);
            ctx.seen(cqe);
        }
    }

    std::vector<uint64_t> single_latencies;
    single_latencies.reserve(BENCHMARK_ITERATIONS);

    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        auto* sqe = ctx.get_sqe();
        if (!sqe) continue;

        io_uring_prep_nop(sqe);

        uint64_t start = rdtsc();
        ctx.submit();
        struct io_uring_cqe* cqe;
        ctx.wait(&cqe);
        uint64_t end = rdtsc();

        ctx.seen(cqe);
        single_latencies.push_back(end - start);
    }

    std::sort(single_latencies.begin(), single_latencies.end());
    double single_median = static_cast<double>(single_latencies[single_latencies.size() / 2]) / cpu_freq_ghz;
    double single_mean = static_cast<double>(std::accumulate(single_latencies.begin(), single_latencies.end(), 0ULL)) /
                         static_cast<double>(single_latencies.size()) / cpu_freq_ghz;

    std::cout << "  Mean:   " << std::fixed << std::setprecision(1) << single_mean << " ns/submit\n";
    std::cout << "  Median: " << single_median << " ns/submit\n";

    // ========================================================================
    // Batched Submit Benchmark
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Batched Submit (multiple NOPs per syscall)\n";
    std::cout << "----------------------------------------------------------\n";
    std::cout << "  Batch     Total     Per-Op    Speedup\n";

    double baseline_per_op = single_median;

    for (size_t batch_size : BATCH_SIZES) {
        // Warmup
        for (int i = 0; i < WARMUP_ITERATIONS / static_cast<int>(batch_size); ++i) {
            for (size_t j = 0; j < batch_size; ++j) {
                auto* sqe = ctx.get_sqe();
                if (sqe) io_uring_prep_nop(sqe);
            }
            ctx.submit();

            for (size_t j = 0; j < batch_size; ++j) {
                struct io_uring_cqe* cqe;
                ctx.wait(&cqe);
                ctx.seen(cqe);
            }
        }

        std::vector<uint64_t> batch_latencies;
        batch_latencies.reserve(BENCHMARK_ITERATIONS);

        for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
            // Queue batch_size operations
            for (size_t j = 0; j < batch_size; ++j) {
                auto* sqe = ctx.get_sqe();
                if (sqe) io_uring_prep_nop(sqe);
            }

            uint64_t start = rdtsc();
            ctx.submit();

            // Wait for all completions
            for (size_t j = 0; j < batch_size; ++j) {
                struct io_uring_cqe* cqe;
                ctx.wait(&cqe);
                ctx.seen(cqe);
            }
            uint64_t end = rdtsc();

            batch_latencies.push_back(end - start);
        }

        std::sort(batch_latencies.begin(), batch_latencies.end());
        double batch_median = static_cast<double>(batch_latencies[batch_latencies.size() / 2]) / cpu_freq_ghz;
        double per_op = batch_median / static_cast<double>(batch_size);
        double speedup = baseline_per_op / per_op;

        std::cout << "  " << std::setw(5) << batch_size
                  << std::setw(10) << std::fixed << std::setprecision(1) << batch_median << " ns"
                  << std::setw(10) << per_op << " ns"
                  << std::setw(10) << std::setprecision(1) << speedup << "x\n";
    }

    // ========================================================================
    // BatchSubmitter API Benchmark
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  BatchSubmitter API Overhead\n";
    std::cout << "----------------------------------------------------------\n";

    BatchSubmitter<32> batch(ctx);

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
        for (int j = 0; j < 8; ++j) {
            (void)batch.queue_nop();
        }
        (void)batch.submit();

        for (int j = 0; j < 8; ++j) {
            struct io_uring_cqe* cqe;
            ctx.wait(&cqe);
            ctx.seen(cqe);
        }
    }

    std::vector<uint64_t> api_latencies;
    api_latencies.reserve(BENCHMARK_ITERATIONS);

    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        uint64_t start = rdtsc();

        // Queue 8 operations
        for (int j = 0; j < 8; ++j) {
            (void)batch.queue_nop();
        }
        (void)batch.submit();

        uint64_t end = rdtsc();

        // Drain completions
        for (int j = 0; j < 8; ++j) {
            struct io_uring_cqe* cqe;
            ctx.wait(&cqe);
            ctx.seen(cqe);
        }

        api_latencies.push_back(end - start);
    }

    std::sort(api_latencies.begin(), api_latencies.end());
    double api_median = static_cast<double>(api_latencies[api_latencies.size() / 2]) / cpu_freq_ghz;
    double api_per_op = api_median / 8.0;

    std::cout << "  8-op batch: " << std::fixed << std::setprecision(1) << api_median << " ns total\n";
    std::cout << "  Per-op:     " << api_per_op << " ns\n";
    std::cout << "  Speedup:    " << std::setprecision(1) << (baseline_per_op / api_per_op) << "x vs single\n";

    // ========================================================================
    // Summary
    // ========================================================================

    std::cout << "\n==========================================================\n";
    std::cout << "  Summary\n";
    std::cout << "==========================================================\n\n";

    std::cout << "Components Added:\n";
    std::cout << "  1. BatchSubmitter<N> - Queue N operations, single submit\n";
    std::cout << "  2. AutoFlushBatchSubmitter - Auto-flush when full\n";
    std::cout << "  3. ScatterGatherSend - Multiple buffers, single writev\n";
    std::cout << "  4. LinkedOperations - Chain dependent operations\n";

    std::cout << "\nKey Benefits:\n";
    std::cout << "  - Amortized syscall overhead across batch\n";
    std::cout << "  - Better CPU cache utilization\n";
    std::cout << "  - Reduced context switches\n";
    std::cout << "  - Works with DEFER_TASKRUN for additional gains\n";

    std::cout << "\nUsage:\n";
    std::cout << "  BatchSubmitter<32> batch(ctx);\n";
    std::cout << "  batch.queue_send(fd1, data1);\n";
    std::cout << "  batch.queue_send(fd2, data2);\n";
    std::cout << "  batch.queue_send(fd3, data3);\n";
    std::cout << "  batch.submit();  // Single syscall for all 3\n";

    std::cout << "\n==========================================================\n";

    return 0;
}

#endif // NFX_IO_URING_AVAILABLE
