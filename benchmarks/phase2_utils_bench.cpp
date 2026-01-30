/*
    Phase 2 Utilities Benchmark (Realistic Scenarios)

    Tests real-world performance of new utilities.
*/

#include <nexusfix/util/bit_utils.hpp>
#include <nexusfix/util/branchless.hpp>
#include <nexusfix/util/string_hash.hpp>
#include <nexusfix/memory/seqlock.hpp>
#include <nexusfix/memory/object_pool.hpp>
#include <nexusfix/util/rdtsc_timestamp.hpp>

#include <iostream>
#include <iomanip>
#include <vector>
#include <random>
#include <cstring>
#include <chrono>
#include <mutex>
#include <new>
#include <queue>

using namespace nfx::util;
using namespace nfx::util::literals;
using namespace nfx::memory;

// ============================================================================
// Benchmark Infrastructure
// ============================================================================

struct BenchResult {
    const char* name;
    double ns_per_op;
    double ops_per_sec;
};

template<typename Func>
BenchResult run_bench(const char* name, size_t iterations, Func&& func) {
    // Warmup
    for (size_t i = 0; i < 100; ++i) {
        func();
    }

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < iterations; ++i) {
        func();
    }
    auto end = std::chrono::high_resolution_clock::now();

    double elapsed_ns = std::chrono::duration<double, std::nano>(end - start).count();
    double ns_per_op = elapsed_ns / iterations;
    double ops_per_sec = 1e9 / ns_per_op;

    return {name, ns_per_op, ops_per_sec};
}

void print_result(const BenchResult& r) {
    std::cout << std::left << std::setw(45) << r.name
              << std::right << std::setw(12) << std::fixed << std::setprecision(2) << r.ns_per_op << " ns"
              << std::setw(12) << std::setprecision(2) << r.ops_per_sec / 1e6 << " M/s\n";
}

void print_comparison(const BenchResult& before, const BenchResult& after) {
    double speedup = before.ns_per_op / after.ns_per_op;
    const char* verdict = speedup > 1.0 ? "FASTER" : (speedup < 1.0 ? "slower" : "same");
    std::cout << "  => " << std::fixed << std::setprecision(2) << speedup << "x " << verdict << "\n\n";
}

volatile int sink = 0;

// ============================================================================
// Test 1: Branchless Min/Max (Random Data - Branch Misprediction)
// ============================================================================

void bench_branchless() {
    std::cout << "=== 1. Branchless Min/Max (Random Data) ===\n";
    std::cout << "   Scenario: 10,000 random comparisons (causes branch misprediction)\n\n";

    constexpr size_t N = 10000;
    constexpr size_t ITERS = 1000;
    std::vector<int> data(N);
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(-1000000, 1000000);
    for (auto& x : data) x = dist(rng);

    auto r1 = run_bench("std::min (conditional)", ITERS, [&]() {
        int sum = 0;
        for (size_t i = 0; i < N - 1; ++i) {
            sum += std::min(data[i], data[i+1]);
        }
        sink = sum;
    });
    print_result(r1);

    auto r2 = run_bench("branchless_min", ITERS, [&]() {
        int sum = 0;
        for (size_t i = 0; i < N - 1; ++i) {
            sum += branchless_min(data[i], data[i+1]);
        }
        sink = sum;
    });
    print_result(r2);
    print_comparison(r1, r2);
}

// ============================================================================
// Test 2: String Hash (Longer strings, many types)
// ============================================================================

void bench_string_hash() {
    std::cout << "=== 2. Message Type Dispatch (FIX Version String) ===\n";
    std::cout << "   Scenario: Dispatch based on FIX version (8-char strings)\n\n";

    constexpr size_t N = 100000;

    // Longer strings like FIX versions
    const char* versions[] = {"FIX.4.0", "FIX.4.1", "FIX.4.2", "FIX.4.3", "FIX.4.4", "FIX.5.0", "FIXT1.1"};
    std::vector<std::string_view> test_data;
    test_data.reserve(N);
    std::mt19937 rng(42);
    std::uniform_int_distribution<size_t> dist(0, 6);
    for (size_t i = 0; i < N; ++i) {
        test_data.push_back(versions[dist(rng)]);
    }

    auto r1 = run_bench("strcmp chain (7 versions)", N, [&]() {
        int count = 0;
        for (const auto& v : test_data) {
            if (v == "FIX.4.0") count += 1;
            else if (v == "FIX.4.1") count += 2;
            else if (v == "FIX.4.2") count += 3;
            else if (v == "FIX.4.3") count += 4;
            else if (v == "FIX.4.4") count += 5;
            else if (v == "FIX.5.0") count += 6;
            else if (v == "FIXT1.1") count += 7;
        }
        sink = count;
    });
    print_result(r1);

    auto r2 = run_bench("hash switch (7 versions)", N, [&]() {
        int count = 0;
        for (const auto& v : test_data) {
            switch (fnv1a_hash_runtime(v)) {
                case "FIX.4.0"_hash: count += 1; break;
                case "FIX.4.1"_hash: count += 2; break;
                case "FIX.4.2"_hash: count += 3; break;
                case "FIX.4.3"_hash: count += 4; break;
                case "FIX.4.4"_hash: count += 5; break;
                case "FIX.5.0"_hash: count += 6; break;
                case "FIXT1.1"_hash: count += 7; break;
            }
        }
        sink = count;
    });
    print_result(r2);
    print_comparison(r1, r2);
}

// ============================================================================
// Test 3: Seqlock vs Mutex
// ============================================================================

struct MarketData {
    double bid;
    double ask;
    int64_t volume;
    int64_t timestamp;
};

void bench_seqlock() {
    std::cout << "=== 3. Seqlock vs Mutex (Read-Heavy) ===\n";
    std::cout << "   Scenario: Single-threaded read of shared market data\n\n";

    constexpr size_t N = 1000000;

    std::mutex mtx;
    MarketData md_mutex{100.5, 100.6, 1000, 12345};

    auto r1 = run_bench("mutex lock/read/unlock", N, [&]() {
        std::lock_guard<std::mutex> lock(mtx);
        MarketData copy = md_mutex;
        sink = static_cast<int>(copy.bid);
    });
    print_result(r1);

    Seqlock<MarketData> seqlock{{100.5, 100.6, 1000, 12345}};

    auto r2 = run_bench("seqlock read", N, [&]() {
        MarketData copy = seqlock.read();
        sink = static_cast<int>(copy.bid);
    });
    print_result(r2);
    print_comparison(r1, r2);
}

// ============================================================================
// Test 4: Object Pool (Batch Allocation Pattern)
// ============================================================================

struct Order {
    char symbol[16];
    double price;
    int64_t quantity;
    int64_t order_id;
    char side;
    char padding[7];
};

void bench_object_pool() {
    std::cout << "=== 4. Object Pool vs new/delete (Batch Pattern) ===\n";
    std::cout << "   Scenario: Allocate 100 objects, then deallocate all\n\n";

    constexpr size_t BATCH = 100;
    constexpr size_t ITERS = 10000;

    // new/delete batch
    auto r1 = run_bench("new/delete (100 objects)", ITERS, [&]() {
        std::vector<Order*> orders;
        orders.reserve(BATCH);
        for (size_t i = 0; i < BATCH; ++i) {
            orders.push_back(new Order{});
        }
        for (auto* o : orders) {
            delete o;
        }
    });
    print_result(r1);

    // Object pool batch
    ObjectPool<Order, 256> pool;

    auto r2 = run_bench("ObjectPool (100 objects)", ITERS, [&]() {
        std::vector<Order*> orders;
        orders.reserve(BATCH);
        for (size_t i = 0; i < BATCH; ++i) {
            orders.push_back(pool.allocate());
        }
        for (auto* o : orders) {
            pool.deallocate(o);
        }
    });
    print_result(r2);
    print_comparison(r1, r2);
}

// ============================================================================
// Test 5: Object Pool (High Frequency Pattern)
// ============================================================================

void bench_object_pool_high_freq() {
    std::cout << "=== 5. Object Pool (High Frequency Alloc/Free) ===\n";
    std::cout << "   Scenario: Rapid alloc/free with queue (message processing)\n\n";

    constexpr size_t ITERS = 100000;
    constexpr size_t QUEUE_SIZE = 10;

    // new/delete with queue
    std::queue<Order*> q1;

    auto r1 = run_bench("new/delete queue pattern", ITERS, [&]() {
        // Add to queue
        q1.push(new Order{});
        if (q1.size() > QUEUE_SIZE) {
            delete q1.front();
            q1.pop();
        }
    });
    // Cleanup
    while (!q1.empty()) { delete q1.front(); q1.pop(); }
    print_result(r1);

    // Object pool with queue
    ObjectPool<Order, 256> pool;
    std::queue<Order*> q2;

    auto r2 = run_bench("ObjectPool queue pattern", ITERS, [&]() {
        q2.push(pool.allocate());
        if (q2.size() > QUEUE_SIZE) {
            pool.deallocate(q2.front());
            q2.pop();
        }
    });
    // Cleanup
    while (!q2.empty()) { pool.deallocate(q2.front()); q2.pop(); }
    print_result(r2);
    print_comparison(r1, r2);
}

// ============================================================================
// Test 6: Branchless Range Check
// ============================================================================

void bench_range_check() {
    std::cout << "=== 6. Branchless Range Check ===\n";
    std::cout << "   Scenario: Check if char is digit (FIX field parsing)\n\n";

    constexpr size_t N = 100000;

    // Generate mixed chars
    std::string test_str;
    for (size_t i = 0; i < 10000; ++i) {
        test_str += "123abc456DEF789xyz0";
    }
    const char* data = test_str.c_str();
    size_t len = test_str.size();

    auto r1 = run_bench("traditional: c >= '0' && c <= '9'", N, [&]() {
        int count = 0;
        for (size_t i = 0; i < len; ++i) {
            char c = data[i];
            if (c >= '0' && c <= '9') {
                count++;
            }
        }
        sink = count;
    });
    print_result(r1);

    auto r2 = run_bench("branchless: is_digit()", N, [&]() {
        int count = 0;
        for (size_t i = 0; i < len; ++i) {
            count += is_digit(data[i]);
        }
        sink = count;
    });
    print_result(r2);
    print_comparison(r1, r2);
}

// ============================================================================
// Test 7: Byte Swap
// ============================================================================

void bench_byteswap() {
    std::cout << "=== 7. Byte Swap (Network to Host) ===\n";
    std::cout << "   Scenario: Convert big-endian to little-endian\n\n";

    constexpr size_t N = 1000000;
    uint32_t network_data = 0x12345678;

    // __builtin_bswap32
    auto r1 = run_bench("__builtin_bswap32", N, [&]() {
        sink = static_cast<int>(__builtin_bswap32(network_data));
    });
    print_result(r1);

    // Our byteswap32
    auto r2 = run_bench("byteswap32 (constexpr)", N, [&]() {
        sink = static_cast<int>(byteswap32(network_data));
    });
    print_result(r2);
    print_comparison(r1, r2);
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "============================================================\n";
    std::cout << "         NexusFIX Phase 2 Utilities Benchmark\n";
    std::cout << "============================================================\n\n";

    bench_branchless();
    bench_string_hash();
    bench_seqlock();
    bench_object_pool();
    bench_object_pool_high_freq();
    bench_range_check();
    bench_byteswap();

    std::cout << "============================================================\n";
    std::cout << "                       Summary\n";
    std::cout << "============================================================\n";
    std::cout << "FASTER = Phase 2 utility is faster than traditional approach\n";
    std::cout << "slower = Traditional approach is faster (may reconsider use)\n";

    return 0;
}
