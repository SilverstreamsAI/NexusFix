// sbe_benchmark.cpp
// NexusFIX SBE Binary Encoding Benchmark
// Target: ~5ns decode latency (vs ~200ns FIX text parsing = 40x speedup)

#include <chrono>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <x86intrin.h>

#include "nexusfix/sbe/sbe.hpp"
#include "nexusfix/nexusfix.hpp"

namespace nfx::bench {

// ============================================================================
// High-Resolution Timing Utilities
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
    BenchmarkStats stats{};
    stats.iterations = latencies.size();

    if (latencies.empty()) return stats;

    std::sort(latencies.begin(), latencies.end());

    stats.min_ns = latencies.front();
    stats.max_ns = latencies.back();

    double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
    stats.mean_ns = sum / static_cast<double>(latencies.size());

    auto percentile = [&latencies](double p) {
        size_t idx = static_cast<size_t>(p * static_cast<double>(latencies.size() - 1));
        return latencies[idx];
    };

    stats.p50_ns = percentile(0.50);
    stats.p90_ns = percentile(0.90);
    stats.p99_ns = percentile(0.99);
    stats.p999_ns = percentile(0.999);

    double sq_sum = 0.0;
    for (double lat : latencies) {
        double diff = lat - stats.mean_ns;
        sq_sum += diff * diff;
    }
    stats.stddev_ns = std::sqrt(sq_sum / static_cast<double>(latencies.size()));

    return stats;
}

void print_stats(const char* name, const BenchmarkStats& stats) {
    std::cout << "\n" << name << " (" << stats.iterations << " iterations):\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Min:    " << std::setw(8) << stats.min_ns << " ns\n";
    std::cout << "  P50:    " << std::setw(8) << stats.p50_ns << " ns\n";
    std::cout << "  P90:    " << std::setw(8) << stats.p90_ns << " ns\n";
    std::cout << "  P99:    " << std::setw(8) << stats.p99_ns << " ns\n";
    std::cout << "  P99.9:  " << std::setw(8) << stats.p999_ns << " ns\n";
    std::cout << "  Max:    " << std::setw(8) << stats.max_ns << " ns\n";
    std::cout << "  Mean:   " << std::setw(8) << stats.mean_ns << " ns\n";
    std::cout << "  StdDev: " << std::setw(8) << stats.stddev_ns << " ns\n";
}

// ============================================================================
// Test Data
// ============================================================================

// Sample FIX ExecutionReport message for text parsing comparison
constexpr std::string_view SAMPLE_EXEC_REPORT =
    "8=FIX.4.4\x01"
    "9=200\x01"
    "35=8\x01"
    "49=SENDER\x01"
    "56=TARGET\x01"
    "34=1\x01"
    "52=20240115-10:30:00.123\x01"
    "37=EX001\x01"
    "17=EXEC001\x01"
    "11=ORD001\x01"
    "55=AAPL\x01"
    "54=1\x01"
    "150=1\x01"
    "39=1\x01"
    "44=150.50\x01"
    "38=100\x01"
    "31=150.55\x01"
    "32=50\x01"
    "151=50\x01"
    "14=50\x01"
    "6=150.525\x01"
    "60=20240115-10:30:00.123\x01"
    "10=123\x01";

// Prepare SBE encoded ExecutionReport
void prepare_sbe_exec_report(char* buffer, size_t length) {
    FixedPrice price, lastPx, avgPx;
    price.raw = 15050000000LL;   // 150.50
    lastPx.raw = 15055000000LL;  // 150.55
    avgPx.raw = 15052500000LL;   // 150.525

    Qty orderQty, lastQty, leavesQty, cumQty;
    orderQty.raw = 1000000LL;  // 100
    lastQty.raw = 500000LL;    // 50
    leavesQty.raw = 500000LL;  // 50
    cumQty.raw = 500000LL;     // 50

    Timestamp ts{1705315800123000000LL};  // 2024-01-15 10:30:00.123

    sbe::ExecutionReportCodec::wrapForEncode(buffer, length)
        .encodeHeader()
        .orderId("EX001")
        .execId("EXEC001")
        .clOrdId("ORD001")
        .symbol("AAPL")
        .side(Side::Buy)
        .execType(ExecType::PartialFill)
        .ordStatus(OrdStatus::PartiallyFilled)
        .price(price)
        .orderQty(orderQty)
        .lastPx(lastPx)
        .lastQty(lastQty)
        .leavesQty(leavesQty)
        .cumQty(cumQty)
        .avgPx(avgPx)
        .transactTime(ts);
}

// ============================================================================
// Benchmarks
// ============================================================================

// Benchmark SBE ExecutionReport decode (single field access)
BenchmarkStats benchmark_sbe_decode_single_field(
    const char* buffer, size_t length,
    size_t iterations, double freq_ghz) {

    std::vector<double> latencies;
    latencies.reserve(iterations);

    // Warm up
    for (size_t i = 0; i < 1000; ++i) {
        auto codec = sbe::ExecutionReportCodec::wrapForDecode(buffer, length);
        volatile auto symbol = codec.symbol();
        (void)symbol;
    }

    // Benchmark
    for (size_t i = 0; i < iterations; ++i) {
        uint64_t start = rdtsc_start();

        auto codec = sbe::ExecutionReportCodec::wrapForDecode(buffer, length);
        volatile auto symbol = codec.symbol();
        (void)symbol;

        uint64_t end = rdtsc_end();
        latencies.push_back(cycles_to_ns(end - start, freq_ghz));
    }

    return calculate_stats(latencies);
}

// Benchmark SBE ExecutionReport decode (all fields)
BenchmarkStats benchmark_sbe_decode_all_fields(
    const char* buffer, size_t length,
    size_t iterations, double freq_ghz) {

    std::vector<double> latencies;
    latencies.reserve(iterations);

    // Warm up
    for (size_t i = 0; i < 1000; ++i) {
        auto codec = sbe::ExecutionReportCodec::wrapForDecode(buffer, length);
        volatile auto orderId = codec.orderId();
        volatile auto execId = codec.execId();
        volatile auto clOrdId = codec.clOrdId();
        volatile auto symbol = codec.symbol();
        volatile auto side = codec.side();
        volatile auto execType = codec.execType();
        volatile auto ordStatus = codec.ordStatus();
        volatile auto price = codec.price().raw;
        volatile auto orderQty = codec.orderQty().raw;
        volatile auto lastPx = codec.lastPx().raw;
        volatile auto lastQty = codec.lastQty().raw;
        volatile auto leavesQty = codec.leavesQty().raw;
        volatile auto cumQty = codec.cumQty().raw;
        volatile auto avgPx = codec.avgPx().raw;
        volatile auto transactTime = codec.transactTime().nanos;
        (void)orderId; (void)execId; (void)clOrdId; (void)symbol;
        (void)side; (void)execType; (void)ordStatus;
        (void)price; (void)orderQty; (void)lastPx; (void)lastQty;
        (void)leavesQty; (void)cumQty; (void)avgPx; (void)transactTime;
    }

    // Benchmark
    for (size_t i = 0; i < iterations; ++i) {
        uint64_t start = rdtsc_start();

        auto codec = sbe::ExecutionReportCodec::wrapForDecode(buffer, length);
        volatile auto orderId = codec.orderId();
        volatile auto execId = codec.execId();
        volatile auto clOrdId = codec.clOrdId();
        volatile auto symbol = codec.symbol();
        volatile auto side = codec.side();
        volatile auto execType = codec.execType();
        volatile auto ordStatus = codec.ordStatus();
        volatile auto price = codec.price().raw;
        volatile auto orderQty = codec.orderQty().raw;
        volatile auto lastPx = codec.lastPx().raw;
        volatile auto lastQty = codec.lastQty().raw;
        volatile auto leavesQty = codec.leavesQty().raw;
        volatile auto cumQty = codec.cumQty().raw;
        volatile auto avgPx = codec.avgPx().raw;
        volatile auto transactTime = codec.transactTime().nanos;
        (void)orderId; (void)execId; (void)clOrdId; (void)symbol;
        (void)side; (void)execType; (void)ordStatus;
        (void)price; (void)orderQty; (void)lastPx; (void)lastQty;
        (void)leavesQty; (void)cumQty; (void)avgPx; (void)transactTime;

        uint64_t end = rdtsc_end();
        latencies.push_back(cycles_to_ns(end - start, freq_ghz));
    }

    return calculate_stats(latencies);
}

// Benchmark FIX text parsing for comparison
BenchmarkStats benchmark_fix_text_parse(
    std::string_view msg,
    size_t iterations, double freq_ghz) {

    std::vector<double> latencies;
    latencies.reserve(iterations);

    // Create buffer for parsing
    std::vector<char> buffer(msg.begin(), msg.end());

    // Warm up
    for (size_t i = 0; i < 1000; ++i) {
        auto result = fix44::ExecutionReport::from_buffer(
            std::span<const char>(buffer.data(), buffer.size()));
        (void)result;
    }

    // Benchmark
    for (size_t i = 0; i < iterations; ++i) {
        uint64_t start = rdtsc_start();

        auto result = fix44::ExecutionReport::from_buffer(
            std::span<const char>(buffer.data(), buffer.size()));
        (void)result;

        uint64_t end = rdtsc_end();
        latencies.push_back(cycles_to_ns(end - start, freq_ghz));
    }

    return calculate_stats(latencies);
}

// Benchmark SBE encode
BenchmarkStats benchmark_sbe_encode(
    size_t iterations, double freq_ghz) {

    std::vector<double> latencies;
    latencies.reserve(iterations);

    alignas(8) char buffer[sbe::ExecutionReportCodec::TOTAL_SIZE];

    FixedPrice price, lastPx, avgPx;
    price.raw = 15050000000LL;
    lastPx.raw = 15055000000LL;
    avgPx.raw = 15052500000LL;

    Qty orderQty, lastQty, leavesQty, cumQty;
    orderQty.raw = 1000000LL;
    lastQty.raw = 500000LL;
    leavesQty.raw = 500000LL;
    cumQty.raw = 500000LL;

    Timestamp ts{1705315800123000000LL};

    // Warm up
    for (size_t i = 0; i < 1000; ++i) {
        sbe::ExecutionReportCodec::wrapForEncode(buffer, sizeof(buffer))
            .encodeHeader()
            .orderId("EX001")
            .execId("EXEC001")
            .clOrdId("ORD001")
            .symbol("AAPL")
            .side(Side::Buy)
            .execType(ExecType::PartialFill)
            .ordStatus(OrdStatus::PartiallyFilled)
            .price(price)
            .orderQty(orderQty)
            .lastPx(lastPx)
            .lastQty(lastQty)
            .leavesQty(leavesQty)
            .cumQty(cumQty)
            .avgPx(avgPx)
            .transactTime(ts);
    }

    // Benchmark
    for (size_t i = 0; i < iterations; ++i) {
        uint64_t start = rdtsc_start();

        sbe::ExecutionReportCodec::wrapForEncode(buffer, sizeof(buffer))
            .encodeHeader()
            .orderId("EX001")
            .execId("EXEC001")
            .clOrdId("ORD001")
            .symbol("AAPL")
            .side(Side::Buy)
            .execType(ExecType::PartialFill)
            .ordStatus(OrdStatus::PartiallyFilled)
            .price(price)
            .orderQty(orderQty)
            .lastPx(lastPx)
            .lastQty(lastQty)
            .leavesQty(leavesQty)
            .cumQty(cumQty)
            .avgPx(avgPx)
            .transactTime(ts);

        uint64_t end = rdtsc_end();
        latencies.push_back(cycles_to_ns(end - start, freq_ghz));
    }

    return calculate_stats(latencies);
}

// Benchmark dispatch
BenchmarkStats benchmark_sbe_dispatch(
    const char* buffer, size_t length,
    size_t iterations, double freq_ghz) {

    std::vector<double> latencies;
    latencies.reserve(iterations);

    // Warm up
    for (size_t i = 0; i < 1000; ++i) {
        sbe::dispatch(buffer, length, [](auto& codec) {
            using T = std::decay_t<decltype(codec)>;
            if constexpr (std::is_same_v<T, sbe::ExecutionReportCodec>) {
                volatile auto symbol = codec.symbol();
                (void)symbol;
            }
        });
    }

    // Benchmark
    for (size_t i = 0; i < iterations; ++i) {
        uint64_t start = rdtsc_start();

        sbe::dispatch(buffer, length, [](auto& codec) {
            using T = std::decay_t<decltype(codec)>;
            if constexpr (std::is_same_v<T, sbe::ExecutionReportCodec>) {
                volatile auto symbol = codec.symbol();
                (void)symbol;
            }
        });

        uint64_t end = rdtsc_end();
        latencies.push_back(cycles_to_ns(end - start, freq_ghz));
    }

    return calculate_stats(latencies);
}

}  // namespace nfx::bench

int main() {
    using namespace nfx::bench;

    std::cout << "========================================\n";
    std::cout << "NexusFIX SBE Binary Encoding Benchmark\n";
    std::cout << "========================================\n\n";

    // Calibrate CPU frequency
    std::cout << "Calibrating CPU frequency...\n";
    double freq_ghz = get_cpu_freq_ghz();
    std::cout << "CPU Frequency: " << std::fixed << std::setprecision(3)
              << freq_ghz << " GHz\n";

    // Prepare SBE buffer
    alignas(8) char sbe_buffer[nfx::sbe::ExecutionReportCodec::TOTAL_SIZE];
    prepare_sbe_exec_report(sbe_buffer, sizeof(sbe_buffer));

    constexpr size_t ITERATIONS = 100000;

    std::cout << "\n----------------------------------------\n";
    std::cout << "SBE Decode Benchmarks (Target: <10ns P50)\n";
    std::cout << "----------------------------------------\n";

    // SBE single field decode
    auto sbe_single = benchmark_sbe_decode_single_field(
        sbe_buffer, sizeof(sbe_buffer), ITERATIONS, freq_ghz);
    print_stats("SBE Decode (single field)", sbe_single);

    // SBE all fields decode
    auto sbe_all = benchmark_sbe_decode_all_fields(
        sbe_buffer, sizeof(sbe_buffer), ITERATIONS, freq_ghz);
    print_stats("SBE Decode (all 15 fields)", sbe_all);

    // SBE dispatch + decode
    auto sbe_dispatch = benchmark_sbe_dispatch(
        sbe_buffer, sizeof(sbe_buffer), ITERATIONS, freq_ghz);
    print_stats("SBE Dispatch + Decode", sbe_dispatch);

    // SBE encode
    auto sbe_encode = benchmark_sbe_encode(ITERATIONS, freq_ghz);
    print_stats("SBE Encode (all fields)", sbe_encode);

    std::cout << "\n----------------------------------------\n";
    std::cout << "FIX Text Parsing Comparison\n";
    std::cout << "----------------------------------------\n";

    // FIX text parse
    auto fix_text = benchmark_fix_text_parse(SAMPLE_EXEC_REPORT, ITERATIONS, freq_ghz);
    print_stats("FIX Text Parse (ExecutionReport)", fix_text);

    // Summary
    std::cout << "\n========================================\n";
    std::cout << "Performance Summary\n";
    std::cout << "========================================\n\n";

    double speedup = fix_text.p50_ns / sbe_all.p50_ns;

    std::cout << std::fixed << std::setprecision(1);
    std::cout << "SBE Decode P50:     " << std::setw(8) << sbe_all.p50_ns << " ns\n";
    std::cout << "FIX Text Parse P50: " << std::setw(8) << fix_text.p50_ns << " ns\n";
    std::cout << "Speedup:            " << std::setw(8) << speedup << "x\n";

    // Target check
    std::cout << "\n";
    if (sbe_all.p50_ns < 10.0) {
        std::cout << "[PASS] SBE decode P50 < 10ns target\n";
    } else {
        std::cout << "[WARN] SBE decode P50 > 10ns target\n";
    }

    if (speedup >= 20.0) {
        std::cout << "[PASS] Speedup >= 20x target\n";
    } else if (speedup >= 10.0) {
        std::cout << "[WARN] Speedup between 10-20x\n";
    } else {
        std::cout << "[FAIL] Speedup < 10x\n";
    }

    return 0;
}
