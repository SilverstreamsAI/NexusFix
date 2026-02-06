// Benchmark: PMR Monotonic Buffer vs mimalloc Per-Session Heap
//
// Compares allocation latency, burst allocation, deallocation,
// and cleanup performance between std::pmr::monotonic_buffer_resource
// and nfx::memory::MimallocMemoryResource.
//
// Build: cmake -DNFX_ENABLE_MIMALLOC=ON --build build --target mimalloc_bench
// Run:   ./build/bin/benchmarks/mimalloc_bench

#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <cstring>
#include <memory_resource>
#include <thread>

#include "benchmark_utils.hpp"
#include "nexusfix/memory/mimalloc_resource.hpp"
#include "nexusfix/memory/buffer_pool.hpp"

using namespace nfx::bench;
using namespace nfx::memory;

// Benchmark configuration
constexpr int WARMUP_ITERATIONS = 10000;
constexpr int BENCHMARK_ITERATIONS = 100000;
constexpr size_t MESSAGE_SIZE = 256;   // Typical FIX message size
constexpr size_t PMR_POOL_SIZE = 64 * 1024 * 1024;  // 64MB pool
constexpr size_t BURST_COUNT = 1000;   // Messages per burst

// Sample FIX ExecutionReport
constexpr char SAMPLE_MESSAGE[] =
    "8=FIX.4.4\x01" "9=150\x01" "35=8\x01" "49=SENDER\x01" "56=TARGET\x01"
    "34=1\x01" "52=20260206-12:00:00.000\x01" "37=ORDER123\x01" "11=CLIENT456\x01"
    "17=EXEC789\x01" "150=0\x01" "39=0\x01" "55=AAPL\x01" "54=1\x01"
    "38=100\x01" "44=150.50\x01" "14=0\x01" "151=100\x01" "6=0\x01" "10=123\x01";

void print_stats(const char* label, const LatencyStats& s) {
    std::cout << "  " << label << ":\n";
    std::cout << "    Min:    " << std::fixed << std::setprecision(1) << s.min_ns << " ns\n";
    std::cout << "    Mean:   " << s.mean_ns << " ns\n";
    std::cout << "    P50:    " << s.p50_ns << " ns\n";
    std::cout << "    P99:    " << s.p99_ns << " ns\n";
    std::cout << "    P99.9:  " << s.p999_ns << " ns\n";
    std::cout << "    Max:    " << s.max_ns << " ns\n";
}

int main() {
    std::cout << "==========================================================\n";
    std::cout << "  mimalloc vs PMR Monotonic Buffer Benchmark\n";
    std::cout << "==========================================================\n\n";

    // Pin to core for consistent results
    (void)bind_to_core(2);

    std::cout << "Calibrating CPU frequency...\n";
    double freq_ghz = estimate_cpu_freq_ghz_busy();
    std::cout << "  CPU frequency: " << std::fixed << std::setprecision(3)
              << freq_ghz << " GHz\n";

    std::cout << "\nConfiguration:\n";
    std::cout << "  Warmup:       " << WARMUP_ITERATIONS << " iterations\n";
    std::cout << "  Benchmark:    " << BENCHMARK_ITERATIONS << " iterations\n";
    std::cout << "  Message size: " << MESSAGE_SIZE << " bytes\n";
    std::cout << "  PMR pool:     " << (PMR_POOL_SIZE / 1024 / 1024) << " MB\n";
    std::cout << "  Burst count:  " << BURST_COUNT << " messages\n";

    // ========================================================================
    // Test 1: Single Allocation Latency
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Test 1: Single Allocation Latency (" << MESSAGE_SIZE << " bytes)\n";
    std::cout << "----------------------------------------------------------\n";

    // BEFORE: PMR monotonic buffer
    {
        std::vector<char> pool_storage(PMR_POOL_SIZE);
        std::pmr::monotonic_buffer_resource pool(
            pool_storage.data(), pool_storage.size(),
            std::pmr::null_memory_resource());

        // Warmup
        for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
            compiler_barrier();
            void* p = pool.allocate(MESSAGE_SIZE, alignof(std::max_align_t));
            compiler_barrier();
            asm volatile("" :: "r"(p));
        }
        pool.release();

        // Measure
        std::vector<uint64_t> cycles;
        cycles.reserve(BENCHMARK_ITERATIONS);

        for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
            uint64_t start = rdtsc_vm_safe();
            void* p = pool.allocate(MESSAGE_SIZE, alignof(std::max_align_t));
            uint64_t end = rdtsc_vm_safe();
            asm volatile("" :: "r"(p));
            cycles.push_back(end - start);
        }

        LatencyStats pmr_stats;
        pmr_stats.compute(cycles, freq_ghz);
        print_stats("BEFORE: PMR monotonic", pmr_stats);
    }

    // AFTER: mimalloc heap
    {
        MimallocMemoryResource resource;

        // Warmup
        for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
            compiler_barrier();
            void* p = resource.allocate(MESSAGE_SIZE, alignof(std::max_align_t));
            compiler_barrier();
            resource.deallocate(p, MESSAGE_SIZE, alignof(std::max_align_t));
        }

        // Measure
        std::vector<uint64_t> cycles;
        cycles.reserve(BENCHMARK_ITERATIONS);

        for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
            uint64_t start = rdtsc_vm_safe();
            void* p = resource.allocate(MESSAGE_SIZE, alignof(std::max_align_t));
            uint64_t end = rdtsc_vm_safe();
            asm volatile("" :: "r"(p));
            resource.deallocate(p, MESSAGE_SIZE, alignof(std::max_align_t));
            cycles.push_back(end - start);
        }

        LatencyStats mi_stats;
        mi_stats.compute(cycles, freq_ghz);
        print_stats("AFTER: mimalloc heap", mi_stats);
    }

    // ========================================================================
    // Test 2: Burst Allocation (1000 messages)
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Test 2: Burst Allocation (" << BURST_COUNT << " messages)\n";
    std::cout << "----------------------------------------------------------\n";

    constexpr int BURST_ITERATIONS = 1000;

    // BEFORE: PMR monotonic burst
    {
        std::vector<uint64_t> cycles;
        cycles.reserve(BURST_ITERATIONS);

        for (int iter = 0; iter < BURST_ITERATIONS; ++iter) {
            std::vector<char> pool_storage(PMR_POOL_SIZE);
            std::pmr::monotonic_buffer_resource pool(
                pool_storage.data(), pool_storage.size(),
                std::pmr::null_memory_resource());

            uint64_t start = rdtsc_vm_safe();
            for (size_t i = 0; i < BURST_COUNT; ++i) {
                void* p = pool.allocate(MESSAGE_SIZE, alignof(std::max_align_t));
                std::memcpy(p, SAMPLE_MESSAGE, sizeof(SAMPLE_MESSAGE) - 1);
                asm volatile("" :: "r"(p));
            }
            uint64_t end = rdtsc_vm_safe();
            cycles.push_back(end - start);
        }

        LatencyStats pmr_burst;
        pmr_burst.compute(cycles, freq_ghz);
        print_stats("BEFORE: PMR burst (total ns)", pmr_burst);
        std::cout << "    Per-msg: " << std::fixed << std::setprecision(1)
                  << pmr_burst.p50_ns / static_cast<double>(BURST_COUNT) << " ns\n";
    }

    // AFTER: mimalloc burst
    {
        std::vector<uint64_t> cycles;
        cycles.reserve(BURST_ITERATIONS);

        for (int iter = 0; iter < BURST_ITERATIONS; ++iter) {
            MimallocMemoryResource resource;
            std::vector<void*> ptrs;
            ptrs.reserve(BURST_COUNT);

            uint64_t start = rdtsc_vm_safe();
            for (size_t i = 0; i < BURST_COUNT; ++i) {
                void* p = resource.allocate(MESSAGE_SIZE, alignof(std::max_align_t));
                std::memcpy(p, SAMPLE_MESSAGE, sizeof(SAMPLE_MESSAGE) - 1);
                ptrs.push_back(p);
            }
            uint64_t end = rdtsc_vm_safe();
            cycles.push_back(end - start);
            // Resource destructor frees all via mi_heap_destroy
        }

        LatencyStats mi_burst;
        mi_burst.compute(cycles, freq_ghz);
        print_stats("AFTER: mimalloc burst (total ns)", mi_burst);
        std::cout << "    Per-msg: " << std::fixed << std::setprecision(1)
                  << mi_burst.p50_ns / static_cast<double>(BURST_COUNT) << " ns\n";
    }

    // ========================================================================
    // Test 3: Deallocation Latency
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Test 3: Individual Deallocation Latency\n";
    std::cout << "----------------------------------------------------------\n";

    {
        MimallocMemoryResource resource;

        // Pre-allocate blocks
        std::vector<void*> ptrs;
        ptrs.reserve(BENCHMARK_ITERATIONS);
        for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
            ptrs.push_back(resource.allocate(MESSAGE_SIZE, alignof(std::max_align_t)));
        }

        // Warmup
        for (int i = 0; i < WARMUP_ITERATIONS && i < BENCHMARK_ITERATIONS; ++i) {
            compiler_barrier();
        }

        // Measure deallocation
        std::vector<uint64_t> cycles;
        cycles.reserve(BENCHMARK_ITERATIONS);

        for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {
            void* p = ptrs[static_cast<size_t>(i)];
            uint64_t start = rdtsc_vm_safe();
            resource.deallocate(p, MESSAGE_SIZE, alignof(std::max_align_t));
            uint64_t end = rdtsc_vm_safe();
            cycles.push_back(end - start);
        }

        LatencyStats dealloc_stats;
        dealloc_stats.compute(cycles, freq_ghz);
        print_stats("mimalloc deallocation", dealloc_stats);
    }

    // ========================================================================
    // Test 4: Heap Destruction vs pool.release()
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Test 4: Cleanup - mi_heap_destroy() vs pool.release()\n";
    std::cout << "----------------------------------------------------------\n";

    constexpr int CLEANUP_ITERATIONS = 1000;
    constexpr size_t ALLOCS_PER_HEAP = 1000;

    // PMR pool.release()
    {
        std::vector<uint64_t> cycles;
        cycles.reserve(CLEANUP_ITERATIONS);

        for (int iter = 0; iter < CLEANUP_ITERATIONS; ++iter) {
            std::vector<char> pool_storage(PMR_POOL_SIZE);
            std::pmr::monotonic_buffer_resource pool(
                pool_storage.data(), pool_storage.size(),
                std::pmr::null_memory_resource());

            for (size_t i = 0; i < ALLOCS_PER_HEAP; ++i) {
                void* p = pool.allocate(MESSAGE_SIZE, alignof(std::max_align_t));
                asm volatile("" :: "r"(p));
            }

            uint64_t start = rdtsc_vm_safe();
            pool.release();
            uint64_t end = rdtsc_vm_safe();
            cycles.push_back(end - start);
        }

        LatencyStats release_stats;
        release_stats.compute(cycles, freq_ghz);
        print_stats("BEFORE: pool.release()", release_stats);
    }

    // mimalloc heap destruction
    {
        std::vector<uint64_t> cycles;
        cycles.reserve(CLEANUP_ITERATIONS);

        for (int iter = 0; iter < CLEANUP_ITERATIONS; ++iter) {
            auto* resource = new MimallocMemoryResource();

            for (size_t i = 0; i < ALLOCS_PER_HEAP; ++i) {
                void* p = resource->allocate(MESSAGE_SIZE, alignof(std::max_align_t));
                asm volatile("" :: "r"(p));
            }

            uint64_t start = rdtsc_vm_safe();
            delete resource;
            uint64_t end = rdtsc_vm_safe();
            cycles.push_back(end - start);
        }

        LatencyStats destroy_stats;
        destroy_stats.compute(cycles, freq_ghz);
        print_stats("AFTER: mi_heap_destroy()", destroy_stats);
    }

    // ========================================================================
    // Test 5: Multi-threaded Allocation (cross-thread free)
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Test 5: Multi-threaded Allocation (cross-thread free)\n";
    std::cout << "----------------------------------------------------------\n";

    constexpr int MT_ITERATIONS = 10000;

    {
        MimallocMemoryResource resource;
        std::vector<void*> ptrs(MT_ITERATIONS);

        // Allocate on main thread
        for (int i = 0; i < MT_ITERATIONS; ++i) {
            ptrs[static_cast<size_t>(i)] = resource.allocate(MESSAGE_SIZE, alignof(std::max_align_t));
        }

        // Free on another thread and measure
        std::vector<uint64_t> cycles;
        cycles.reserve(MT_ITERATIONS);

        std::thread worker([&]() {
            (void)bind_to_core(3);
            for (int i = 0; i < MT_ITERATIONS; ++i) {
                uint64_t start = rdtsc_vm_safe();
                resource.deallocate(ptrs[static_cast<size_t>(i)], MESSAGE_SIZE, alignof(std::max_align_t));
                uint64_t end = rdtsc_vm_safe();
                cycles.push_back(end - start);
            }
        });
        worker.join();

        LatencyStats mt_stats;
        mt_stats.compute(cycles, freq_ghz);
        print_stats("Cross-thread deallocation", mt_stats);
    }

    // ========================================================================
    // Test 6: Combined SessionHeap (Monotonic + mimalloc)
    // ========================================================================

    std::cout << "\n----------------------------------------------------------\n";
    std::cout << "  Test 6: Combined SessionHeap (Monotonic + mimalloc)\n";
    std::cout << "----------------------------------------------------------\n";

    // 6a: Burst allocation through SessionHeap (hot path)
    std::cout << "\n  6a: Burst allocation comparison (" << BURST_COUNT << " messages)\n\n";

    constexpr int SESSION_BURST_ITERATIONS = 1000;

    // Monotonic-only (standalone)
    {
        std::vector<uint64_t> cycles;
        cycles.reserve(SESSION_BURST_ITERATIONS);

        for (int iter = 0; iter < SESSION_BURST_ITERATIONS; ++iter) {
            std::vector<char> pool_storage(PMR_POOL_SIZE);
            std::pmr::monotonic_buffer_resource pool(
                pool_storage.data(), pool_storage.size(),
                std::pmr::null_memory_resource());

            uint64_t start = rdtsc_vm_safe();
            for (size_t i = 0; i < BURST_COUNT; ++i) {
                void* p = pool.allocate(MESSAGE_SIZE, alignof(std::max_align_t));
                std::memcpy(p, SAMPLE_MESSAGE, sizeof(SAMPLE_MESSAGE) - 1);
                asm volatile("" :: "r"(p));
            }
            uint64_t end = rdtsc_vm_safe();
            cycles.push_back(end - start);
        }

        LatencyStats mono_stats;
        mono_stats.compute(cycles, freq_ghz);
        print_stats("Monotonic-only burst (total ns)", mono_stats);
        std::cout << "    Per-msg: " << std::fixed << std::setprecision(1)
                  << mono_stats.p50_ns / static_cast<double>(BURST_COUNT) << " ns\n";
    }

    // SessionHeap (monotonic + mimalloc upstream)
    {
        std::vector<uint64_t> cycles;
        cycles.reserve(SESSION_BURST_ITERATIONS);

        for (int iter = 0; iter < SESSION_BURST_ITERATIONS; ++iter) {
            SessionHeap session(PMR_POOL_SIZE);

            uint64_t start = rdtsc_vm_safe();
            for (size_t i = 0; i < BURST_COUNT; ++i) {
                void* p = session.allocate(MESSAGE_SIZE, alignof(std::max_align_t));
                std::memcpy(p, SAMPLE_MESSAGE, sizeof(SAMPLE_MESSAGE) - 1);
                asm volatile("" :: "r"(p));
            }
            uint64_t end = rdtsc_vm_safe();
            cycles.push_back(end - start);
        }

        LatencyStats session_stats;
        session_stats.compute(cycles, freq_ghz);
        print_stats("SessionHeap burst (total ns)", session_stats);
        std::cout << "    Per-msg: " << std::fixed << std::setprecision(1)
                  << session_stats.p50_ns / static_cast<double>(BURST_COUNT) << " ns\n";
    }

    // mimalloc-only
    {
        std::vector<uint64_t> cycles;
        cycles.reserve(SESSION_BURST_ITERATIONS);

        for (int iter = 0; iter < SESSION_BURST_ITERATIONS; ++iter) {
            MimallocMemoryResource resource;

            uint64_t start = rdtsc_vm_safe();
            for (size_t i = 0; i < BURST_COUNT; ++i) {
                void* p = resource.allocate(MESSAGE_SIZE, alignof(std::max_align_t));
                std::memcpy(p, SAMPLE_MESSAGE, sizeof(SAMPLE_MESSAGE) - 1);
                asm volatile("" :: "r"(p));
            }
            uint64_t end = rdtsc_vm_safe();
            cycles.push_back(end - start);
        }

        LatencyStats mi_stats;
        mi_stats.compute(cycles, freq_ghz);
        print_stats("mimalloc-only burst (total ns)", mi_stats);
        std::cout << "    Per-msg: " << std::fixed << std::setprecision(1)
                  << mi_stats.p50_ns / static_cast<double>(BURST_COUNT) << " ns\n";
    }

    // 6b: Overflow test - allocate beyond initial buffer, measure fallback
    std::cout << "\n  6b: Overflow latency (small buffer, forces mimalloc fallback)\n\n";

    {
        constexpr size_t SMALL_BUFFER = 4096;  // 4KB - will overflow quickly
        constexpr size_t OVERFLOW_ALLOCS = 100;
        constexpr int OVERFLOW_ITERATIONS = 1000;

        std::vector<uint64_t> cycles;
        cycles.reserve(OVERFLOW_ITERATIONS);

        for (int iter = 0; iter < OVERFLOW_ITERATIONS; ++iter) {
            SessionHeap session(SMALL_BUFFER);

            uint64_t start = rdtsc_vm_safe();
            for (size_t i = 0; i < OVERFLOW_ALLOCS; ++i) {
                void* p = session.allocate(MESSAGE_SIZE, alignof(std::max_align_t));
                std::memcpy(p, SAMPLE_MESSAGE, sizeof(SAMPLE_MESSAGE) - 1);
                asm volatile("" :: "r"(p));
            }
            uint64_t end = rdtsc_vm_safe();
            cycles.push_back(end - start);
        }

        LatencyStats overflow_stats;
        overflow_stats.compute(cycles, freq_ghz);
        print_stats("SessionHeap overflow burst (total ns)", overflow_stats);
        std::cout << "    Per-msg: " << std::fixed << std::setprecision(1)
                  << overflow_stats.p50_ns / static_cast<double>(OVERFLOW_ALLOCS)
                  << " ns (mixed bump + mimalloc)\n";
    }

    // ========================================================================
    // Summary
    // ========================================================================

    std::cout << "\n==========================================================\n";
    std::cout << "  mimalloc Integration Benefits\n";
    std::cout << "==========================================================\n\n";

    std::cout << "  1. Per-session heap isolation (no cross-session interference)\n";
    std::cout << "  2. O(1) heap destruction via mi_heap_destroy()\n";
    std::cout << "  3. Thread-safe cross-thread deallocation\n";
    std::cout << "  4. Aligned allocation support (cache-line, SIMD)\n";
    std::cout << "  5. std::pmr::memory_resource compatible\n";
    std::cout << "  6. SessionHeap: monotonic bump + mimalloc overflow\n";

    std::cout << "\n==========================================================\n";
    std::cout << "  Implementation Reference\n";
    std::cout << "==========================================================\n\n";

    std::cout << "  File: include/nexusfix/memory/mimalloc_resource.hpp\n";
    std::cout << "  Ticket: TICKET_210b\n";

    std::cout << "\n==========================================================\n";

    return 0;
}
