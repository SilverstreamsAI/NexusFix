// ============================================================================
// TICKET_022: MsgType Dispatch Benchmark
// Compare: Switch-based vs Compile-time Lookup Table
// ============================================================================

#include <iostream>
#include <chrono>
#include <random>
#include <array>
#include <cstdint>
#include <string_view>

// Include the new implementation
#include "nexusfix/interfaces/i_message.hpp"

// ============================================================================
// OLD Implementation (switch-based) - for comparison
// ============================================================================

namespace old_impl {

inline constexpr char Heartbeat        = '0';
inline constexpr char TestRequest      = '1';
inline constexpr char ResendRequest    = '2';
inline constexpr char Reject           = '3';
inline constexpr char SequenceReset    = '4';
inline constexpr char Logout           = '5';
inline constexpr char Logon            = 'A';
inline constexpr char NewOrderSingle   = 'D';
inline constexpr char OrderCancelRequest = 'F';
inline constexpr char OrderCancelReplaceRequest = 'G';
inline constexpr char OrderStatusRequest = 'H';
inline constexpr char ExecutionReport  = '8';
inline constexpr char OrderCancelReject = '9';
inline constexpr char MarketDataRequest = 'V';
inline constexpr char MarketDataSnapshotFullRefresh = 'W';
inline constexpr char MarketDataIncrementalRefresh = 'X';
inline constexpr char MarketDataRequestReject = 'Y';

[[nodiscard]] inline std::string_view name(char type) noexcept {
    switch (type) {
        case Heartbeat:        return "Heartbeat";
        case TestRequest:      return "TestRequest";
        case ResendRequest:    return "ResendRequest";
        case Reject:           return "Reject";
        case SequenceReset:    return "SequenceReset";
        case Logout:           return "Logout";
        case Logon:            return "Logon";
        case NewOrderSingle:   return "NewOrderSingle";
        case OrderCancelRequest: return "OrderCancelRequest";
        case OrderCancelReplaceRequest: return "OrderCancelReplaceRequest";
        case OrderStatusRequest: return "OrderStatusRequest";
        case ExecutionReport:  return "ExecutionReport";
        case OrderCancelReject: return "OrderCancelReject";
        case MarketDataRequest: return "MarketDataRequest";
        case MarketDataSnapshotFullRefresh: return "MarketDataSnapshotFullRefresh";
        case MarketDataIncrementalRefresh: return "MarketDataIncrementalRefresh";
        case MarketDataRequestReject: return "MarketDataRequestReject";
        default:               return "Unknown";
    }
}

[[nodiscard]] inline bool is_admin(char type) noexcept {
    return type == Heartbeat ||
           type == TestRequest ||
           type == ResendRequest ||
           type == Reject ||
           type == SequenceReset ||
           type == Logout ||
           type == Logon;
}

} // namespace old_impl

// ============================================================================
// Benchmark utilities
// ============================================================================

inline uint64_t rdtsc() {
    uint64_t lo, hi;
    asm volatile ("lfence; rdtsc; lfence" : "=a"(lo), "=d"(hi));
    return (hi << 32) | lo;
}

// Prevent compiler from optimizing away the result
template<typename T>
inline void do_not_optimize(T&& value) {
    asm volatile("" : : "r,m"(value) : "memory");
}

// ============================================================================
// Benchmark
// ============================================================================

int main() {
    std::cout << "============================================================\n";
    std::cout << "TICKET_022: MsgType Dispatch Benchmark\n";
    std::cout << "============================================================\n\n";

    // Test data: common message types in realistic distribution
    constexpr std::array<char, 17> all_types = {
        '0', '1', '2', '3', '4', '5', 'A',  // Admin
        '8', '9', 'D', 'F', 'G', 'H',       // Order
        'V', 'W', 'X', 'Y'                  // Market Data
    };

    // Hot path types (most common in trading)
    constexpr std::array<char, 5> hot_types = {
        '8',  // ExecutionReport (most common)
        'D',  // NewOrderSingle
        '0',  // Heartbeat
        'A',  // Logon
        'W'   // MarketDataSnapshot
    };

    constexpr int ITERATIONS = 10'000'000;
    constexpr int WARMUP = 100'000;

    // ========================================================================
    // Benchmark 1: name() - All types
    // ========================================================================

    std::cout << "--- name() Benchmark (All 17 types, " << ITERATIONS << " iterations) ---\n\n";

    // Warmup
    for (int i = 0; i < WARMUP; ++i) {
        for (char t : all_types) {
            do_not_optimize(old_impl::name(t));
            do_not_optimize(nfx::msg_type::name(t));
        }
    }

    // OLD: Switch-based
    uint64_t old_name_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (char t : all_types) {
            do_not_optimize(old_impl::name(t));
        }
    }
    uint64_t old_name_cycles = rdtsc() - old_name_start;

    // NEW: Lookup table
    uint64_t new_name_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (char t : all_types) {
            do_not_optimize(nfx::msg_type::name(t));
        }
    }
    uint64_t new_name_cycles = rdtsc() - new_name_start;

    double name_ops = static_cast<double>(ITERATIONS) * all_types.size();
    double old_name_cpop = static_cast<double>(old_name_cycles) / name_ops;
    double new_name_cpop = static_cast<double>(new_name_cycles) / name_ops;
    double name_improvement = (old_name_cpop - new_name_cpop) / old_name_cpop * 100;

    std::cout << "  OLD (switch):     " << old_name_cpop << " cycles/op\n";
    std::cout << "  NEW (lookup):     " << new_name_cpop << " cycles/op\n";
    std::cout << "  Improvement:      " << name_improvement << "%\n\n";

    // ========================================================================
    // Benchmark 2: is_admin() - All types
    // ========================================================================

    std::cout << "--- is_admin() Benchmark (All 17 types, " << ITERATIONS << " iterations) ---\n\n";

    // OLD: if-chain
    uint64_t old_admin_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (char t : all_types) {
            do_not_optimize(old_impl::is_admin(t));
        }
    }
    uint64_t old_admin_cycles = rdtsc() - old_admin_start;

    // NEW: Lookup table
    uint64_t new_admin_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (char t : all_types) {
            do_not_optimize(nfx::msg_type::is_admin(t));
        }
    }
    uint64_t new_admin_cycles = rdtsc() - new_admin_start;

    double admin_ops = static_cast<double>(ITERATIONS) * all_types.size();
    double old_admin_cpop = static_cast<double>(old_admin_cycles) / admin_ops;
    double new_admin_cpop = static_cast<double>(new_admin_cycles) / admin_ops;
    double admin_improvement = (old_admin_cpop - new_admin_cpop) / old_admin_cpop * 100;

    std::cout << "  OLD (if-chain):   " << old_admin_cpop << " cycles/op\n";
    std::cout << "  NEW (lookup):     " << new_admin_cpop << " cycles/op\n";
    std::cout << "  Improvement:      " << admin_improvement << "%\n\n";

    // ========================================================================
    // Benchmark 3: Hot path types only
    // ========================================================================

    std::cout << "--- Hot Path Benchmark (5 common types, " << ITERATIONS << " iterations) ---\n\n";

    // OLD: name() on hot types
    uint64_t old_hot_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (char t : hot_types) {
            do_not_optimize(old_impl::name(t));
        }
    }
    uint64_t old_hot_cycles = rdtsc() - old_hot_start;

    // NEW: name() on hot types
    uint64_t new_hot_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (char t : hot_types) {
            do_not_optimize(nfx::msg_type::name(t));
        }
    }
    uint64_t new_hot_cycles = rdtsc() - new_hot_start;

    double hot_ops = static_cast<double>(ITERATIONS) * hot_types.size();
    double old_hot_cpop = static_cast<double>(old_hot_cycles) / hot_ops;
    double new_hot_cpop = static_cast<double>(new_hot_cycles) / hot_ops;
    double hot_improvement = (old_hot_cpop - new_hot_cpop) / old_hot_cpop * 100;

    std::cout << "  OLD (switch):     " << old_hot_cpop << " cycles/op\n";
    std::cout << "  NEW (lookup):     " << new_hot_cpop << " cycles/op\n";
    std::cout << "  Improvement:      " << hot_improvement << "%\n\n";

    // ========================================================================
    // Benchmark 4: Random access pattern (cache pressure)
    // ========================================================================

    std::cout << "--- Random Access Pattern (" << ITERATIONS << " iterations) ---\n\n";

    // Generate random indices
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, all_types.size() - 1);
    std::array<char, 1024> random_types;
    for (auto& t : random_types) {
        t = all_types[dist(rng)];
    }

    // OLD: Random access
    uint64_t old_rand_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (char t : random_types) {
            do_not_optimize(old_impl::name(t));
        }
    }
    uint64_t old_rand_cycles = rdtsc() - old_rand_start;

    // NEW: Random access
    uint64_t new_rand_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (char t : random_types) {
            do_not_optimize(nfx::msg_type::name(t));
        }
    }
    uint64_t new_rand_cycles = rdtsc() - new_rand_start;

    double rand_ops = static_cast<double>(ITERATIONS) * random_types.size();
    double old_rand_cpop = static_cast<double>(old_rand_cycles) / rand_ops;
    double new_rand_cpop = static_cast<double>(new_rand_cycles) / rand_ops;
    double rand_improvement = (old_rand_cpop - new_rand_cpop) / old_rand_cpop * 100;

    std::cout << "  OLD (switch):     " << old_rand_cpop << " cycles/op\n";
    std::cout << "  NEW (lookup):     " << new_rand_cpop << " cycles/op\n";
    std::cout << "  Improvement:      " << rand_improvement << "%\n\n";

    // ========================================================================
    // Summary
    // ========================================================================

    std::cout << "============================================================\n";
    std::cout << "SUMMARY\n";
    std::cout << "============================================================\n\n";

    std::cout << "| Benchmark          | OLD (cycles) | NEW (cycles) | Improvement |\n";
    std::cout << "|--------------------|--------------|--------------|-------------|\n";
    std::cout << "| name() all types   | " << old_name_cpop << "       | " << new_name_cpop << "       | " << name_improvement << "% |\n";
    std::cout << "| is_admin() all     | " << old_admin_cpop << "       | " << new_admin_cpop << "       | " << admin_improvement << "% |\n";
    std::cout << "| Hot path (5 types) | " << old_hot_cpop << "       | " << new_hot_cpop << "       | " << hot_improvement << "% |\n";
    std::cout << "| Random access      | " << old_rand_cpop << "       | " << new_rand_cpop << "       | " << rand_improvement << "% |\n";

    return 0;
}
