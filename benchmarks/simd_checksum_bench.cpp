// Benchmark: SIMD FIX Checksum Performance
// Compares scalar vs SSE2 vs AVX2 vs AVX-512 checksum calculation
//
// Build: cmake --build build && ./build/bin/benchmarks/simd_checksum_bench

#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <cstring>
#include <random>

#include "nexusfix/parser/simd_checksum.hpp"
#include "nexusfix/util/cpu_affinity.hpp"

using namespace nfx::parser;
using namespace nfx::util;

// Benchmark configuration
constexpr int WARMUP_ITERATIONS = 10000;
constexpr int BENCHMARK_ITERATIONS = 100000;

// Test message sizes
constexpr size_t SMALL_MSG = 64;    // Heartbeat
constexpr size_t MEDIUM_MSG = 256;  // NewOrderSingle
constexpr size_t LARGE_MSG = 1024;  // ExecutionReport with fills

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

// Generate random FIX-like message
std::vector<char> generate_message(size_t size) {
    std::vector<char> msg(size);
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(32, 126);

    for (size_t i = 0; i < size; ++i) {
        msg[i] = static_cast<char>(dist(rng));
    }

    // Add some FIX structure
    if (size >= 20) {
        std::memcpy(msg.data(), "8=FIX.4.4\x019=", 12);
    }

    return msg;
}

// Percentile calculation
template<typename T>
T percentile(std::vector<T>& data, double p) {
    size_t idx = static_cast<size_t>(p * static_cast<double>(data.size() - 1));
    return data[idx];
}

// Benchmark a checksum function
template<typename Func>
void benchmark_checksum(const char* name, Func func,
                        const char* data, size_t len,
                        double cpu_freq_ghz) {
    std::vector<uint64_t> latencies;
    latencies.reserve(BENCHMARK_ITERATIONS);

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
        uint8_t cs = func(data, len);
        asm volatile("" : : "r"(cs) : "memory");
    }

    // Benchmark
    for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        uint64_t start = rdtsc();
        uint8_t cs = func(data, len);
        uint64_t end = rdtsc();

        asm volatile("" : : "r"(cs) : "memory");
        latencies.push_back(end - start);
    }

    std::sort(latencies.begin(), latencies.end());

    double median_cycles = static_cast<double>(latencies[BENCHMARK_ITERATIONS / 2]);
    double median_ns = median_cycles / cpu_freq_ghz;
    double bytes_per_cycle = static_cast<double>(len) / median_cycles;
    double throughput_gbps = (static_cast<double>(len) * cpu_freq_ghz) / median_cycles;

    std::cout << "  " << std::left << std::setw(12) << name
              << std::right << std::fixed << std::setprecision(1)
              << std::setw(8) << median_ns << " ns"
              << std::setw(8) << median_cycles << " cyc"
              << std::setw(8) << std::setprecision(2) << bytes_per_cycle << " B/cyc"
              << std::setw(10) << std::setprecision(1) << throughput_gbps << " GB/s\n";
}

int main() {
    std::cout << "==========================================================\n";
    std::cout << "  SIMD FIX Checksum Benchmark\n";
    std::cout << "==========================================================\n\n";

    // Pin to core for consistent results
    (void)CpuAffinity::pin_to_core(2);

    std::cout << "Calibrating CPU frequency...\n";
    double cpu_freq_ghz = get_cpu_freq_ghz();
    std::cout << "  CPU frequency: " << std::fixed << std::setprecision(3)
              << cpu_freq_ghz << " GHz\n";

    // Detect SIMD support
    std::cout << "\nSIMD Support:\n";
#if defined(NFX_AVX512_CHECKSUM)
    std::cout << "  AVX-512: Available\n";
#else
    std::cout << "  AVX-512: Not available\n";
#endif
#if defined(NFX_AVX2_CHECKSUM)
    std::cout << "  AVX2:    Available\n";
#else
    std::cout << "  AVX2:    Not available\n";
#endif
#if defined(NFX_SSE2_CHECKSUM)
    std::cout << "  SSE2:    Available\n";
#else
    std::cout << "  SSE2:    Not available\n";
#endif

    std::cout << "\nConfiguration:\n";
    std::cout << "  Warmup:     " << WARMUP_ITERATIONS << " iterations\n";
    std::cout << "  Benchmark:  " << BENCHMARK_ITERATIONS << " iterations\n";

    // Generate test messages
    auto small_msg = generate_message(SMALL_MSG);
    auto medium_msg = generate_message(MEDIUM_MSG);
    auto large_msg = generate_message(LARGE_MSG);

    // ========================================================================
    // Small Message (64 bytes - Heartbeat)
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Small Message (" << SMALL_MSG << " bytes - Heartbeat)\n";
    std::cout << "----------------------------------------------------------\n";
    std::cout << "  Method        Latency    Cycles   Throughput\n";

    benchmark_checksum("Scalar", checksum_scalar, small_msg.data(), SMALL_MSG, cpu_freq_ghz);
#if defined(NFX_SSE2_CHECKSUM)
    benchmark_checksum("SSE2", checksum_sse2, small_msg.data(), SMALL_MSG, cpu_freq_ghz);
#endif
#if defined(NFX_AVX2_CHECKSUM)
    benchmark_checksum("AVX2", checksum_avx2, small_msg.data(), SMALL_MSG, cpu_freq_ghz);
#endif
#if defined(NFX_AVX512_CHECKSUM)
    benchmark_checksum("AVX-512", checksum_avx512, small_msg.data(), SMALL_MSG, cpu_freq_ghz);
#endif
    benchmark_checksum("Auto", static_cast<uint8_t(*)(const char*, size_t)>(checksum), small_msg.data(), SMALL_MSG, cpu_freq_ghz);

    // ========================================================================
    // Medium Message (256 bytes - NewOrderSingle)
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Medium Message (" << MEDIUM_MSG << " bytes - NewOrderSingle)\n";
    std::cout << "----------------------------------------------------------\n";
    std::cout << "  Method        Latency    Cycles   Throughput\n";

    benchmark_checksum("Scalar", checksum_scalar, medium_msg.data(), MEDIUM_MSG, cpu_freq_ghz);
#if defined(NFX_SSE2_CHECKSUM)
    benchmark_checksum("SSE2", checksum_sse2, medium_msg.data(), MEDIUM_MSG, cpu_freq_ghz);
#endif
#if defined(NFX_AVX2_CHECKSUM)
    benchmark_checksum("AVX2", checksum_avx2, medium_msg.data(), MEDIUM_MSG, cpu_freq_ghz);
#endif
#if defined(NFX_AVX512_CHECKSUM)
    benchmark_checksum("AVX-512", checksum_avx512, medium_msg.data(), MEDIUM_MSG, cpu_freq_ghz);
#endif
    benchmark_checksum("Auto", static_cast<uint8_t(*)(const char*, size_t)>(checksum), medium_msg.data(), MEDIUM_MSG, cpu_freq_ghz);

    // ========================================================================
    // Large Message (1024 bytes - ExecutionReport)
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Large Message (" << LARGE_MSG << " bytes - ExecutionReport)\n";
    std::cout << "----------------------------------------------------------\n";
    std::cout << "  Method        Latency    Cycles   Throughput\n";

    benchmark_checksum("Scalar", checksum_scalar, large_msg.data(), LARGE_MSG, cpu_freq_ghz);
#if defined(NFX_SSE2_CHECKSUM)
    benchmark_checksum("SSE2", checksum_sse2, large_msg.data(), LARGE_MSG, cpu_freq_ghz);
#endif
#if defined(NFX_AVX2_CHECKSUM)
    benchmark_checksum("AVX2", checksum_avx2, large_msg.data(), LARGE_MSG, cpu_freq_ghz);
#endif
#if defined(NFX_AVX512_CHECKSUM)
    benchmark_checksum("AVX-512", checksum_avx512, large_msg.data(), LARGE_MSG, cpu_freq_ghz);
#endif
    benchmark_checksum("Auto", static_cast<uint8_t(*)(const char*, size_t)>(checksum), large_msg.data(), LARGE_MSG, cpu_freq_ghz);

    // ========================================================================
    // Verify Correctness
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Correctness Verification\n";
    std::cout << "----------------------------------------------------------\n";

    uint8_t scalar_cs = checksum_scalar(medium_msg.data(), MEDIUM_MSG);
    uint8_t auto_cs = checksum(medium_msg.data(), MEDIUM_MSG);

    std::cout << "  Scalar checksum:  " << static_cast<int>(scalar_cs) << "\n";
    std::cout << "  Auto checksum:    " << static_cast<int>(auto_cs) << "\n";
    std::cout << "  Match: " << (scalar_cs == auto_cs ? "YES" : "NO") << "\n";

    // Test formatting
    char formatted[4] = {};
    format_checksum(scalar_cs, formatted);
    std::cout << "  Formatted: " << formatted << "\n";

    // ========================================================================
    // Summary
    // ========================================================================

    std::cout << "\n==========================================================\n";
    std::cout << "  Implementation Summary\n";
    std::cout << "==========================================================\n\n";

    std::cout << "Components Added:\n";
    std::cout << "  1. checksum_scalar() - Baseline scalar implementation\n";
    std::cout << "  2. checksum_sse2() - 16 bytes/iteration (SSE2)\n";
    std::cout << "  3. checksum_avx2() - 32 bytes/iteration (AVX2)\n";
    std::cout << "  4. checksum_avx512() - 64 bytes/iteration (AVX-512)\n";
    std::cout << "  5. checksum() - Auto-dispatch to best available\n";
    std::cout << "  6. IncrementalChecksum - Streaming checksum\n";
    std::cout << "  7. validate_fix_checksum() - Full message validation\n";

    std::cout << "\nKey Optimizations:\n";
    std::cout << "  - Uses PSADBW (SAD) instruction for parallel byte sum\n";
    std::cout << "  - Unrolled loops for cache efficiency\n";
    std::cout << "  - Graceful fallback for older CPUs\n";
    std::cout << "  - Zero-copy operation\n";

    std::cout << "\nUsage:\n";
    std::cout << "  // Calculate checksum\n";
    std::cout << "  uint8_t cs = nfx::parser::checksum(msg_data, msg_len);\n";
    std::cout << "\n  // Validate message\n";
    std::cout << "  bool valid = nfx::parser::validate_fix_checksum(message);\n";
    std::cout << "\n  // Incremental\n";
    std::cout << "  IncrementalChecksum inc;\n";
    std::cout << "  inc.update(header);\n";
    std::cout << "  inc.update(body);\n";
    std::cout << "  uint8_t cs = inc.finalize();\n";

    std::cout << "\n==========================================================\n";

    return 0;
}
