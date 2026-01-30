/*
    MPSC Queue Benchmark

    Compares:
    - SPSC Queue (baseline)
    - MPSC Queue with sequence array
    - SimpleMPSC Queue with claim-publish

    Scenarios:
    - Single producer (compare overhead vs SPSC)
    - 2 producers
    - 4 producers
    - 8 producers

    Metrics:
    - Throughput (messages/second)
    - Latency (ns per operation)
*/

#include <nexusfix/memory/spsc_queue.hpp>
#include <nexusfix/memory/mpsc_queue.hpp>
#include <nexusfix/memory/wait_strategy.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>
#include <numeric>
#include <algorithm>

using namespace nfx::memory;
using namespace std::chrono;

// ============================================================================
// Test Configuration
// ============================================================================

constexpr size_t QUEUE_CAPACITY = 4096;  // Smaller for stack safety
constexpr size_t MESSAGES_PER_PRODUCER = 100'000;  // Reduced for faster testing
constexpr size_t WARMUP_ITERATIONS = 1'000;

// Test payload
struct TestMessage {
    uint64_t sequence;
    uint64_t timestamp;
    uint64_t producer_id;
    uint64_t payload[5];
};

static_assert(sizeof(TestMessage) == 64, "TestMessage should be cache-line sized");

// ============================================================================
// Timing Utilities
// ============================================================================

inline uint64_t rdtsc() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
    uint64_t lo, hi;
    asm volatile("lfence; rdtsc; lfence" : "=a"(lo), "=d"(hi));
    return (hi << 32) | lo;
#else
    return static_cast<uint64_t>(
        steady_clock::now().time_since_epoch().count());
#endif
}

double get_cpu_freq_ghz() {
    constexpr int calibration_ms = 100;
    auto start_time = steady_clock::now();
    uint64_t start_tsc = rdtsc();

    std::this_thread::sleep_for(milliseconds(calibration_ms));

    uint64_t end_tsc = rdtsc();
    auto end_time = steady_clock::now();

    auto elapsed_ns = duration_cast<nanoseconds>(end_time - start_time).count();
    double freq = static_cast<double>(end_tsc - start_tsc) /
                  static_cast<double>(elapsed_ns);
    return freq;
}

// ============================================================================
// SPSC Benchmark
// ============================================================================

struct SPSCResult {
    double throughput_mps;  // messages per second (millions)
    double latency_ns;      // nanoseconds per operation
    uint64_t total_messages;
};

SPSCResult benchmark_spsc(size_t num_messages) {
    auto queue_ptr = std::make_unique<SPSCQueue<TestMessage, QUEUE_CAPACITY>>();
    auto& queue = *queue_ptr;
    std::atomic<bool> done{false};
    std::atomic<uint64_t> consumed{0};

    // Consumer thread - starts immediately
    std::thread consumer([&]() {
        TestMessage msg;
        uint64_t count = 0;
        while (!done.load(std::memory_order_acquire) || !queue.empty()) {
            if (queue.try_pop(msg)) {
                ++count;
            } else {
                _mm_pause();
            }
        }
        consumed.store(count, std::memory_order_release);
    });

    // Warmup - consumer is already running
    for (size_t i = 0; i < WARMUP_ITERATIONS; ++i) {
        TestMessage msg{i, rdtsc(), 0, {}};
        while (!queue.try_push(msg)) {
            _mm_pause();
        }
    }
    // Wait for warmup to be consumed
    while (!queue.empty()) {
        std::this_thread::yield();
    }

    // Start benchmark
    auto start_time = steady_clock::now();

    // Producer (main thread)
    for (size_t i = 0; i < num_messages; ++i) {
        TestMessage msg{i, rdtsc(), 0, {}};
        while (!queue.try_push(msg)) {
            _mm_pause();
        }
    }

    auto end_time = steady_clock::now();
    done.store(true, std::memory_order_release);
    consumer.join();

    auto elapsed = duration_cast<nanoseconds>(end_time - start_time);
    double elapsed_sec = elapsed.count() / 1e9;

    return {
        .throughput_mps = static_cast<double>(num_messages) / elapsed_sec / 1e6,
        .latency_ns = static_cast<double>(elapsed.count()) / num_messages,
        .total_messages = consumed.load()
    };
}

// ============================================================================
// MPSC Benchmark
// ============================================================================

struct MPSCResult {
    double throughput_mps;
    double latency_ns;
    uint64_t total_messages;
    size_t num_producers;
};

template<typename QueueT>
MPSCResult benchmark_mpsc(size_t num_producers, size_t messages_per_producer) {
    auto queue_ptr = std::make_unique<QueueT>();
    auto& queue = *queue_ptr;
    std::atomic<bool> start{false};
    std::atomic<uint64_t> consumed{0};
    std::atomic<size_t> producers_done{0};

    // Consumer thread - starts processing immediately when start=true
    std::thread consumer([&]() {
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        TestMessage msg;
        uint64_t count = 0;
        while (producers_done.load(std::memory_order_acquire) < num_producers ||
               !queue.empty()) {
            if (queue.try_pop(msg)) {
                ++count;
            } else {
                _mm_pause();
            }
        }
        consumed.store(count, std::memory_order_release);
    });

    // Producer threads
    std::vector<std::thread> producers;
    producers.reserve(num_producers);

    for (size_t p = 0; p < num_producers; ++p) {
        producers.emplace_back([&, p]() {
            // Wait for start signal
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            // Produce messages
            for (size_t i = 0; i < messages_per_producer; ++i) {
                TestMessage msg{i, rdtsc(), p, {}};
                while (!queue.try_push(msg)) {
                    _mm_pause();
                }
            }

            producers_done.fetch_add(1, std::memory_order_release);
        });
    }

    // Let threads start up
    std::this_thread::sleep_for(milliseconds(1));

    // Start benchmark - all threads begin simultaneously
    auto start_time = steady_clock::now();
    start.store(true, std::memory_order_release);

    // Wait for all producers
    for (auto& t : producers) {
        t.join();
    }

    auto end_time = steady_clock::now();
    consumer.join();

    auto elapsed = duration_cast<nanoseconds>(end_time - start_time);
    double elapsed_sec = elapsed.count() / 1e9;
    size_t total_messages = num_producers * messages_per_producer;

    return {
        .throughput_mps = static_cast<double>(total_messages) / elapsed_sec / 1e6,
        .latency_ns = static_cast<double>(elapsed.count()) / total_messages,
        .total_messages = consumed.load(),
        .num_producers = num_producers
    };
}

// ============================================================================
// Main
// ============================================================================

void print_separator() {
    std::cout << std::string(70, '=') << "\n";
}

void print_header(const char* title) {
    std::cout << "\n";
    print_separator();
    std::cout << "  " << title << "\n";
    print_separator();
}

int main() {
    std::cout << std::fixed << std::setprecision(2);

    print_header("MPSC Queue Benchmark");

    // CPU info
    double cpu_freq = get_cpu_freq_ghz();
    std::cout << "\nCPU Frequency: " << cpu_freq << " GHz\n";
    std::cout << "Queue Capacity: " << QUEUE_CAPACITY << " entries\n";
    std::cout << "Message Size: " << sizeof(TestMessage) << " bytes\n";
    std::cout << "Messages per Producer: " << MESSAGES_PER_PRODUCER / 1'000'000.0
              << "M\n\n";

    // SPSC Baseline
    print_header("SPSC Queue (Baseline)");
    std::cout << "\nRunning SPSC benchmark...\n";

    auto spsc_result = benchmark_spsc(MESSAGES_PER_PRODUCER);
    std::cout << "\nResults:\n";
    std::cout << "  Throughput: " << spsc_result.throughput_mps << " M msg/s\n";
    std::cout << "  Latency:    " << spsc_result.latency_ns << " ns/op\n";
    std::cout << "  Messages:   " << spsc_result.total_messages << " consumed\n";

    // MPSC with sequence array
    print_header("MPSC Queue (Sequence Array)");

    using MPSCQueueType = MPSCQueue<TestMessage, QUEUE_CAPACITY, BusySpinWait>;

    std::vector<size_t> producer_counts = {1, 2, 4, 8};
    std::vector<MPSCResult> mpsc_results;

    for (size_t num_producers : producer_counts) {
        std::cout << "\nRunning with " << num_producers << " producer(s)...\n";

        auto result = benchmark_mpsc<MPSCQueueType>(
            num_producers,
            MESSAGES_PER_PRODUCER / num_producers);

        mpsc_results.push_back(result);

        std::cout << "  Throughput: " << result.throughput_mps << " M msg/s\n";
        std::cout << "  Latency:    " << result.latency_ns << " ns/op\n";
        std::cout << "  Messages:   " << result.total_messages << " consumed\n";
    }

    // SimpleMPSC with claim-publish
    print_header("SimpleMPSC Queue (Claim-Publish)");

    using SimpleMPSCType = SimpleMPSCQueue<TestMessage, QUEUE_CAPACITY, BusySpinWait>;
    std::vector<MPSCResult> simple_results;

    for (size_t num_producers : producer_counts) {
        std::cout << "\nRunning with " << num_producers << " producer(s)...\n";

        auto result = benchmark_mpsc<SimpleMPSCType>(
            num_producers,
            MESSAGES_PER_PRODUCER / num_producers);

        simple_results.push_back(result);

        std::cout << "  Throughput: " << result.throughput_mps << " M msg/s\n";
        std::cout << "  Latency:    " << result.latency_ns << " ns/op\n";
        std::cout << "  Messages:   " << result.total_messages << " consumed\n";
    }

    // Summary comparison
    print_header("Summary Comparison");

    std::cout << "\n";
    std::cout << std::left << std::setw(25) << "Configuration"
              << std::right << std::setw(15) << "Throughput"
              << std::setw(15) << "Latency"
              << std::setw(15) << "vs SPSC" << "\n";
    std::cout << std::string(70, '-') << "\n";

    // SPSC baseline
    std::cout << std::left << std::setw(25) << "SPSC (baseline)"
              << std::right << std::setw(12) << spsc_result.throughput_mps << " M/s"
              << std::setw(12) << spsc_result.latency_ns << " ns"
              << std::setw(15) << "---" << "\n";

    // MPSC results
    for (size_t i = 0; i < mpsc_results.size(); ++i) {
        const auto& r = mpsc_results[i];
        double ratio = r.throughput_mps / spsc_result.throughput_mps;
        std::string label = "MPSC " + std::to_string(r.num_producers) + "P";

        std::cout << std::left << std::setw(25) << label
                  << std::right << std::setw(12) << r.throughput_mps << " M/s"
                  << std::setw(12) << r.latency_ns << " ns"
                  << std::setw(12) << ratio << "x" << "\n";
    }

    std::cout << std::string(70, '-') << "\n";

    // SimpleMPSC results
    for (size_t i = 0; i < simple_results.size(); ++i) {
        const auto& r = simple_results[i];
        double ratio = r.throughput_mps / spsc_result.throughput_mps;
        std::string label = "SimpleMPSC " + std::to_string(r.num_producers) + "P";

        std::cout << std::left << std::setw(25) << label
                  << std::right << std::setw(12) << r.throughput_mps << " M/s"
                  << std::setw(12) << r.latency_ns << " ns"
                  << std::setw(12) << ratio << "x" << "\n";
    }

    print_separator();

    // Analysis
    std::cout << "\nAnalysis:\n";

    if (!mpsc_results.empty() && mpsc_results[0].throughput_mps > 0) {
        double mpsc_1p_overhead =
            (spsc_result.latency_ns / mpsc_results[0].latency_ns - 1.0) * 100.0;
        std::cout << "  MPSC 1P overhead vs SPSC: "
                  << std::showpos << mpsc_1p_overhead << "%" << std::noshowpos << "\n";
    }

    if (mpsc_results.size() >= 2) {
        double scaling_2p = mpsc_results[1].throughput_mps / mpsc_results[0].throughput_mps;
        std::cout << "  MPSC scaling 1P->2P: " << scaling_2p << "x\n";
    }

    if (mpsc_results.size() >= 3) {
        double scaling_4p = mpsc_results[2].throughput_mps / mpsc_results[0].throughput_mps;
        std::cout << "  MPSC scaling 1P->4P: " << scaling_4p << "x\n";
    }

    std::cout << "\n";

    return 0;
}
