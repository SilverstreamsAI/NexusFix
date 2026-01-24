// timing_comparison.cpp
// Compare rdtscp vs nanobench timing on same workload

#include <cstdint>
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <x86intrin.h>

#define ANKERL_NANOBENCH_IMPLEMENT
#include "include/nanobench.h"

#include "nexusfix/nexusfix.hpp"

using namespace nfx;

// ============================================================================
// Test Data
// ============================================================================

constexpr std::string_view EXEC_REPORT_BODY =
    "8=FIX.4.4\x01" "9=200\x01" "35=8\x01" "49=SENDER\x01" "56=TARGET\x01"
    "34=1\x01" "52=20240115-10:30:00.000\x01"
    "37=ORD123456\x01" "11=CLO789\x01" "17=EXEC001\x01"
    "150=0\x01" "39=0\x01" "55=AAPL\x01" "54=1\x01"
    "38=1000\x01" "44=150.50\x01" "14=0\x01" "151=1000\x01"
    "6=0\x01" "60=20240115-10:30:00.000\x01" "10=128\x01";

// ============================================================================
// rdtscp Timing
// ============================================================================

inline uint64_t rdtsc_start() noexcept {
    uint32_t aux;
    uint64_t tsc = __rdtscp(&aux);
    _mm_lfence();
    return tsc;
}

inline uint64_t rdtsc_end() noexcept {
    _mm_lfence();
    uint32_t aux;
    return __rdtscp(&aux);
}

inline uint64_t rdtsc() noexcept {
    uint32_t aux;
    return __rdtscp(&aux);
}

inline double get_cpu_freq_ghz() noexcept {
    auto start_time = std::chrono::steady_clock::now();
    uint64_t start_cycles = rdtsc();

    volatile uint64_t dummy = 0;
    for (int i = 0; i < 10000000; ++i) {
        dummy += i;
    }

    uint64_t end_cycles = rdtsc();
    auto end_time = std::chrono::steady_clock::now();

    auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        end_time - start_time).count();

    return static_cast<double>(end_cycles - start_cycles) / elapsed_ns;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "===========================================\n";
    std::cout << "  rdtscp vs nanobench Timing Comparison\n";
    std::cout << "===========================================\n\n";

    // Prepare test data
    std::string msg(EXEC_REPORT_BODY);
    std::span<const char> data{msg.data(), msg.size()};

    // Warmup
    std::cout << "Warming up...\n";
    for (size_t i = 0; i < 10000; ++i) {
        auto result = IndexedParser::parse(data);
        ankerl::nanobench::doNotOptimizeAway(result);
    }

    // =========================================================================
    // Test 1: rdtscp timing
    // =========================================================================
    std::cout << "\n--- rdtscp Timing ---\n";

    double freq_ghz = get_cpu_freq_ghz();
    std::cout << "CPU frequency: " << std::fixed << std::setprecision(3)
              << freq_ghz << " GHz\n";

    constexpr size_t ITERATIONS = 100000;
    std::vector<double> latencies;
    latencies.reserve(ITERATIONS);

    for (size_t i = 0; i < ITERATIONS; ++i) {
        uint64_t start = rdtsc_start();
        auto result = IndexedParser::parse(data);
        uint64_t end = rdtsc_end();

        ankerl::nanobench::doNotOptimizeAway(result);

        uint64_t cycles = end - start;
        double ns = static_cast<double>(cycles) / freq_ghz;
        latencies.push_back(ns);
    }

    std::sort(latencies.begin(), latencies.end());

    double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
    double mean = sum / latencies.size();
    size_t p50_idx = latencies.size() * 50 / 100;
    size_t p99_idx = latencies.size() * 99 / 100;

    std::cout << "  Iterations: " << ITERATIONS << "\n";
    std::cout << "  Min:   " << std::fixed << std::setprecision(2)
              << latencies.front() << " ns\n";
    std::cout << "  Mean:  " << mean << " ns\n";
    std::cout << "  P50:   " << latencies[p50_idx] << " ns\n";
    std::cout << "  P99:   " << latencies[p99_idx] << " ns\n";
    std::cout << "  Max:   " << latencies.back() << " ns\n";

    // =========================================================================
    // Test 2: nanobench timing
    // =========================================================================
    std::cout << "\n--- nanobench Timing ---\n";

    ankerl::nanobench::Bench bench;
    bench.warmup(1000)
         .minEpochIterations(ITERATIONS)
         .run("ExecutionReport Parse", [&]() {
             auto result = IndexedParser::parse(data);
             ankerl::nanobench::doNotOptimizeAway(result);
         });

    // =========================================================================
    // Summary
    // =========================================================================
    std::cout << "\n===========================================\n";
    std::cout << "  Summary\n";
    std::cout << "===========================================\n";
    std::cout << "rdtscp Mean:    " << std::fixed << std::setprecision(2)
              << mean << " ns\n";
    std::cout << "nanobench:      See table above\n";
    std::cout << "\nNote: nanobench measures throughput-optimized latency\n";
    std::cout << "      rdtscp measures per-operation serialized latency\n";

    return 0;
}
