// Benchmark: SIMD SOH Scanner (Scalar vs AVX2 vs AVX-512)
// Compares performance of different SIMD implementations
//
// Build: g++ -std=c++23 -O3 -march=native -mavx2 simd_scanner_bench.cpp -o simd_scanner_bench
// With AVX-512: g++ -std=c++23 -O3 -march=native -mavx512f -mavx512bw simd_scanner_bench.cpp -o simd_scanner_bench

#include <iostream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <numeric>
#include <algorithm>
#include <random>
#include <cstring>
#include <cmath>

// Define NFX_HAS_SIMD before including
#define NFX_HAS_SIMD 1
#include "../include/nexusfix/interfaces/i_message.hpp"
#include "../include/nexusfix/memory/buffer_pool.hpp"
#include "../include/nexusfix/parser/simd_scanner.hpp"

// Benchmark configuration
constexpr int WARMUP_ITERATIONS = 1000;
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

struct BenchmarkResult {
    double mean_ns;
    double median_ns;
    double throughput_gbps;
    size_t bytes_processed;
};

// Generate realistic FIX message data
std::vector<char> generate_fix_data(size_t size, int soh_density = 20) {
    std::vector<char> data(size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> char_dist('0', 'z');
    std::uniform_int_distribution<> soh_dist(1, soh_density);

    for (size_t i = 0; i < size; ++i) {
        if (soh_dist(gen) == 1) {
            data[i] = nfx::fix::SOH;
        } else {
            data[i] = static_cast<char>(char_dist(gen));
        }
    }

    return data;
}

template<typename ScanFunc>
BenchmarkResult benchmark_scan(
    ScanFunc&& scan_func,
    std::span<const char> data,
    int iterations,
    double cpu_freq_ghz)
{
    std::vector<uint64_t> latencies;
    latencies.reserve(iterations);

    for (int i = 0; i < iterations; ++i) {
        uint64_t start = rdtsc();
        auto result = scan_func(data);
        uint64_t end = rdtsc();

        // Prevent optimization
        asm volatile("" :: "r"(result.count));

        latencies.push_back(end - start);
    }

    std::sort(latencies.begin(), latencies.end());

    size_t n = latencies.size();
    double mean_cycles = std::accumulate(latencies.begin(), latencies.end(), 0.0) / n;
    double median_cycles = latencies[n / 2];

    double mean_ns = mean_cycles / cpu_freq_ghz;
    double median_ns = median_cycles / cpu_freq_ghz;

    // Throughput: bytes / time
    double throughput_gbps = (data.size() * 8.0) / mean_ns;  // Gbps

    return BenchmarkResult{
        .mean_ns = mean_ns,
        .median_ns = median_ns,
        .throughput_gbps = throughput_gbps,
        .bytes_processed = data.size()
    };
}

void print_result(const char* name, const BenchmarkResult& r) {
    std::cout << std::setw(20) << std::left << name
              << std::setw(12) << std::fixed << std::setprecision(1) << r.mean_ns << " ns"
              << std::setw(12) << r.median_ns << " ns"
              << std::setw(12) << std::setprecision(2) << r.throughput_gbps << " Gbps\n";
}

int main() {
    std::cout << "==========================================================\n";
    std::cout << "  SIMD SOH Scanner Benchmark: Scalar vs AVX2 vs AVX-512\n";
    std::cout << "==========================================================\n\n";

    // Feature detection
    std::cout << "SIMD Features:\n";
    std::cout << "  NFX_SIMD_AVAILABLE:   " << NFX_SIMD_AVAILABLE << "\n";
    std::cout << "  NFX_AVX512_AVAILABLE: " << NFX_AVX512_AVAILABLE << "\n";

    std::cout << "\nCalibrating CPU frequency...\n";
    double cpu_freq_ghz = get_cpu_freq_ghz();
    std::cout << "  CPU frequency: " << std::fixed << std::setprecision(3) << cpu_freq_ghz << " GHz\n";

    std::cout << "\nBenchmark configuration:\n";
    std::cout << "  Warmup:     " << WARMUP_ITERATIONS << " iterations\n";
    std::cout << "  Benchmark:  " << BENCHMARK_ITERATIONS << " iterations\n";
    std::cout << "  Runs:       " << NUM_RUNS << "\n";

    // Test different buffer sizes
    std::vector<size_t> sizes = {64, 128, 256, 512, 1024, 2048, 4096, 8192};

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Results (mean latency, median latency, throughput)\n";
    std::cout << "----------------------------------------------------------\n";

    for (size_t size : sizes) {
        std::cout << "\nBuffer size: " << size << " bytes\n";
        std::cout << std::string(60, '-') << "\n";

        auto data = generate_fix_data(size);
        std::span<const char> span{data.data(), data.size()};

        // Warmup
        for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
            auto r1 = nfx::simd::scan_soh_scalar(span);
            asm volatile("" :: "r"(r1.count));
#if NFX_SIMD_AVAILABLE
            auto r2 = nfx::simd::scan_soh_avx2(span);
            asm volatile("" :: "r"(r2.count));
#endif
#if NFX_AVX512_AVAILABLE
            auto r3 = nfx::simd::scan_soh_avx512(span);
            asm volatile("" :: "r"(r3.count));
#endif
        }

        // Benchmark scalar
        std::vector<BenchmarkResult> scalar_results;
        for (int run = 0; run < NUM_RUNS; ++run) {
            scalar_results.push_back(benchmark_scan(
                nfx::simd::scan_soh_scalar, span, BENCHMARK_ITERATIONS, cpu_freq_ghz));
        }

        // Average scalar results
        BenchmarkResult scalar_avg{};
        for (const auto& r : scalar_results) {
            scalar_avg.mean_ns += r.mean_ns;
            scalar_avg.median_ns += r.median_ns;
            scalar_avg.throughput_gbps += r.throughput_gbps;
        }
        scalar_avg.mean_ns /= NUM_RUNS;
        scalar_avg.median_ns /= NUM_RUNS;
        scalar_avg.throughput_gbps /= NUM_RUNS;
        scalar_avg.bytes_processed = size;

        print_result("Scalar", scalar_avg);

#if NFX_SIMD_AVAILABLE
        // Benchmark AVX2
        std::vector<BenchmarkResult> avx2_results;
        for (int run = 0; run < NUM_RUNS; ++run) {
            avx2_results.push_back(benchmark_scan(
                nfx::simd::scan_soh_avx2, span, BENCHMARK_ITERATIONS, cpu_freq_ghz));
        }

        BenchmarkResult avx2_avg{};
        for (const auto& r : avx2_results) {
            avx2_avg.mean_ns += r.mean_ns;
            avx2_avg.median_ns += r.median_ns;
            avx2_avg.throughput_gbps += r.throughput_gbps;
        }
        avx2_avg.mean_ns /= NUM_RUNS;
        avx2_avg.median_ns /= NUM_RUNS;
        avx2_avg.throughput_gbps /= NUM_RUNS;
        avx2_avg.bytes_processed = size;

        print_result("AVX2", avx2_avg);

        // Speedup
        double avx2_speedup = scalar_avg.mean_ns / avx2_avg.mean_ns;
        std::cout << "  AVX2 speedup: " << std::fixed << std::setprecision(2) << avx2_speedup << "x\n";
#endif

#if NFX_AVX512_AVAILABLE
        // Benchmark AVX-512
        std::vector<BenchmarkResult> avx512_results;
        for (int run = 0; run < NUM_RUNS; ++run) {
            avx512_results.push_back(benchmark_scan(
                nfx::simd::scan_soh_avx512, span, BENCHMARK_ITERATIONS, cpu_freq_ghz));
        }

        BenchmarkResult avx512_avg{};
        for (const auto& r : avx512_results) {
            avx512_avg.mean_ns += r.mean_ns;
            avx512_avg.median_ns += r.median_ns;
            avx512_avg.throughput_gbps += r.throughput_gbps;
        }
        avx512_avg.mean_ns /= NUM_RUNS;
        avx512_avg.median_ns /= NUM_RUNS;
        avx512_avg.throughput_gbps /= NUM_RUNS;
        avx512_avg.bytes_processed = size;

        print_result("AVX-512", avx512_avg);

        double avx512_speedup = scalar_avg.mean_ns / avx512_avg.mean_ns;
        double avx512_vs_avx2 = avx2_avg.mean_ns / avx512_avg.mean_ns;
        std::cout << "  AVX-512 speedup (vs scalar): " << std::fixed << std::setprecision(2) << avx512_speedup << "x\n";
        std::cout << "  AVX-512 speedup (vs AVX2):   " << avx512_vs_avx2 << "x\n";
#endif
    }

    std::cout << "\n==========================================================\n";
    std::cout << "  Summary\n";
    std::cout << "==========================================================\n";

    std::cout << "\nImplementation hierarchy (auto-selected by buffer size):\n";
    std::cout << "  >= 128 bytes: AVX-512 (if available)\n";
    std::cout << "  >= 64 bytes:  AVX2\n";
    std::cout << "  < 64 bytes:   Scalar\n";

#if !NFX_AVX512_AVAILABLE
    std::cout << "\nNote: AVX-512 not available on this system.\n";
    std::cout << "Expected AVX-512 improvement: ~1.5-2x vs AVX2 for large buffers.\n";
#endif

    std::cout << "\n==========================================================\n";

    return 0;
}
