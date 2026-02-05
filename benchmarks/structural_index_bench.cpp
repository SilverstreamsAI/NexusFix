// structural_index_bench.cpp
// TICKET_208: simdjson-style Two-Stage Structural Indexing Benchmark
//
// Uses nfx::bench utilities (benchmark_utils.hpp):
//   - rdtsc_vm_safe() for timing
//   - LatencyStats for statistics
//   - ScopedTimer for RAII measurement
//   - warmup_icache() for I-Cache warming
//   - print_comparison() for before/after output
//
// Comparison: IndexedParser (before) vs StructuralIndex (after)

#include <iostream>
#include <iomanip>
#include <vector>
#include <cstdint>

#include "nexusfix/nexusfix.hpp"
#include "nexusfix/parser/structural_index.hpp"
#include "benchmark_utils.hpp"

using namespace nfx;
using namespace nfx::bench;

// ============================================================================
// Test Message Builder
// ============================================================================

static std::string build_fix_message(std::string_view body) {
    std::string msg(body);

    uint32_t sum = 0;
    for (char c : msg) {
        sum += static_cast<uint8_t>(c);
    }
    uint8_t checksum = static_cast<uint8_t>(sum % 256);

    char cs[8];
    cs[0] = '1';
    cs[1] = '0';
    cs[2] = '=';
    cs[3] = static_cast<char>('0' + (checksum / 100));
    cs[4] = static_cast<char>('0' + ((checksum / 10) % 10));
    cs[5] = static_cast<char>('0' + (checksum % 10));
    cs[6] = '\x01';
    cs[7] = '\0';

    msg += cs;
    return msg;
}

// ============================================================================
// Test Messages
// ============================================================================

static constexpr std::string_view EXEC_REPORT_BODY =
    "8=FIX.4.4\x01"
    "9=200\x01"
    "35=8\x01"
    "49=SENDER\x01"
    "56=TARGET\x01"
    "34=12345\x01"
    "52=20240115-10:30:00.123\x01"
    "37=ORD123456\x01"
    "17=EXEC789012\x01"
    "150=0\x01"
    "39=0\x01"
    "54=1\x01"
    "151=1000\x01"
    "14=0\x01"
    "6=0\x01"
    "55=AAPL\x01"
    "38=1000\x01"
    "44=150.50\x01";

static constexpr std::string_view HEARTBEAT_BODY =
    "8=FIX.4.4\x01"
    "9=60\x01"
    "35=0\x01"
    "49=SENDER\x01"
    "56=TARGET\x01"
    "34=50\x01"
    "52=20240115-10:30:00.000\x01";

static constexpr std::string_view NEW_ORDER_BODY =
    "8=FIX.4.4\x01"
    "9=150\x01"
    "35=D\x01"
    "49=SENDER\x01"
    "56=TARGET\x01"
    "34=100\x01"
    "52=20240115-10:30:00.000\x01"
    "11=CLORD001\x01"
    "55=AAPL\x01"
    "54=1\x01"
    "60=20240115-10:30:00.000\x01"
    "38=1000\x01"
    "40=2\x01"
    "44=150.00\x01"
    "59=0\x01";

// ============================================================================
// Print helpers
// ============================================================================

static void print_stats(const char* name, const LatencyStats& stats) {
    std::cout << "\n=== " << name << " ===\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Iterations: " << stats.count << "\n";
    std::cout << "  Min:    " << std::setw(10) << stats.min_ns << " ns\n";
    std::cout << "  Mean:   " << std::setw(10) << stats.mean_ns << " ns\n";
    std::cout << "  P50:    " << std::setw(10) << stats.p50_ns << " ns\n";
    std::cout << "  P90:    " << std::setw(10) << stats.p90_ns << " ns\n";
    std::cout << "  P99:    " << std::setw(10) << stats.p99_ns << " ns\n";
    std::cout << "  P99.9:  " << std::setw(10) << stats.p999_ns << " ns\n";
    std::cout << "  Max:    " << std::setw(10) << stats.max_ns << " ns\n";
    std::cout << "  StdDev: " << std::setw(10) << stats.stddev_ns << " ns\n";
}

// ============================================================================
// Benchmarks
// ============================================================================

/// Stage 1: Scalar build_index
static LatencyStats bench_build_index_scalar(
    std::span<const char> data, size_t iterations, double freq_ghz)
{
    std::vector<uint64_t> cycles;
    cycles.reserve(iterations);

    // Warmup with compiler barriers
    warmup_icache([&]() {
        auto idx = simd::build_index_scalar(data);
        compiler_barrier();
        (void)idx;
    });

    // Benchmark
    for (size_t i = 0; i < iterations; ++i) {
        uint64_t elapsed;
        {
            ScopedTimer timer(elapsed);
            auto idx = simd::build_index_scalar(data);
            compiler_barrier();
            (void)idx;
        }
        cycles.push_back(elapsed);
    }

    LatencyStats stats;
    stats.compute(cycles, freq_ghz);
    return stats;
}

/// Stage 1: Runtime-dispatched build_index
static LatencyStats bench_build_index_dispatch(
    std::span<const char> data, size_t iterations, double freq_ghz)
{
    std::vector<uint64_t> cycles;
    cycles.reserve(iterations);

    warmup_icache([&]() {
        auto idx = simd::build_index(data);
        compiler_barrier();
        (void)idx;
    });

    for (size_t i = 0; i < iterations; ++i) {
        uint64_t elapsed;
        {
            ScopedTimer timer(elapsed);
            auto idx = simd::build_index(data);
            compiler_barrier();
            (void)idx;
        }
        cycles.push_back(elapsed);
    }

    LatencyStats stats;
    stats.compute(cycles, freq_ghz);
    return stats;
}

/// Stage 2: Field extraction by tag (linear search)
static LatencyStats bench_field_extraction(
    std::span<const char> data, size_t iterations, double freq_ghz)
{
    auto idx = simd::build_index(data);
    simd::IndexedFieldAccessor accessor{idx, data};

    std::vector<uint64_t> cycles;
    cycles.reserve(iterations);

    warmup_icache([&]() {
        auto v = accessor.get(55);
        compiler_barrier();
        (void)v;
    });

    for (size_t i = 0; i < iterations; ++i) {
        uint64_t elapsed;
        {
            ScopedTimer timer(elapsed);
            auto symbol = accessor.get(55);
            auto order_id = accessor.get(37);
            auto side = accessor.get_char(54);
            auto msg_type = accessor.msg_type();
            compiler_barrier();
            (void)symbol; (void)order_id; (void)side; (void)msg_type;
        }
        cycles.push_back(elapsed);
    }

    LatencyStats stats;
    stats.compute(cycles, freq_ghz);
    return stats;
}

/// Stage 2: Direct field access by index (O(1))
static LatencyStats bench_field_by_index(
    std::span<const char> data, size_t iterations, double freq_ghz)
{
    auto idx = simd::build_index(data);

    std::vector<uint64_t> cycles;
    cycles.reserve(iterations);

    warmup_icache([&]() {
        auto v = idx.value_at(data, 0);
        compiler_barrier();
        (void)v;
    });

    for (size_t i = 0; i < iterations; ++i) {
        uint64_t elapsed;
        {
            ScopedTimer timer(elapsed);
            auto f0 = idx.value_at(data, 0);
            auto f2 = idx.value_at(data, 2);
            auto f7 = idx.value_at(data, 7);
            auto f15 = idx.value_at(data, 15);
            compiler_barrier();
            (void)f0; (void)f2; (void)f7; (void)f15;
        }
        cycles.push_back(elapsed);
    }

    LatencyStats stats;
    stats.compute(cycles, freq_ghz);
    return stats;
}

/// Full pipeline: build_index + extract 4 fields
static LatencyStats bench_full_pipeline(
    std::span<const char> data, size_t iterations, double freq_ghz)
{
    std::vector<uint64_t> cycles;
    cycles.reserve(iterations);

    warmup_icache([&]() {
        auto idx = simd::build_index(data);
        simd::IndexedFieldAccessor accessor{idx, data};
        auto v = accessor.get(55);
        compiler_barrier();
        (void)v;
    });

    for (size_t i = 0; i < iterations; ++i) {
        uint64_t elapsed;
        {
            ScopedTimer timer(elapsed);
            auto idx = simd::build_index(data);
            simd::IndexedFieldAccessor accessor{idx, data};
            auto symbol = accessor.get(55);
            auto order_id = accessor.get(37);
            auto side = accessor.get_char(54);
            auto msg_type = accessor.msg_type();
            compiler_barrier();
            (void)symbol; (void)order_id; (void)side; (void)msg_type;
        }
        cycles.push_back(elapsed);
    }

    LatencyStats stats;
    stats.compute(cycles, freq_ghz);
    return stats;
}

/// IndexedParser::parse (before - baseline)
static LatencyStats bench_indexed_parser(
    std::span<const char> data, size_t iterations, double freq_ghz)
{
    std::vector<uint64_t> cycles;
    cycles.reserve(iterations);

    warmup_icache([&]() {
        auto r = IndexedParser::parse(data);
        compiler_barrier();
        (void)r;
    });

    for (size_t i = 0; i < iterations; ++i) {
        uint64_t elapsed;
        {
            ScopedTimer timer(elapsed);
            auto result = IndexedParser::parse(data);
            compiler_barrier();
            (void)result;
        }
        cycles.push_back(elapsed);
    }

    LatencyStats stats;
    stats.compute(cycles, freq_ghz);
    return stats;
}

/// Padded buffer build_index
static LatencyStats bench_padded_buffer(
    std::span<const char> data, size_t iterations, double freq_ghz)
{
    simd::MediumPaddedBuffer padded;
    padded.set(data);

    std::vector<uint64_t> cycles;
    cycles.reserve(iterations);

    warmup_icache([&]() {
        auto idx = simd::build_index(padded.data());
        compiler_barrier();
        (void)idx;
    });

    for (size_t i = 0; i < iterations; ++i) {
        uint64_t elapsed;
        {
            ScopedTimer timer(elapsed);
            auto idx = simd::build_index(padded.data());
            compiler_barrier();
            (void)idx;
        }
        cycles.push_back(elapsed);
    }

    LatencyStats stats;
    stats.compute(cycles, freq_ghz);
    return stats;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    size_t iterations = 100000;

    if (argc > 1) {
        iterations = std::stoul(argv[1]);
    }

    // Initialize runtime SIMD dispatch
    simd::init_simd_dispatch();

    std::cout << "==========================================================\n";
    std::cout << "  NexusFIX Structural Index Benchmark (TICKET_208)\n";
    std::cout << "==========================================================\n\n";

    std::cout << "Configuration:\n";
    std::cout << "  Iterations: " << iterations << "\n";
    std::cout << "  SIMD Impl:  "
              << simd::simd_impl_name(simd::active_simd_impl()) << "\n";
    std::cout << "  Timing:     rdtsc_vm_safe (lfence serialized)\n";

    // Calibrate CPU frequency (busy-wait for accuracy)
    std::cout << "\nCalibrating CPU frequency (busy-wait)...\n";
    double freq_ghz = estimate_cpu_freq_ghz_busy();
    std::cout << "  CPU frequency: " << std::fixed << std::setprecision(3)
              << freq_ghz << " GHz\n";

    // Build test messages
    std::string exec_msg = build_fix_message(EXEC_REPORT_BODY);
    std::string hb_msg = build_fix_message(HEARTBEAT_BODY);
    std::string nos_msg = build_fix_message(NEW_ORDER_BODY);

    std::span<const char> exec_data{exec_msg.data(), exec_msg.size()};
    std::span<const char> hb_data{hb_msg.data(), hb_msg.size()};
    std::span<const char> nos_data{nos_msg.data(), nos_msg.size()};

    // Warm up data cache
    warmup_dcache(const_cast<char*>(exec_msg.data()), exec_msg.size());

    // ========================================================================
    // Stage 1: Structural Indexing
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Stage 1: Structural Indexing (build_index)\n";
    std::cout << "----------------------------------------------------------\n";

    auto scalar_stats = bench_build_index_scalar(exec_data, iterations, freq_ghz);
    print_stats("build_index_scalar (ExecutionReport)", scalar_stats);

    auto dispatch_stats = bench_build_index_dispatch(exec_data, iterations, freq_ghz);
    std::string dispatch_name = "build_index [";
    dispatch_name += simd::simd_impl_name(simd::active_simd_impl());
    dispatch_name += "] (ExecutionReport)";
    print_stats(dispatch_name.c_str(), dispatch_stats);

    // ========================================================================
    // Stage 2: Field Extraction
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Stage 2: Field Extraction\n";
    std::cout << "----------------------------------------------------------\n";

    auto extract_stats = bench_field_extraction(exec_data, iterations, freq_ghz);
    print_stats("Field Extraction (4 fields by tag)", extract_stats);

    auto index_stats = bench_field_by_index(exec_data, iterations, freq_ghz);
    print_stats("Direct Field Access (4 fields by index)", index_stats);

    // ========================================================================
    // Full Pipeline
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Full Pipeline: build_index + extract\n";
    std::cout << "----------------------------------------------------------\n";

    auto pipeline_stats = bench_full_pipeline(exec_data, iterations, freq_ghz);
    print_stats("Full Pipeline (build + extract 4 fields)", pipeline_stats);

    // ========================================================================
    // Before vs After: IndexedParser vs StructuralIndex
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Before vs After: IndexedParser vs StructuralIndex\n";
    std::cout << "----------------------------------------------------------\n";

    auto parser_stats = bench_indexed_parser(exec_data, iterations, freq_ghz);
    print_stats("IndexedParser::parse (Before)", parser_stats);
    print_stats("StructuralIndex::build_index (After)", dispatch_stats);

    std::cout << "\n";
    print_comparison_header("IndexedParser", "StructIndex");
    print_comparison("Mean", parser_stats.mean_ns, dispatch_stats.mean_ns);
    print_comparison("P50", parser_stats.p50_ns, dispatch_stats.p50_ns);
    print_comparison("P99", parser_stats.p99_ns, dispatch_stats.p99_ns);
    print_comparison("P99.9", parser_stats.p999_ns, dispatch_stats.p999_ns);

    // ========================================================================
    // Padded Buffer
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Buffer Strategy\n";
    std::cout << "----------------------------------------------------------\n";

    auto padded_stats = bench_padded_buffer(exec_data, iterations, freq_ghz);

    std::cout << "\n";
    print_comparison_header("Unpadded", "Padded");
    print_comparison("Mean", dispatch_stats.mean_ns, padded_stats.mean_ns);
    print_comparison("P50", dispatch_stats.p50_ns, padded_stats.p50_ns);
    print_comparison("P99", dispatch_stats.p99_ns, padded_stats.p99_ns);

    // ========================================================================
    // Message Size Scaling
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Message Size Scaling\n";
    std::cout << "----------------------------------------------------------\n";

    struct MsgCase {
        const char* name;
        std::span<const char> data;
        size_t msg_size;
    };

    MsgCase cases[] = {
        {"Heartbeat", hb_data, hb_msg.size()},
        {"NewOrderSingle", nos_data, nos_msg.size()},
        {"ExecutionReport", exec_data, exec_msg.size()},
    };

    std::cout << "\n  " << std::left << std::setw(20) << "Message"
              << std::right << std::setw(6) << "Size"
              << std::setw(12) << "P50"
              << std::setw(12) << "P99"
              << std::setw(12) << "Throughput" << "\n";
    std::cout << "  " << std::string(62, '-') << "\n";

    for (const auto& tc : cases) {
        auto stats = bench_build_index_dispatch(tc.data, iterations, freq_ghz);
        double throughput_gbps = (static_cast<double>(tc.msg_size) * 1e9) /
                                 (stats.p50_ns * 1e9);

        std::cout << "  " << std::left << std::setw(20) << tc.name
                  << std::right << std::setw(5) << tc.msg_size << "B"
                  << std::setw(10) << std::fixed << std::setprecision(1) << stats.p50_ns << " ns"
                  << std::setw(10) << stats.p99_ns << " ns"
                  << std::setw(10) << std::setprecision(2) << throughput_gbps << " GB/s\n";
    }

    // ========================================================================
    // Summary
    // ========================================================================

    std::cout << "\n==========================================================\n";
    std::cout << "  TICKET_208 Performance Targets\n";
    std::cout << "==========================================================\n\n";

    std::cout << std::fixed << std::setprecision(1);
    std::cout << "  SOH scanning:        " << std::setw(8) << dispatch_stats.p50_ns
              << " ns  (target: < 100 ns)  "
              << (dispatch_stats.p50_ns < 100 ? "PASS" : "FAIL") << "\n";
    std::cout << "  Field indexing:      " << std::setw(8) << dispatch_stats.p50_ns
              << " ns  (target: <  50 ns)  "
              << (dispatch_stats.p50_ns < 50 ? "PASS" : "FAIL") << "\n";
    std::cout << "  Per-field extract:   " << std::setw(8) << (index_stats.p50_ns / 4.0)
              << " ns  (target: <  20 ns)  "
              << ((index_stats.p50_ns / 4.0) < 20 ? "PASS" : "FAIL") << "\n";

    double speedup = parser_stats.p50_ns / dispatch_stats.p50_ns;
    std::cout << "\n  Speedup vs IndexedParser: " << std::setprecision(1) << speedup << "x\n";
    std::cout << "==========================================================\n";

    return 0;
}
