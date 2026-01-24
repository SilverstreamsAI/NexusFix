// Benchmark: Compile-Time FIX Serializer Performance
// Compares constexpr serializer vs runtime builder
//
// Build: cmake --build build && ./build/bin/benchmarks/constexpr_serializer_bench

#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <cstring>

#include "nexusfix/serializer/constexpr_serializer.hpp"
#include "nexusfix/messages/common/header.hpp"
#include "nexusfix/util/cpu_affinity.hpp"

using namespace nfx;
using namespace nfx::serializer;

// Benchmark configuration
constexpr int WARMUP_ITERATIONS = 10000;
constexpr int BENCHMARK_ITERATIONS = 100000;

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

// Escape SOH for display
std::string escape_soh(std::string_view msg) {
    std::string result;
    result.reserve(msg.size() * 2);
    for (char c : msg) {
        if (c == '\x01') {
            result += '|';
        } else {
            result += c;
        }
    }
    return result;
}

int main() {
    std::cout << "==========================================================\n";
    std::cout << "  Compile-Time FIX Serializer Benchmark\n";
    std::cout << "==========================================================\n\n";

    // Pin to core for consistent results
    (void)util::CpuAffinity::pin_to_core(2);

    std::cout << "Calibrating CPU frequency...\n";
    double cpu_freq_ghz = get_cpu_freq_ghz();
    std::cout << "  CPU frequency: " << std::fixed << std::setprecision(3)
              << cpu_freq_ghz << " GHz\n";

    std::cout << "\nConfiguration:\n";
    std::cout << "  Warmup:       " << WARMUP_ITERATIONS << " iterations\n";
    std::cout << "  Benchmark:    " << BENCHMARK_ITERATIONS << " iterations\n";

    // Test data
    constexpr std::string_view BEGIN_STRING = "FIX.4.4";
    constexpr std::string_view SENDER = "SENDER";
    constexpr std::string_view TARGET = "TARGET";
    constexpr std::string_view SENDING_TIME = "20260123-10:30:00.000";
    constexpr std::string_view TEST_REQ_ID = "TEST123";

    // ========================================================================
    // Verify Output
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Sample Output Verification\n";
    std::cout << "----------------------------------------------------------\n";

    MessageFactory<512> factory(BEGIN_STRING, SENDER, TARGET);

    auto heartbeat = factory.build_heartbeat(1, SENDING_TIME, TEST_REQ_ID);
    std::cout << "\nHeartbeat (" << heartbeat.size() << " bytes):\n";
    std::cout << "  " << escape_soh({heartbeat.data(), heartbeat.size()}) << "\n";

    auto logon = factory.build_logon(1, SENDING_TIME, 30);
    std::cout << "\nLogon (" << logon.size() << " bytes):\n";
    std::cout << "  " << escape_soh({logon.data(), logon.size()}) << "\n";

    // ========================================================================
    // Warmup
    // ========================================================================

    std::cout << "\nWarming up...\n";
    for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
        auto msg = factory.build_heartbeat(static_cast<uint32_t>(i), SENDING_TIME, TEST_REQ_ID);
        asm volatile("" : : "r"(msg.data()) : "memory");
    }

    // ========================================================================
    // FastMessageBuilder Direct Benchmark
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  FastMessageBuilder (Compile-Time Tags)\n";
    std::cout << "----------------------------------------------------------\n";

    std::vector<uint64_t> fast_latencies;
    fast_latencies.reserve(BENCHMARK_ITERATIONS);

    FastMessageBuilder<256> fast_builder;

    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        uint64_t start = rdtsc();

        fast_builder.reset();
        fast_builder.begin_string(BEGIN_STRING);
        fast_builder.template field<9>(uint32_t(100));
        fast_builder.msg_type('0');
        fast_builder.sender_comp_id(SENDER);
        fast_builder.target_comp_id(TARGET);
        fast_builder.msg_seq_num(static_cast<uint32_t>(i));
        fast_builder.sending_time(SENDING_TIME);
        fast_builder.template field<112>(TEST_REQ_ID);

        uint64_t end = rdtsc();

        asm volatile("" : : "r"(fast_builder.data().data()) : "memory");
        fast_latencies.push_back(end - start);
    }

    std::sort(fast_latencies.begin(), fast_latencies.end());

    double fast_median = static_cast<double>(fast_latencies[BENCHMARK_ITERATIONS / 2]) / cpu_freq_ghz;
    double fast_mean = static_cast<double>(std::accumulate(fast_latencies.begin(), fast_latencies.end(), 0ULL)) /
                       static_cast<double>(BENCHMARK_ITERATIONS) / cpu_freq_ghz;
    double fast_p99 = static_cast<double>(percentile(fast_latencies, 0.99)) / cpu_freq_ghz;

    std::cout << "  Mean:   " << std::fixed << std::setprecision(1) << fast_mean << " ns\n";
    std::cout << "  Median: " << fast_median << " ns\n";
    std::cout << "  P99:    " << fast_p99 << " ns\n";
    std::cout << "  Size:   " << fast_builder.size() << " bytes\n";

    // ========================================================================
    // MessageFactory Benchmark
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  MessageFactory (Complete Message)\n";
    std::cout << "----------------------------------------------------------\n";

    std::vector<uint64_t> factory_latencies;
    factory_latencies.reserve(BENCHMARK_ITERATIONS);

    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        uint64_t start = rdtsc();
        auto msg = factory.build_heartbeat(static_cast<uint32_t>(i), SENDING_TIME, TEST_REQ_ID);
        uint64_t end = rdtsc();

        asm volatile("" : : "r"(msg.data()) : "memory");
        factory_latencies.push_back(end - start);
    }

    std::sort(factory_latencies.begin(), factory_latencies.end());

    double factory_median = static_cast<double>(factory_latencies[BENCHMARK_ITERATIONS / 2]) / cpu_freq_ghz;
    double factory_mean = static_cast<double>(std::accumulate(factory_latencies.begin(), factory_latencies.end(), 0ULL)) /
                          static_cast<double>(BENCHMARK_ITERATIONS) / cpu_freq_ghz;
    double factory_p99 = static_cast<double>(percentile(factory_latencies, 0.99)) / cpu_freq_ghz;

    std::cout << "  Mean:   " << std::fixed << std::setprecision(1) << factory_mean << " ns\n";
    std::cout << "  Median: " << factory_median << " ns\n";
    std::cout << "  P99:    " << factory_p99 << " ns\n";

    // ========================================================================
    // Runtime HeaderBuilder Benchmark (for comparison)
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  HeaderBuilder (Runtime Tag Conversion)\n";
    std::cout << "----------------------------------------------------------\n";

    std::vector<uint64_t> runtime_latencies;
    runtime_latencies.reserve(BENCHMARK_ITERATIONS);

    HeaderBuilder header_builder;

    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        uint64_t start = rdtsc();

        header_builder.reset();
        header_builder.begin_string(BEGIN_STRING);
        header_builder.body_length_placeholder();
        header_builder.msg_type('0');
        header_builder.sender_comp_id(SENDER);
        header_builder.target_comp_id(TARGET);
        header_builder.msg_seq_num(static_cast<uint32_t>(i));
        header_builder.sending_time(SENDING_TIME);

        uint64_t end = rdtsc();

        asm volatile("" : : "r"(header_builder.data().data()) : "memory");
        runtime_latencies.push_back(end - start);
    }

    std::sort(runtime_latencies.begin(), runtime_latencies.end());

    double runtime_median = static_cast<double>(runtime_latencies[BENCHMARK_ITERATIONS / 2]) / cpu_freq_ghz;
    double runtime_mean = static_cast<double>(std::accumulate(runtime_latencies.begin(), runtime_latencies.end(), 0ULL)) /
                          static_cast<double>(BENCHMARK_ITERATIONS) / cpu_freq_ghz;
    double runtime_p99 = static_cast<double>(percentile(runtime_latencies, 0.99)) / cpu_freq_ghz;

    std::cout << "  Mean:   " << std::fixed << std::setprecision(1) << runtime_mean << " ns\n";
    std::cout << "  Median: " << runtime_median << " ns\n";
    std::cout << "  P99:    " << runtime_p99 << " ns\n";
    std::cout << "  Size:   " << header_builder.size() << " bytes\n";

    // ========================================================================
    // Comparison Summary
    // ========================================================================

    std::cout << "\n==========================================================\n";
    std::cout << "  Comparison Summary\n";
    std::cout << "==========================================================\n\n";

    double speedup = runtime_median / fast_median;

    std::cout << "                    Constexpr    Runtime     Speedup\n";
    std::cout << "  --------------------------------------------------------\n";
    std::cout << "  Mean:        " << std::setw(10) << std::fixed << std::setprecision(1) << fast_mean << " ns"
              << std::setw(10) << runtime_mean << " ns"
              << std::setw(10) << std::setprecision(2) << (runtime_mean / fast_mean) << "x\n";
    std::cout << "  Median:      " << std::setw(10) << std::setprecision(1) << fast_median << " ns"
              << std::setw(10) << runtime_median << " ns"
              << std::setw(10) << std::setprecision(2) << speedup << "x\n";
    std::cout << "  P99:         " << std::setw(10) << std::setprecision(1) << fast_p99 << " ns"
              << std::setw(10) << runtime_p99 << " ns"
              << std::setw(10) << std::setprecision(2) << (runtime_p99 / fast_p99) << "x\n";

    std::cout << "\n  Compile-time serializer is " << std::setprecision(1) << speedup
              << "x faster!\n";

    // ========================================================================
    // Throughput
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Throughput\n";
    std::cout << "----------------------------------------------------------\n";

    double fast_throughput = 1e9 / fast_median;
    double runtime_throughput = 1e9 / runtime_median;

    std::cout << "  Constexpr: " << std::fixed << std::setprecision(1)
              << (fast_throughput / 1e6) << " M msgs/sec\n";
    std::cout << "  Runtime:   " << (runtime_throughput / 1e6) << " M msgs/sec\n";

    // ========================================================================
    // Summary
    // ========================================================================

    std::cout << "\n==========================================================\n";
    std::cout << "  Implementation Summary\n";
    std::cout << "==========================================================\n\n";

    std::cout << "Components Added:\n";
    std::cout << "  1. IntToString<N> - Compile-time integer to string\n";
    std::cout << "  2. TagString<Tag> - Pre-computed tag strings (e.g., \"35=\")\n";
    std::cout << "  3. FastMessageBuilder - Zero-overhead message building\n";
    std::cout << "  4. FastIntSerializer - Branch-free integer formatting\n";
    std::cout << "  5. MessageFactory - Complete message templates\n";
    std::cout << "  6. LogonBuilder, HeartbeatBuilder - Typed builders\n";

    std::cout << "\nKey Optimizations:\n";
    std::cout << "  - Tag strings computed at compile time\n";
    std::cout << "  - No runtime integer-to-string for tags\n";
    std::cout << "  - memcpy for all string values\n";
    std::cout << "  - Fixed-size buffers (no allocation)\n";
    std::cout << "  - Branch-free integer serialization\n";

    std::cout << "\nUsage:\n";
    std::cout << "  FastMessageBuilder<256> builder;\n";
    std::cout << "  builder.begin_string(\"FIX.4.4\");\n";
    std::cout << "  builder.msg_type('D');\n";
    std::cout << "  builder.field<49>(\"SENDER\");  // Compile-time tag\n";
    std::cout << "  builder.field<11>(\"ORDER123\");\n";
    std::cout << "  auto msg = builder.data();\n";

    std::cout << "\n==========================================================\n";

    return 0;
}
