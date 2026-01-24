// Benchmark: Hash Map Performance (std::unordered_map vs absl::flat_hash_map)
// Compares lookup, insert, and iteration performance
//
// Build: cmake --build build && ./benchmarks/hash_map_bench

#include <iostream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <numeric>
#include <algorithm>
#include <random>
#include <unordered_map>

#define NFX_HAS_ABSEIL 1
#include <absl/container/flat_hash_map.h>

// Benchmark configuration
constexpr int WARMUP_ITERATIONS = 1000;
constexpr int BENCHMARK_ITERATIONS = 100000;
constexpr int NUM_RUNS = 5;
constexpr size_t MAP_SIZE = 10000;  // Number of entries in map

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
    double ops_per_sec;
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
        .ops_per_sec = 1e9 / mean
    };
}

template<typename MapType>
BenchmarkResult benchmark_lookup(
    MapType& map,
    const std::vector<uint32_t>& keys,
    int iterations,
    double cpu_freq_ghz)
{
    std::vector<uint64_t> latencies;
    latencies.reserve(iterations);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dist(0, keys.size() - 1);

    for (int i = 0; i < iterations; ++i) {
        uint32_t key = keys[dist(gen)];

        uint64_t start = rdtsc();
        auto it = map.find(key);
        uint64_t end = rdtsc();

        // Prevent optimization
        asm volatile("" :: "r"(it != map.end()));

        latencies.push_back(end - start);
    }

    return calculate_stats(latencies, cpu_freq_ghz);
}

template<typename MapType>
BenchmarkResult benchmark_insert(
    const std::vector<uint32_t>& keys,
    int iterations,
    double cpu_freq_ghz)
{
    std::vector<uint64_t> latencies;
    latencies.reserve(iterations);

    for (int i = 0; i < iterations; ++i) {
        MapType map;
        map.reserve(keys.size());

        uint64_t start = rdtsc();
        for (uint32_t key : keys) {
            map[key] = std::vector<char>(100, 'x');
        }
        uint64_t end = rdtsc();

        latencies.push_back((end - start) / keys.size());
    }

    return calculate_stats(latencies, cpu_freq_ghz);
}

void print_result(const char* name, const BenchmarkResult& r) {
    std::cout << std::setw(25) << std::left << name
              << std::setw(12) << std::fixed << std::setprecision(1) << r.mean_ns << " ns"
              << std::setw(12) << r.median_ns << " ns"
              << std::setw(12) << r.p99_ns << " ns"
              << std::setw(15) << std::scientific << std::setprecision(2) << r.ops_per_sec << " ops/s\n";
}

int main() {
    std::cout << "==========================================================\n";
    std::cout << "  Hash Map Benchmark: std::unordered_map vs absl::flat_hash_map\n";
    std::cout << "==========================================================\n\n";

    std::cout << "Calibrating CPU frequency...\n";
    double cpu_freq_ghz = get_cpu_freq_ghz();
    std::cout << "  CPU frequency: " << std::fixed << std::setprecision(3) << cpu_freq_ghz << " GHz\n";

    std::cout << "\nConfiguration:\n";
    std::cout << "  Map size:     " << MAP_SIZE << " entries\n";
    std::cout << "  Value size:   100 bytes (typical FIX message)\n";
    std::cout << "  Warmup:       " << WARMUP_ITERATIONS << " iterations\n";
    std::cout << "  Benchmark:    " << BENCHMARK_ITERATIONS << " iterations\n";
    std::cout << "  Runs:         " << NUM_RUNS << "\n";

    // Generate test keys
    std::vector<uint32_t> keys(MAP_SIZE);
    std::iota(keys.begin(), keys.end(), 1);

    // Shuffle for random access pattern
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(keys.begin(), keys.end(), gen);

    // Populate maps
    std::unordered_map<uint32_t, std::vector<char>> std_map;
    absl::flat_hash_map<uint32_t, std::vector<char>> absl_map;

    std_map.reserve(MAP_SIZE);
    absl_map.reserve(MAP_SIZE);

    for (uint32_t key : keys) {
        std::vector<char> value(100, 'x');
        std_map[key] = value;
        absl_map[key] = value;
    }

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Lookup Performance (random access)\n";
    std::cout << "----------------------------------------------------------\n";
    std::cout << std::setw(25) << "Implementation"
              << std::setw(12) << "Mean"
              << std::setw(12) << "Median"
              << std::setw(12) << "P99"
              << std::setw(15) << "Throughput\n";
    std::cout << std::string(76, '-') << "\n";

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
        auto it1 = std_map.find(keys[i % keys.size()]);
        auto it2 = absl_map.find(keys[i % keys.size()]);
        asm volatile("" :: "r"(it1 != std_map.end()), "r"(it2 != absl_map.end()));
    }

    // Benchmark std::unordered_map lookups
    std::vector<BenchmarkResult> std_results;
    for (int run = 0; run < NUM_RUNS; ++run) {
        std_results.push_back(benchmark_lookup(std_map, keys, BENCHMARK_ITERATIONS, cpu_freq_ghz));
    }

    BenchmarkResult std_avg{};
    for (const auto& r : std_results) {
        std_avg.mean_ns += r.mean_ns;
        std_avg.median_ns += r.median_ns;
        std_avg.p99_ns += r.p99_ns;
    }
    std_avg.mean_ns /= NUM_RUNS;
    std_avg.median_ns /= NUM_RUNS;
    std_avg.p99_ns /= NUM_RUNS;
    std_avg.ops_per_sec = 1e9 / std_avg.mean_ns;

    print_result("std::unordered_map", std_avg);

    // Benchmark absl::flat_hash_map lookups
    std::vector<BenchmarkResult> absl_results;
    for (int run = 0; run < NUM_RUNS; ++run) {
        absl_results.push_back(benchmark_lookup(absl_map, keys, BENCHMARK_ITERATIONS, cpu_freq_ghz));
    }

    BenchmarkResult absl_avg{};
    for (const auto& r : absl_results) {
        absl_avg.mean_ns += r.mean_ns;
        absl_avg.median_ns += r.median_ns;
        absl_avg.p99_ns += r.p99_ns;
    }
    absl_avg.mean_ns /= NUM_RUNS;
    absl_avg.median_ns /= NUM_RUNS;
    absl_avg.p99_ns /= NUM_RUNS;
    absl_avg.ops_per_sec = 1e9 / absl_avg.mean_ns;

    print_result("absl::flat_hash_map", absl_avg);

    // Calculate speedup
    double lookup_speedup = std_avg.mean_ns / absl_avg.mean_ns;
    std::cout << "\n  Speedup: " << std::fixed << std::setprecision(2) << lookup_speedup << "x faster\n";

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Insert Performance (bulk insert)\n";
    std::cout << "----------------------------------------------------------\n";
    std::cout << std::setw(25) << "Implementation"
              << std::setw(12) << "Mean"
              << std::setw(12) << "Median"
              << std::setw(12) << "P99"
              << std::setw(15) << "Throughput\n";
    std::cout << std::string(76, '-') << "\n";

    // Benchmark std::unordered_map inserts
    auto std_insert = benchmark_insert<std::unordered_map<uint32_t, std::vector<char>>>(
        keys, 100, cpu_freq_ghz);
    print_result("std::unordered_map", std_insert);

    // Benchmark absl::flat_hash_map inserts
    auto absl_insert = benchmark_insert<absl::flat_hash_map<uint32_t, std::vector<char>>>(
        keys, 100, cpu_freq_ghz);
    print_result("absl::flat_hash_map", absl_insert);

    double insert_speedup = std_insert.mean_ns / absl_insert.mean_ns;
    std::cout << "\n  Speedup: " << std::fixed << std::setprecision(2) << insert_speedup << "x faster\n";

    // Summary
    std::cout << "\n==========================================================\n";
    std::cout << "  Summary\n";
    std::cout << "==========================================================\n\n";

    std::cout << "Lookup Performance:\n";
    std::cout << "  std::unordered_map:   " << std::fixed << std::setprecision(1) << std_avg.mean_ns << " ns\n";
    std::cout << "  absl::flat_hash_map:  " << absl_avg.mean_ns << " ns\n";
    std::cout << "  Speedup:              " << std::setprecision(2) << lookup_speedup << "x\n";

    std::cout << "\nInsert Performance:\n";
    std::cout << "  std::unordered_map:   " << std::setprecision(1) << std_insert.mean_ns << " ns/op\n";
    std::cout << "  absl::flat_hash_map:  " << absl_insert.mean_ns << " ns/op\n";
    std::cout << "  Speedup:              " << std::setprecision(2) << insert_speedup << "x\n";

    std::cout << "\nWhy absl::flat_hash_map is faster:\n";
    std::cout << "  - Swiss Tables: SIMD-accelerated probing (SSE compares 16 slots)\n";
    std::cout << "  - Higher load factor: 87.5% vs 50% = smaller tables\n";
    std::cout << "  - Better cache locality: flat storage vs node-based\n";

    std::cout << "\n==========================================================\n";

    return 0;
}
