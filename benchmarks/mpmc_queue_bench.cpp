/*
    MPMC Queue Benchmark

    Compares:
    - SPSC Queue (baseline)
    - MPSC Queue 4P1C (reference)
    - MPMC Queue in various configurations

    Scenarios:
    - 1P1C  (compare overhead vs SPSC)
    - 2P2C
    - 4P4C
    - 4P1C  (compare with MPSC)
    - 1P4C

    Metrics:
    - Throughput (messages/second)
    - Latency (ns per operation)
*/

#include <nexusfix/memory/spsc_queue.hpp>
#include <nexusfix/memory/mpsc_queue.hpp>
#include <nexusfix/memory/mpmc_queue.hpp>
#include <nexusfix/memory/wait_strategy.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

using namespace nfx::memory;
using namespace std::chrono;

// ============================================================================
// Test Configuration
// ============================================================================

constexpr size_t QUEUE_CAPACITY = 4096;
constexpr size_t TOTAL_MESSAGES = 100'000;
constexpr size_t WARMUP_ITERATIONS = 1'000;

// Test payload (cache-line sized)
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
// Result Types
// ============================================================================

struct BenchResult {
    double throughput_mps;  // millions of messages per second
    double latency_ns;      // nanoseconds per operation
    uint64_t total_messages;
    size_t num_producers;
    size_t num_consumers;
    const char* queue_type;
};

// ============================================================================
// SPSC Baseline
// ============================================================================

BenchResult benchmark_spsc(size_t num_messages) {
    auto queue_ptr = std::make_unique<SPSCQueue<TestMessage, QUEUE_CAPACITY>>();
    auto& queue = *queue_ptr;
    std::atomic<bool> done{false};
    std::atomic<uint64_t> consumed{0};

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

    // Warmup
    for (size_t i = 0; i < WARMUP_ITERATIONS; ++i) {
        TestMessage msg{i, rdtsc(), 0, {}};
        while (!queue.try_push(msg)) {
            _mm_pause();
        }
    }
    while (!queue.empty()) {
        std::this_thread::yield();
    }

    // Benchmark
    auto start_time = steady_clock::now();

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
        .total_messages = consumed.load(),
        .num_producers = 1,
        .num_consumers = 1,
        .queue_type = "SPSC"
    };
}

// ============================================================================
// MPSC 4P1C Reference
// ============================================================================

BenchResult benchmark_mpsc_4p1c(size_t messages_per_producer) {
    constexpr size_t NUM_PRODUCERS = 4;
    using QueueT = MPSCQueue<TestMessage, QUEUE_CAPACITY, BusySpinWait>;

    auto queue_ptr = std::make_unique<QueueT>();
    auto& queue = *queue_ptr;
    std::atomic<bool> start{false};
    std::atomic<uint64_t> consumed{0};
    std::atomic<size_t> producers_done{0};

    std::thread consumer([&]() {
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        TestMessage msg;
        uint64_t count = 0;
        while (producers_done.load(std::memory_order_acquire) < NUM_PRODUCERS ||
               !queue.empty()) {
            if (queue.try_pop(msg)) {
                ++count;
            } else {
                _mm_pause();
            }
        }
        consumed.store(count, std::memory_order_release);
    });

    std::vector<std::thread> producers;
    producers.reserve(NUM_PRODUCERS);
    for (size_t p = 0; p < NUM_PRODUCERS; ++p) {
        producers.emplace_back([&, p]() {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (size_t i = 0; i < messages_per_producer; ++i) {
                TestMessage msg{i, rdtsc(), p, {}};
                while (!queue.try_push(msg)) {
                    _mm_pause();
                }
            }
            producers_done.fetch_add(1, std::memory_order_release);
        });
    }

    std::this_thread::sleep_for(milliseconds(1));

    auto start_time = steady_clock::now();
    start.store(true, std::memory_order_release);

    for (auto& t : producers) {
        t.join();
    }
    auto end_time = steady_clock::now();
    consumer.join();

    auto elapsed = duration_cast<nanoseconds>(end_time - start_time);
    double elapsed_sec = elapsed.count() / 1e9;
    size_t total = NUM_PRODUCERS * messages_per_producer;

    return {
        .throughput_mps = static_cast<double>(total) / elapsed_sec / 1e6,
        .latency_ns = static_cast<double>(elapsed.count()) / total,
        .total_messages = consumed.load(),
        .num_producers = NUM_PRODUCERS,
        .num_consumers = 1,
        .queue_type = "MPSC"
    };
}

// ============================================================================
// MPMC Generic Benchmark
// ============================================================================

BenchResult benchmark_mpmc(size_t num_producers, size_t num_consumers,
                           size_t messages_per_producer) {
    using QueueT = MPMCQueue<TestMessage, QUEUE_CAPACITY, BusySpinWait>;

    auto queue_ptr = std::make_unique<QueueT>();
    auto& queue = *queue_ptr;
    std::atomic<bool> start{false};
    std::atomic<size_t> producers_done{0};
    std::atomic<uint64_t> total_consumed{0};

    size_t total_messages = num_producers * messages_per_producer;

    // Consumer threads
    std::vector<std::thread> consumers;
    consumers.reserve(num_consumers);
    for (size_t c = 0; c < num_consumers; ++c) {
        consumers.emplace_back([&]() {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            TestMessage msg;
            uint64_t count = 0;
            while (true) {
                if (queue.try_pop(msg)) {
                    ++count;
                } else if (producers_done.load(std::memory_order_acquire) == num_producers) {
                    // Final drain
                    while (queue.try_pop(msg)) {
                        ++count;
                    }
                    break;
                } else {
                    _mm_pause();
                }
            }
            total_consumed.fetch_add(count, std::memory_order_relaxed);
        });
    }

    // Producer threads
    std::vector<std::thread> producers;
    producers.reserve(num_producers);
    for (size_t p = 0; p < num_producers; ++p) {
        producers.emplace_back([&, p]() {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (size_t i = 0; i < messages_per_producer; ++i) {
                TestMessage msg{i, rdtsc(), p, {}};
                while (!queue.try_push(msg)) {
                    _mm_pause();
                }
            }
            producers_done.fetch_add(1, std::memory_order_release);
        });
    }

    std::this_thread::sleep_for(milliseconds(1));

    auto start_time = steady_clock::now();
    start.store(true, std::memory_order_release);

    for (auto& t : producers) {
        t.join();
    }
    auto end_time = steady_clock::now();

    for (auto& t : consumers) {
        t.join();
    }

    auto elapsed = duration_cast<nanoseconds>(end_time - start_time);
    double elapsed_sec = elapsed.count() / 1e9;

    return {
        .throughput_mps = static_cast<double>(total_messages) / elapsed_sec / 1e6,
        .latency_ns = static_cast<double>(elapsed.count()) / total_messages,
        .total_messages = total_consumed.load(),
        .num_producers = num_producers,
        .num_consumers = num_consumers,
        .queue_type = "MPMC"
    };
}

// ============================================================================
// Main
// ============================================================================

void print_separator() {
    std::cout << std::string(78, '=') << "\n";
}

void print_header(const char* title) {
    std::cout << "\n";
    print_separator();
    std::cout << "  " << title << "\n";
    print_separator();
}

void print_result(const BenchResult& r, const BenchResult* baseline = nullptr) {
    std::string label = std::string(r.queue_type) + " " +
                        std::to_string(r.num_producers) + "P" +
                        std::to_string(r.num_consumers) + "C";

    std::cout << std::left << std::setw(20) << label
              << std::right << std::setw(12) << r.throughput_mps << " M/s"
              << std::setw(12) << r.latency_ns << " ns"
              << std::setw(12) << r.total_messages;

    if (baseline) {
        double ratio = r.throughput_mps / baseline->throughput_mps;
        std::cout << std::setw(10) << ratio << "x";
    } else {
        std::cout << std::setw(10) << "---";
    }

    std::cout << "\n";
}

int main() {
    std::cout << std::fixed << std::setprecision(2);

    print_header("MPMC Queue Benchmark (TICKET_209)");

    double cpu_freq = get_cpu_freq_ghz();
    std::cout << "\nCPU Frequency: " << cpu_freq << " GHz\n";
    std::cout << "Queue Capacity: " << QUEUE_CAPACITY << " entries\n";
    std::cout << "Message Size: " << sizeof(TestMessage) << " bytes\n";
    std::cout << "Total Messages: " << TOTAL_MESSAGES / 1'000.0 << "K\n\n";

    // ========================================================================
    // Run benchmarks
    // ========================================================================

    print_header("SPSC Baseline (1P1C)");
    std::cout << "\nRunning SPSC baseline...\n";
    auto spsc = benchmark_spsc(TOTAL_MESSAGES);
    std::cout << "  Throughput: " << spsc.throughput_mps << " M msg/s\n";
    std::cout << "  Latency:    " << spsc.latency_ns << " ns/op\n";
    std::cout << "  Messages:   " << spsc.total_messages << " consumed\n";

    print_header("MPSC Reference (4P1C)");
    std::cout << "\nRunning MPSC 4P1C...\n";
    auto mpsc_4p1c = benchmark_mpsc_4p1c(TOTAL_MESSAGES / 4);
    std::cout << "  Throughput: " << mpsc_4p1c.throughput_mps << " M msg/s\n";
    std::cout << "  Latency:    " << mpsc_4p1c.latency_ns << " ns/op\n";
    std::cout << "  Messages:   " << mpsc_4p1c.total_messages << " consumed\n";

    print_header("MPMC Queue Configurations");

    struct Config {
        size_t producers;
        size_t consumers;
    };

    Config configs[] = {
        {1, 1},
        {2, 2},
        {4, 4},
        {4, 1},
        {1, 4},
    };

    std::vector<BenchResult> mpmc_results;
    for (const auto& cfg : configs) {
        std::cout << "\nRunning MPMC " << cfg.producers << "P"
                  << cfg.consumers << "C...\n";

        size_t msgs_per_producer = TOTAL_MESSAGES / cfg.producers;
        auto result = benchmark_mpmc(cfg.producers, cfg.consumers, msgs_per_producer);

        mpmc_results.push_back(result);

        std::cout << "  Throughput: " << result.throughput_mps << " M msg/s\n";
        std::cout << "  Latency:    " << result.latency_ns << " ns/op\n";
        std::cout << "  Messages:   " << result.total_messages << " consumed\n";
    }

    // ========================================================================
    // Summary Table
    // ========================================================================

    print_header("Summary Comparison");

    std::cout << "\n";
    std::cout << std::left << std::setw(20) << "Configuration"
              << std::right << std::setw(16) << "Throughput"
              << std::setw(12) << "Latency"
              << std::setw(12) << "Messages"
              << std::setw(10) << "vs SPSC" << "\n";
    std::cout << std::string(78, '-') << "\n";

    // Baselines
    print_result(spsc);
    print_result(mpsc_4p1c, &spsc);

    std::cout << std::string(78, '-') << "\n";

    // MPMC results
    for (const auto& r : mpmc_results) {
        print_result(r, &spsc);
    }

    print_separator();

    // ========================================================================
    // Analysis
    // ========================================================================

    std::cout << "\nAnalysis:\n";

    if (!mpmc_results.empty()) {
        // MPMC 1P1C overhead vs SPSC
        double overhead_1p1c =
            (mpmc_results[0].latency_ns / spsc.latency_ns - 1.0) * 100.0;
        std::cout << "  MPMC 1P1C overhead vs SPSC: "
                  << std::showpos << overhead_1p1c << "%" << std::noshowpos << "\n";
    }

    if (mpmc_results.size() >= 4) {
        // MPMC 4P1C vs MPSC 4P1C
        double mpmc_vs_mpsc =
            mpmc_results[3].throughput_mps / mpsc_4p1c.throughput_mps;
        std::cout << "  MPMC 4P1C vs MPSC 4P1C: " << mpmc_vs_mpsc << "x throughput\n";
    }

    if (mpmc_results.size() >= 2) {
        double scaling = mpmc_results[1].throughput_mps / mpmc_results[0].throughput_mps;
        std::cout << "  MPMC scaling 1P1C->2P2C: " << scaling << "x\n";
    }

    if (mpmc_results.size() >= 3) {
        double scaling = mpmc_results[2].throughput_mps / mpmc_results[0].throughput_mps;
        std::cout << "  MPMC scaling 1P1C->4P4C: " << scaling << "x\n";
    }

    std::cout << "\n";

    return 0;
}
