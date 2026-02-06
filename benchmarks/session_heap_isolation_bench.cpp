// TICKET_210b: SessionHeap Isolation Benchmark
//
// Controlled A/B/C comparison to determine optimal allocation strategy.
// All three strategies use IDENTICAL loop structure (no ptrs vector, no
// extra writes) and each has its own isolated warmup phase.
//
// Build: cmake --build build --target session_heap_isolation_bench
// Run:   ./build/bin/benchmarks/session_heap_isolation_bench

#include <iostream>
#include <iomanip>
#include <vector>
#include <cstring>
#include <memory_resource>

#include "benchmark_utils.hpp"
#include "nexusfix/memory/mimalloc_resource.hpp"
#include "nexusfix/memory/buffer_pool.hpp"

using namespace nfx::bench;
using namespace nfx::memory;

constexpr int WARMUP_ROUNDS = 50;
constexpr int BENCH_ROUNDS = 1000;
constexpr size_t MESSAGE_SIZE = 256;
constexpr size_t POOL_SIZE = 64 * 1024 * 1024;  // 64MB

constexpr size_t BURST_SIZES[] = {100, 500, 1000, 5000, 10000};

constexpr char SAMPLE_MESSAGE[] =
    "8=FIX.4.4\x01" "9=150\x01" "35=8\x01" "49=SENDER\x01" "56=TARGET\x01"
    "34=1\x01" "52=20260206-12:00:00.000\x01" "37=ORDER123\x01" "11=CLIENT456\x01"
    "17=EXEC789\x01" "150=0\x01" "39=0\x01" "55=AAPL\x01" "54=1\x01"
    "38=100\x01" "44=150.50\x01" "14=0\x01" "151=100\x01" "6=0\x01" "10=123\x01";

void print_row(const char* label, double p50, double p99) {
    std::cout << "  " << std::left << std::setw(28) << label
              << std::right << std::fixed << std::setprecision(1)
              << std::setw(8) << p50 << " ns"
              << std::setw(10) << p99 << " ns\n";
}

// Strategy A: std::vector<char> + monotonic_buffer_resource (existing baseline)
void bench_monotonic_standalone(size_t burst_count, double freq_ghz,
                                double& out_p50, double& out_p99) {
    // Warmup with own heap
    for (int i = 0; i < WARMUP_ROUNDS; ++i) {
        std::vector<char> ws(POOL_SIZE);
        std::pmr::monotonic_buffer_resource wp(
            ws.data(), ws.size(), std::pmr::null_memory_resource());
        for (size_t j = 0; j < burst_count; ++j) {
            void* p = wp.allocate(MESSAGE_SIZE, alignof(std::max_align_t));
            std::memcpy(p, SAMPLE_MESSAGE, sizeof(SAMPLE_MESSAGE) - 1);
            asm volatile("" :: "r"(p));
        }
    }

    std::vector<uint64_t> cycles;
    cycles.reserve(BENCH_ROUNDS);
    for (int i = 0; i < BENCH_ROUNDS; ++i) {
        std::vector<char> storage(POOL_SIZE);
        std::pmr::monotonic_buffer_resource pool(
            storage.data(), storage.size(), std::pmr::null_memory_resource());

        uint64_t start = rdtsc_vm_safe();
        for (size_t j = 0; j < burst_count; ++j) {
            void* p = pool.allocate(MESSAGE_SIZE, alignof(std::max_align_t));
            std::memcpy(p, SAMPLE_MESSAGE, sizeof(SAMPLE_MESSAGE) - 1);
            asm volatile("" :: "r"(p));
        }
        uint64_t end = rdtsc_vm_safe();
        cycles.push_back(end - start);
    }

    LatencyStats s;
    s.compute(cycles, freq_ghz);
    out_p50 = s.p50_ns / static_cast<double>(burst_count);
    out_p99 = s.p99_ns / static_cast<double>(burst_count);
}

// Strategy B: MimallocMemoryResource only (no monotonic layer)
void bench_mimalloc_only(size_t burst_count, double freq_ghz,
                         double& out_p50, double& out_p99) {
    // Warmup with own heap
    for (int i = 0; i < WARMUP_ROUNDS; ++i) {
        MimallocMemoryResource wr;
        for (size_t j = 0; j < burst_count; ++j) {
            void* p = wr.allocate(MESSAGE_SIZE, alignof(std::max_align_t));
            std::memcpy(p, SAMPLE_MESSAGE, sizeof(SAMPLE_MESSAGE) - 1);
            asm volatile("" :: "r"(p));
        }
    }

    std::vector<uint64_t> cycles;
    cycles.reserve(BENCH_ROUNDS);
    for (int i = 0; i < BENCH_ROUNDS; ++i) {
        MimallocMemoryResource resource;

        uint64_t start = rdtsc_vm_safe();
        for (size_t j = 0; j < burst_count; ++j) {
            void* p = resource.allocate(MESSAGE_SIZE, alignof(std::max_align_t));
            std::memcpy(p, SAMPLE_MESSAGE, sizeof(SAMPLE_MESSAGE) - 1);
            asm volatile("" :: "r"(p));
        }
        uint64_t end = rdtsc_vm_safe();
        cycles.push_back(end - start);
    }

    LatencyStats s;
    s.compute(cycles, freq_ghz);
    out_p50 = s.p50_ns / static_cast<double>(burst_count);
    out_p99 = s.p99_ns / static_cast<double>(burst_count);
}

// Strategy C: SessionHeap (monotonic bump + mimalloc upstream)
void bench_session_heap(size_t burst_count, double freq_ghz,
                        double& out_p50, double& out_p99) {
    // Warmup with own heap
    for (int i = 0; i < WARMUP_ROUNDS; ++i) {
        SessionHeap ws(POOL_SIZE);
        for (size_t j = 0; j < burst_count; ++j) {
            void* p = ws.allocate(MESSAGE_SIZE, alignof(std::max_align_t));
            std::memcpy(p, SAMPLE_MESSAGE, sizeof(SAMPLE_MESSAGE) - 1);
            asm volatile("" :: "r"(p));
        }
    }

    std::vector<uint64_t> cycles;
    cycles.reserve(BENCH_ROUNDS);
    for (int i = 0; i < BENCH_ROUNDS; ++i) {
        SessionHeap session(POOL_SIZE);

        uint64_t start = rdtsc_vm_safe();
        for (size_t j = 0; j < burst_count; ++j) {
            void* p = session.allocate(MESSAGE_SIZE, alignof(std::max_align_t));
            std::memcpy(p, SAMPLE_MESSAGE, sizeof(SAMPLE_MESSAGE) - 1);
            asm volatile("" :: "r"(p));
        }
        uint64_t end = rdtsc_vm_safe();
        cycles.push_back(end - start);
    }

    LatencyStats s;
    s.compute(cycles, freq_ghz);
    out_p50 = s.p50_ns / static_cast<double>(burst_count);
    out_p99 = s.p99_ns / static_cast<double>(burst_count);
}

// Strategy D: mimalloc pre-allocated buffer + monotonic (manual SessionHeap without PMR wrapper)
void bench_mimalloc_monotonic_direct(size_t burst_count, double freq_ghz,
                                     double& out_p50, double& out_p99) {
    // Warmup
    for (int i = 0; i < WARMUP_ROUNDS; ++i) {
        MimallocMemoryResource wr;
        void* buf = wr.allocate(POOL_SIZE, 64);
        std::pmr::monotonic_buffer_resource wp(buf, POOL_SIZE, &wr);
        for (size_t j = 0; j < burst_count; ++j) {
            void* p = wp.allocate(MESSAGE_SIZE, alignof(std::max_align_t));
            std::memcpy(p, SAMPLE_MESSAGE, sizeof(SAMPLE_MESSAGE) - 1);
            asm volatile("" :: "r"(p));
        }
    }

    std::vector<uint64_t> cycles;
    cycles.reserve(BENCH_ROUNDS);
    for (int i = 0; i < BENCH_ROUNDS; ++i) {
        MimallocMemoryResource heap;
        void* buf = heap.allocate(POOL_SIZE, 64);
        std::pmr::monotonic_buffer_resource pool(buf, POOL_SIZE, &heap);

        uint64_t start = rdtsc_vm_safe();
        for (size_t j = 0; j < burst_count; ++j) {
            void* p = pool.allocate(MESSAGE_SIZE, alignof(std::max_align_t));
            std::memcpy(p, SAMPLE_MESSAGE, sizeof(SAMPLE_MESSAGE) - 1);
            asm volatile("" :: "r"(p));
        }
        uint64_t end = rdtsc_vm_safe();
        cycles.push_back(end - start);
    }

    LatencyStats s;
    s.compute(cycles, freq_ghz);
    out_p50 = s.p50_ns / static_cast<double>(burst_count);
    out_p99 = s.p99_ns / static_cast<double>(burst_count);
}

int main() {
    std::cout << "==========================================================\n";
    std::cout << "  SessionHeap Isolation Benchmark (TICKET_210b)\n";
    std::cout << "==========================================================\n\n";

    (void)bind_to_core(2);

    std::cout << "Calibrating CPU frequency...\n";
    double freq_ghz = estimate_cpu_freq_ghz_busy();
    std::cout << "  CPU frequency: " << std::fixed << std::setprecision(3)
              << freq_ghz << " GHz\n\n";

    std::cout << "Strategies:\n";
    std::cout << "  A: Monotonic-only (std::vector<char> + monotonic_buffer_resource)\n";
    std::cout << "  B: mimalloc-only (MimallocMemoryResource)\n";
    std::cout << "  C: SessionHeap (monotonic + mimalloc upstream, via PMR wrapper)\n";
    std::cout << "  D: mimalloc + monotonic direct (no extra PMR wrapper layer)\n";
    std::cout << "\n  Each strategy has isolated warmup. Identical loop structure.\n";
    std::cout << "  Message size: " << MESSAGE_SIZE << " bytes, Pool: "
              << (POOL_SIZE / 1024 / 1024) << " MB, Rounds: " << BENCH_ROUNDS << "\n";

    for (size_t burst : BURST_SIZES) {
        std::cout << "\n----------------------------------------------------------\n";
        std::cout << "  Burst size: " << burst << " messages\n";
        std::cout << "----------------------------------------------------------\n";
        std::cout << "  " << std::left << std::setw(28) << "Strategy"
                  << std::right << std::setw(11) << "P50/msg"
                  << std::setw(13) << "P99/msg" << "\n";
        std::cout << "  " << std::string(52, '-') << "\n";

        double a_p50, a_p99, b_p50, b_p99, c_p50, c_p99, d_p50, d_p99;

        bench_monotonic_standalone(burst, freq_ghz, a_p50, a_p99);
        print_row("A: Monotonic-only", a_p50, a_p99);

        bench_mimalloc_only(burst, freq_ghz, b_p50, b_p99);
        print_row("B: mimalloc-only", b_p50, b_p99);

        bench_session_heap(burst, freq_ghz, c_p50, c_p99);
        print_row("C: SessionHeap", c_p50, c_p99);

        bench_mimalloc_monotonic_direct(burst, freq_ghz, d_p50, d_p99);
        print_row("D: mimalloc+mono direct", d_p50, d_p99);

        // Find winner
        double min_p50 = a_p50;
        const char* winner = "A";
        if (b_p50 < min_p50) { min_p50 = b_p50; winner = "B"; }
        if (c_p50 < min_p50) { min_p50 = c_p50; winner = "C"; }
        if (d_p50 < min_p50) { min_p50 = d_p50; winner = "D"; }

        std::cout << "  " << std::string(52, '-') << "\n";
        std::cout << "  Winner (P50): " << winner << " at "
                  << std::fixed << std::setprecision(1) << min_p50 << " ns/msg\n";
    }

    std::cout << "\n==========================================================\n";
    std::cout << "  Strategy Legend\n";
    std::cout << "==========================================================\n";
    std::cout << "  A: Existing baseline (std::vector 64MB + monotonic + null upstream)\n";
    std::cout << "  B: mimalloc heap only (per-alloc mi_heap_malloc_aligned)\n";
    std::cout << "  C: SessionHeap PMR class (extra vtable hop: C.do_allocate -> pool.allocate)\n";
    std::cout << "  D: mimalloc buffer + monotonic directly (1 vtable hop: pool.allocate)\n";
    std::cout << "\n  If D consistently beats C, the SessionHeap PMR wrapper has overhead.\n";
    std::cout << "  If B beats C/D, monotonic layer adds no value.\n";
    std::cout << "==========================================================\n";

    return 0;
}
