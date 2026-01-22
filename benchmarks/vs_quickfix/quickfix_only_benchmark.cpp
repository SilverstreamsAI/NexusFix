// quickfix_only_benchmark.cpp
// QuickFIX Performance Benchmark (standalone, C++14 compatible)
// TICKET_004: QuickFIX Comparison Benchmark
//
// Usage: ./quickfix_benchmark [iterations]
// Compare output with NexusFIX parse_benchmark results

#include <chrono>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <string>

// QuickFIX headers
#include <quickfix/Message.h>
#include <quickfix/Field.h>
#include <quickfix/FixFields.h>
#include <quickfix/Values.h>

namespace bench {

// ============================================================================
// High-Resolution Timing Utilities
// ============================================================================

inline uint64_t rdtsc() noexcept {
    uint64_t lo, hi;
    asm volatile (
        "lfence\n\t"
        "rdtsc\n\t"
        "lfence\n\t"
        : "=a"(lo), "=d"(hi)
    );
    return (hi << 32) | lo;
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

inline double cycles_to_ns(uint64_t cycles, double freq_ghz) noexcept {
    return static_cast<double>(cycles) / freq_ghz;
}

// ============================================================================
// Statistics
// ============================================================================

struct BenchmarkStats {
    double min_ns;
    double max_ns;
    double mean_ns;
    double p50_ns;
    double p90_ns;
    double p99_ns;
    double p999_ns;
    double stddev_ns;
    size_t iterations;
};

BenchmarkStats calculate_stats(std::vector<double>& latencies) {
    BenchmarkStats stats = {};
    stats.iterations = latencies.size();

    if (latencies.empty()) return stats;

    std::sort(latencies.begin(), latencies.end());

    stats.min_ns = latencies.front();
    stats.max_ns = latencies.back();

    double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
    stats.mean_ns = sum / latencies.size();

    auto percentile = [&](double p) {
        size_t idx = static_cast<size_t>(p * latencies.size());
        if (idx >= latencies.size()) idx = latencies.size() - 1;
        return latencies[idx];
    };

    stats.p50_ns = percentile(0.50);
    stats.p90_ns = percentile(0.90);
    stats.p99_ns = percentile(0.99);
    stats.p999_ns = percentile(0.999);

    double sq_sum = 0.0;
    for (size_t i = 0; i < latencies.size(); ++i) {
        double diff = latencies[i] - stats.mean_ns;
        sq_sum += diff * diff;
    }
    stats.stddev_ns = std::sqrt(sq_sum / latencies.size());

    return stats;
}

void print_stats(const char* name, const BenchmarkStats& stats) {
    std::cout << "\n=== " << name << " ===" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Iterations: " << stats.iterations << std::endl;
    std::cout << "  Min:    " << std::setw(10) << stats.min_ns << " ns" << std::endl;
    std::cout << "  Mean:   " << std::setw(10) << stats.mean_ns << " ns" << std::endl;
    std::cout << "  P50:    " << std::setw(10) << stats.p50_ns << " ns" << std::endl;
    std::cout << "  P90:    " << std::setw(10) << stats.p90_ns << " ns" << std::endl;
    std::cout << "  P99:    " << std::setw(10) << stats.p99_ns << " ns" << std::endl;
    std::cout << "  P99.9:  " << std::setw(10) << stats.p999_ns << " ns" << std::endl;
    std::cout << "  Max:    " << std::setw(10) << stats.max_ns << " ns" << std::endl;
    std::cout << "  StdDev: " << std::setw(10) << stats.stddev_ns << " ns" << std::endl;
}

// ============================================================================
// Test Messages (SOH = \001)
// ============================================================================

// ExecutionReport message (35=8)
const std::string EXEC_REPORT_MSG =
    "8=FIX.4.4\001"
    "9=200\001"
    "35=8\001"
    "49=SENDER\001"
    "56=TARGET\001"
    "34=12345\001"
    "52=20240115-10:30:00.123\001"
    "37=ORD123456\001"
    "17=EXEC789012\001"
    "150=0\001"
    "39=0\001"
    "54=1\001"
    "151=1000\001"
    "14=0\001"
    "6=0\001"
    "55=AAPL\001"
    "38=1000\001"
    "44=150.50\001"
    "10=207\001";

// NewOrderSingle message (35=D)
const std::string NEW_ORDER_MSG =
    "8=FIX.4.4\001"
    "9=150\001"
    "35=D\001"
    "49=SENDER\001"
    "56=TARGET\001"
    "34=100\001"
    "52=20240115-10:30:00.000\001"
    "11=CLORD001\001"
    "55=AAPL\001"
    "54=1\001"
    "60=20240115-10:30:00.000\001"
    "38=1000\001"
    "40=2\001"
    "44=150.00\001"
    "59=0\001"
    "10=076\001";

// Heartbeat message (35=0)
const std::string HEARTBEAT_MSG =
    "8=FIX.4.4\001"
    "9=60\001"
    "35=0\001"
    "49=SENDER\001"
    "56=TARGET\001"
    "34=50\001"
    "52=20240115-10:30:00.000\001"
    "10=052\001";

// ============================================================================
// QuickFIX Benchmarks
// ============================================================================

BenchmarkStats benchmark_parse(const std::string& msg, size_t iterations, double freq_ghz) {
    std::vector<double> latencies;
    latencies.reserve(iterations);

    // Warmup
    for (size_t i = 0; i < 1000; ++i) {
        FIX::Message message;
        message.setString(msg, false);  // false = don't validate
    }

    // Benchmark
    for (size_t i = 0; i < iterations; ++i) {
        FIX::Message message;

        uint64_t start = rdtsc();
        message.setString(msg, false);
        uint64_t end = rdtsc();

        latencies.push_back(cycles_to_ns(end - start, freq_ghz));
    }

    return calculate_stats(latencies);
}

BenchmarkStats benchmark_field_access(const std::string& msg, size_t iterations, double freq_ghz) {
    std::vector<double> latencies;
    latencies.reserve(iterations);

    FIX::Message message;
    message.setString(msg, false);

    // Warmup
    for (size_t i = 0; i < 1000; ++i) {
        FIX::OrderID orderId;
        message.getField(orderId);
    }

    // Benchmark - access 4 fields
    for (size_t i = 0; i < iterations; ++i) {
        uint64_t start = rdtsc();

        FIX::OrderID orderId;
        FIX::ExecID execId;
        FIX::Side side;
        FIX::MsgType msgType;

        message.getField(orderId);
        message.getField(execId);
        message.getField(side);
        message.getHeader().getField(msgType);

        uint64_t end = rdtsc();
        latencies.push_back(cycles_to_ns(end - start, freq_ghz));

        // Prevent optimization
        if (orderId.getValue().empty() && execId.getValue().empty()) {
            std::cerr << "Error" << std::endl;
        }
    }

    return calculate_stats(latencies);
}

void benchmark_throughput(const std::string& msg, size_t num_messages) {
    std::cout << "\n=== Parse Throughput ===" << std::endl;

    auto start = std::chrono::steady_clock::now();
    size_t success = 0;

    for (size_t i = 0; i < num_messages; ++i) {
        FIX::Message message;
        message.setString(msg, false);
        ++success;
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration<double>(end - start);
    double msg_per_sec = success / duration.count();
    double bytes_per_sec = (success * msg.size()) / duration.count();

    std::cout << "  Messages:   " << success << std::endl;
    std::cout << "  Duration:   " << std::fixed << std::setprecision(3)
              << duration.count() << " sec" << std::endl;
    std::cout << "  Throughput: " << std::fixed << std::setprecision(0)
              << msg_per_sec << " msg/sec" << std::endl;
    std::cout << "  Bandwidth:  " << std::fixed << std::setprecision(2)
              << (bytes_per_sec / 1024 / 1024) << " MB/sec" << std::endl;
}

} // namespace bench

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    using namespace bench;

    size_t iterations = 100000;

    if (argc > 1) {
        iterations = std::stoul(argv[1]);
    }

    std::cout << "============================================================" << std::endl;
    std::cout << "           QuickFIX Performance Benchmark" << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << "Iterations: " << iterations << std::endl;
    std::cout << std::endl;
    std::cout << "Compare these results with NexusFIX parse_benchmark output" << std::endl;
    std::cout << "to see the performance difference." << std::endl;

    // Calibrate CPU frequency
    std::cout << "\nCalibrating CPU frequency..." << std::endl;
    double freq_ghz = get_cpu_freq_ghz();
    std::cout << "CPU frequency: " << std::fixed << std::setprecision(3)
              << freq_ghz << " GHz" << std::endl;

    // ========================================================================
    // ExecutionReport Parse
    // ========================================================================
    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "                 EXECUTIONREPORT (35=8)" << std::endl;
    std::cout << "------------------------------------------------------------" << std::endl;

    auto exec_stats = benchmark_parse(EXEC_REPORT_MSG, iterations, freq_ghz);
    print_stats("QuickFIX ExecutionReport Parse", exec_stats);

    // ========================================================================
    // Field Access
    // ========================================================================
    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "                    FIELD ACCESS" << std::endl;
    std::cout << "------------------------------------------------------------" << std::endl;

    auto field_stats = benchmark_field_access(EXEC_REPORT_MSG, iterations, freq_ghz);
    print_stats("QuickFIX Field Access (4 fields)", field_stats);

    // ========================================================================
    // NewOrderSingle Parse
    // ========================================================================
    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "                 NEWORDERSINGLE (35=D)" << std::endl;
    std::cout << "------------------------------------------------------------" << std::endl;

    auto order_stats = benchmark_parse(NEW_ORDER_MSG, iterations, freq_ghz);
    print_stats("QuickFIX NewOrderSingle Parse", order_stats);

    // ========================================================================
    // Heartbeat Parse
    // ========================================================================
    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "                   HEARTBEAT (35=0)" << std::endl;
    std::cout << "------------------------------------------------------------" << std::endl;

    auto hb_stats = benchmark_parse(HEARTBEAT_MSG, iterations, freq_ghz);
    print_stats("QuickFIX Heartbeat Parse", hb_stats);

    // ========================================================================
    // Throughput
    // ========================================================================
    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "                     THROUGHPUT" << std::endl;
    std::cout << "------------------------------------------------------------" << std::endl;

    benchmark_throughput(EXEC_REPORT_MSG, iterations);

    // ========================================================================
    // Summary
    // ========================================================================
    std::cout << "\n============================================================" << std::endl;
    std::cout << "                    QuickFIX SUMMARY" << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "ExecutionReport Parse:  " << exec_stats.mean_ns << " ns (mean)" << std::endl;
    std::cout << "Field Access (4 fields): " << field_stats.mean_ns << " ns (mean)" << std::endl;
    std::cout << "NewOrderSingle Parse:   " << order_stats.mean_ns << " ns (mean)" << std::endl;
    std::cout << "Heartbeat Parse:        " << hb_stats.mean_ns << " ns (mean)" << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << std::endl;
    std::cout << "To compare with NexusFIX, run:" << std::endl;
    std::cout << "  ./build/bin/benchmarks/parse_benchmark " << iterations << std::endl;
    std::cout << std::endl;

    return 0;
}
