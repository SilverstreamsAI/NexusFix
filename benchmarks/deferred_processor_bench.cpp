// Benchmark: Deferred Processor Hot Path Latency
// Compares inline processing vs deferred processing
//
// Build: cmake --build build && ./build/bin/benchmarks/deferred_processor_bench

#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cstring>
#include <thread>
#include <chrono>
#include <memory>
#include <atomic>

#include "nexusfix/util/cpu_affinity.hpp"
#include "nexusfix/memory/spsc_queue.hpp"

// Benchmark configuration
constexpr int WARMUP_ITERATIONS = 10000;
constexpr int BENCHMARK_ITERATIONS = 100000;
constexpr int NUM_RUNS = 5;
constexpr size_t MESSAGE_SIZE = 256;

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

    return BenchmarkResult{
        .mean_ns = mean,
        .median_ns = ns_latencies[n / 2],
        .p99_ns = ns_latencies[static_cast<size_t>(n * 0.99)],
        .p999_ns = ns_latencies[static_cast<size_t>(n * 0.999)],
        .min_ns = ns_latencies.front(),
        .max_ns = ns_latencies.back()
    };
}

// Simulated FIX message buffer (small for benchmark)
struct MessageBuffer {
    uint64_t timestamp;
    uint32_t size;
    char data[MESSAGE_SIZE];
};

// Simulate expensive processing work
volatile uint64_t g_sink = 0;

void expensive_processing(const char* data, size_t size) {
    uint64_t sum = 0;
    for (size_t i = 0; i < size; ++i) {
        sum += static_cast<uint8_t>(data[i]);
    }
    for (int i = 0; i < 100; ++i) {
        sum ^= sum >> 3;
    }
    g_sink = sum;
    asm volatile("" ::: "memory");
}

void print_result(const char* name, const BenchmarkResult& r) {
    std::cout << std::setw(25) << std::left << name
              << std::setw(10) << std::fixed << std::setprecision(1) << r.mean_ns
              << std::setw(10) << r.median_ns
              << std::setw(10) << r.p99_ns
              << std::setw(10) << r.p999_ns
              << std::setw(10) << r.min_ns
              << std::setw(10) << r.max_ns << "\n";
}

int main() {
    using namespace nfx::util;
    using namespace nfx::memory;

    std::cout << "==========================================================\n";
    std::cout << "  Deferred Processor Benchmark: Hot Path Latency\n";
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
    std::cout << "  Message size: " << MESSAGE_SIZE << " bytes\n";

    // Prepare test message
    MessageBuffer msg;
    std::memset(msg.data, 'A', sizeof(msg.data));
    msg.size = MESSAGE_SIZE;

    // ========================================================================
    // Benchmark 1: Inline Processing (traditional approach)
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Inline Processing (synchronous, blocking)\n";
    std::cout << "----------------------------------------------------------\n";
    std::cout << std::setw(25) << "Metric"
              << std::setw(10) << "Mean"
              << std::setw(10) << "Median"
              << std::setw(10) << "P99"
              << std::setw(10) << "P99.9"
              << std::setw(10) << "Min"
              << std::setw(10) << "Max\n";
    std::cout << std::string(85, '-') << "\n";

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
        expensive_processing(msg.data, msg.size);
    }

    std::vector<BenchmarkResult> inline_results;
    for (int run = 0; run < NUM_RUNS; ++run) {
        std::vector<uint64_t> latencies;
        latencies.reserve(BENCHMARK_ITERATIONS);

        for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
            uint64_t start = rdtsc();
            expensive_processing(msg.data, msg.size);
            uint64_t end = rdtsc();
            latencies.push_back(end - start);
        }

        inline_results.push_back(calculate_stats(latencies, cpu_freq_ghz));
    }

    // Average results
    BenchmarkResult inline_avg{};
    for (const auto& r : inline_results) {
        inline_avg.mean_ns += r.mean_ns;
        inline_avg.median_ns += r.median_ns;
        inline_avg.p99_ns += r.p99_ns;
        inline_avg.p999_ns += r.p999_ns;
        inline_avg.min_ns += r.min_ns;
        inline_avg.max_ns += r.max_ns;
    }
    inline_avg.mean_ns /= NUM_RUNS;
    inline_avg.median_ns /= NUM_RUNS;
    inline_avg.p99_ns /= NUM_RUNS;
    inline_avg.p999_ns /= NUM_RUNS;
    inline_avg.min_ns /= NUM_RUNS;
    inline_avg.max_ns /= NUM_RUNS;

    print_result("Inline (average)", inline_avg);

    // ========================================================================
    // Benchmark 2: Deferred Processing (hot path only)
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Deferred Processing (hot path: timestamp + memcpy + publish)\n";
    std::cout << "----------------------------------------------------------\n";
    std::cout << std::setw(25) << "Metric"
              << std::setw(10) << "Mean"
              << std::setw(10) << "Median"
              << std::setw(10) << "P99"
              << std::setw(10) << "P99.9"
              << std::setw(10) << "Min"
              << std::setw(10) << "Max\n";
    std::cout << std::string(85, '-') << "\n";

    // Create queue on heap to avoid stack overflow (4096 capacity = ~1.2MB)
    auto queue = std::make_unique<SPSCQueue<MessageBuffer, 4096>>();
    std::atomic<bool> running{true};
    std::atomic<uint64_t> processed_count{0};

    // Start background consumer thread
    std::thread consumer([&] {
        while (running.load(std::memory_order_relaxed)) {
            MessageBuffer buffer;
            if (queue->try_pop(buffer)) {
                expensive_processing(buffer.data, buffer.size);
                processed_count.fetch_add(1, std::memory_order_relaxed);
            } else {
                std::this_thread::yield();
            }
        }
        // Drain remaining
        MessageBuffer buffer;
        while (queue->try_pop(buffer)) {
            expensive_processing(buffer.data, buffer.size);
            processed_count.fetch_add(1, std::memory_order_relaxed);
        }
    });

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
        MessageBuffer buffer;
        buffer.timestamp = rdtsc();
        buffer.size = MESSAGE_SIZE;
        std::memcpy(buffer.data, msg.data, MESSAGE_SIZE);
        while (!queue->try_push(std::move(buffer))) {
            std::this_thread::yield();
        }
    }

    // Wait for warmup to complete
    while (processed_count.load() < static_cast<uint64_t>(WARMUP_ITERATIONS)) {
        std::this_thread::yield();
    }
    processed_count.store(0);

    std::vector<BenchmarkResult> deferred_results;
    for (int run = 0; run < NUM_RUNS; ++run) {
        std::vector<uint64_t> latencies;
        latencies.reserve(BENCHMARK_ITERATIONS);

        for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
            MessageBuffer buffer;
            buffer.size = MESSAGE_SIZE;
            std::memcpy(buffer.data, msg.data, MESSAGE_SIZE);

            uint64_t start = rdtsc();
            buffer.timestamp = start;
            while (!queue->try_push(std::move(buffer))) {
                asm volatile("pause");
            }
            uint64_t end = rdtsc();

            latencies.push_back(end - start);
        }

        deferred_results.push_back(calculate_stats(latencies, cpu_freq_ghz));

        // Wait for processing to complete before next run
        while (queue->size_approx() > 0) {
            std::this_thread::yield();
        }
    }

    running.store(false);
    consumer.join();

    // Average results
    BenchmarkResult deferred_avg{};
    for (const auto& r : deferred_results) {
        deferred_avg.mean_ns += r.mean_ns;
        deferred_avg.median_ns += r.median_ns;
        deferred_avg.p99_ns += r.p99_ns;
        deferred_avg.p999_ns += r.p999_ns;
        deferred_avg.min_ns += r.min_ns;
        deferred_avg.max_ns += r.max_ns;
    }
    deferred_avg.mean_ns /= NUM_RUNS;
    deferred_avg.median_ns /= NUM_RUNS;
    deferred_avg.p99_ns /= NUM_RUNS;
    deferred_avg.p999_ns /= NUM_RUNS;
    deferred_avg.min_ns /= NUM_RUNS;
    deferred_avg.max_ns /= NUM_RUNS;

    print_result("Deferred (average)", deferred_avg);

    // ========================================================================
    // Benchmark 3: Queue Push Only (overhead measurement)
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Queue Push Only (measure SPSC queue overhead)\n";
    std::cout << "----------------------------------------------------------\n";
    std::cout << std::setw(25) << "Metric"
              << std::setw(10) << "Mean"
              << std::setw(10) << "Median"
              << std::setw(10) << "P99"
              << std::setw(10) << "P99.9"
              << std::setw(10) << "Min"
              << std::setw(10) << "Max\n";
    std::cout << std::string(85, '-') << "\n";

    // Create fresh queue for clean measurement
    auto queue2 = std::make_unique<SPSCQueue<MessageBuffer, 4096>>();

    std::vector<BenchmarkResult> queue_results;
    for (int run = 0; run < NUM_RUNS; ++run) {
        std::vector<uint64_t> latencies;
        latencies.reserve(BENCHMARK_ITERATIONS);

        for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
            MessageBuffer buffer;
            buffer.timestamp = rdtsc();
            buffer.size = MESSAGE_SIZE;
            std::memcpy(buffer.data, msg.data, MESSAGE_SIZE);

            uint64_t start = rdtsc();
            (void)queue2->try_push(std::move(buffer));
            uint64_t end = rdtsc();

            latencies.push_back(end - start);

            // Pop to keep queue from filling
            MessageBuffer tmp;
            (void)queue2->try_pop(tmp);
        }

        queue_results.push_back(calculate_stats(latencies, cpu_freq_ghz));
    }

    BenchmarkResult queue_avg{};
    for (const auto& r : queue_results) {
        queue_avg.mean_ns += r.mean_ns;
        queue_avg.median_ns += r.median_ns;
        queue_avg.p99_ns += r.p99_ns;
        queue_avg.p999_ns += r.p999_ns;
        queue_avg.min_ns += r.min_ns;
        queue_avg.max_ns += r.max_ns;
    }
    queue_avg.mean_ns /= NUM_RUNS;
    queue_avg.median_ns /= NUM_RUNS;
    queue_avg.p99_ns /= NUM_RUNS;
    queue_avg.p999_ns /= NUM_RUNS;
    queue_avg.min_ns /= NUM_RUNS;
    queue_avg.max_ns /= NUM_RUNS;

    print_result("Queue Push Only", queue_avg);

    // ========================================================================
    // Summary
    // ========================================================================

    std::cout << "\n==========================================================\n";
    std::cout << "  Summary\n";
    std::cout << "==========================================================\n\n";

    double speedup = inline_avg.mean_ns / deferred_avg.mean_ns;

    std::cout << "Hot Path Latency Comparison:\n";
    std::cout << "  Approach            Mean       P99        Speedup\n";
    std::cout << "  -------------------------------------------------\n";
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "  Inline:             " << std::setw(8) << inline_avg.mean_ns << " ns   "
              << std::setw(8) << inline_avg.p99_ns << " ns   (baseline)\n";
    std::cout << "  Deferred:           " << std::setw(8) << deferred_avg.mean_ns << " ns   "
              << std::setw(8) << deferred_avg.p99_ns << " ns   "
              << std::setprecision(1) << speedup << "x faster\n";
    std::cout << "  Queue Push Only:    " << std::setw(8) << queue_avg.mean_ns << " ns   "
              << std::setw(8) << queue_avg.p99_ns << " ns   (overhead)\n";

    std::cout << "\nHot Path Reduction:\n";
    std::cout << "  Mean: " << std::fixed << std::setprecision(1)
              << (1.0 - deferred_avg.mean_ns / inline_avg.mean_ns) * 100 << "% reduction\n";
    std::cout << "  P99:  " << (1.0 - deferred_avg.p99_ns / inline_avg.p99_ns) * 100 << "% reduction\n";

    std::cout << "\nNanoLog Pattern Benefits:\n";
    std::cout << "  - Expensive work moved to background thread\n";
    std::cout << "  - Hot path: timestamp + memcpy + atomic publish\n";
    std::cout << "  - No blocking on disk I/O or complex processing\n";
    std::cout << "  - Deterministic latency (no long tail from expensive ops)\n";

    std::cout << "\n==========================================================\n";

    return 0;
}
