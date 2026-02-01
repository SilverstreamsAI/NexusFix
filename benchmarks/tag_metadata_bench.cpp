// ============================================================================
// TICKET_023: Tag Metadata Benchmark
// Compare: Switch-based vs Compile-time Lookup Table
// ============================================================================

#include <iostream>
#include <random>
#include <array>
#include <cstdint>
#include <string_view>

// Include the new implementation
#include "nexusfix/types/tag.hpp"

// ============================================================================
// OLD Implementation (switch-based) - for comparison
// ============================================================================

namespace old_impl {

[[nodiscard]] inline std::string_view tag_name(int tag) noexcept {
    switch (tag) {
        // Header tags
        case 8:   return "BeginString";
        case 9:   return "BodyLength";
        case 35:  return "MsgType";
        case 49:  return "SenderCompID";
        case 56:  return "TargetCompID";
        case 34:  return "MsgSeqNum";
        case 52:  return "SendingTime";
        case 43:  return "PossDupFlag";
        case 97:  return "PossResend";
        case 122: return "OrigSendingTime";
        // Trailer
        case 10:  return "CheckSum";
        // Session
        case 98:  return "EncryptMethod";
        case 108: return "HeartBtInt";
        case 141: return "ResetSeqNumFlag";
        case 112: return "TestReqID";
        case 45:  return "RefSeqNum";
        case 58:  return "Text";
        // Order
        case 11:  return "ClOrdID";
        case 55:  return "Symbol";
        case 54:  return "Side";
        case 38:  return "OrderQty";
        case 40:  return "OrdType";
        case 44:  return "Price";
        case 99:  return "StopPx";
        case 59:  return "TimeInForce";
        case 60:  return "TransactTime";
        case 1:   return "Account";
        case 21:  return "HandlInst";
        // Execution
        case 37:  return "OrderID";
        case 17:  return "ExecID";
        case 150: return "ExecType";
        case 39:  return "OrdStatus";
        case 151: return "LeavesQty";
        case 14:  return "CumQty";
        case 6:   return "AvgPx";
        case 31:  return "LastPx";
        case 32:  return "LastQty";
        case 41:  return "OrigClOrdID";
        default:  return "";
    }
}

[[nodiscard]] inline bool is_header_tag(int tag) noexcept {
    switch (tag) {
        case 8:
        case 9:
        case 35:
        case 49:
        case 56:
        case 34:
        case 52:
        case 43:
        case 97:
        case 122:
            return true;
        default:
            return false;
    }
}

[[nodiscard]] inline bool is_required_tag(int tag) noexcept {
    switch (tag) {
        case 8:   // BeginString
        case 9:   // BodyLength
        case 35:  // MsgType
        case 49:  // SenderCompID
        case 56:  // TargetCompID
        case 34:  // MsgSeqNum
        case 52:  // SendingTime
        case 10:  // CheckSum
            return true;
        default:
            return false;
    }
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

template<typename T>
inline void do_not_optimize(T&& value) {
    asm volatile("" : : "r,m"(value) : "memory");
}

// ============================================================================
// Test data
// ============================================================================

// Common header tags
constexpr std::array<int, 10> HEADER_TAGS = {
    8, 9, 35, 49, 56, 34, 52, 43, 97, 122
};

// All known tags (for comprehensive test)
constexpr std::array<int, 39> ALL_KNOWN_TAGS = {
    1, 6, 8, 9, 10, 11, 14, 17, 21, 31, 32, 34, 35, 37, 38, 39, 40, 41,
    43, 44, 45, 49, 52, 54, 55, 56, 58, 59, 60, 97, 98, 99, 108, 112,
    122, 141, 150, 151
};

// Hot path: most frequently accessed tags during parsing
constexpr std::array<int, 7> HOT_TAGS = {
    8, 9, 35, 34, 49, 56, 52  // Header required tags
};

// ============================================================================
// Benchmark
// ============================================================================

int main() {
    std::cout << "============================================================\n";
    std::cout << "TICKET_023: Tag Metadata Benchmark\n";
    std::cout << "============================================================\n\n";

    constexpr int ITERATIONS = 10'000'000;
    constexpr int WARMUP = 100'000;

    // ========================================================================
    // Benchmark 1: tag_name() - All known tags
    // ========================================================================

    std::cout << "--- tag_name() (39 tags, " << ITERATIONS << " iterations) ---\n\n";

    // Warmup
    for (int i = 0; i < WARMUP; ++i) {
        for (int tag : ALL_KNOWN_TAGS) {
            do_not_optimize(old_impl::tag_name(tag));
            do_not_optimize(nfx::tag::tag_name(tag));
        }
    }

    // OLD: Switch-based
    uint64_t old_name_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (int tag : ALL_KNOWN_TAGS) {
            do_not_optimize(old_impl::tag_name(tag));
        }
    }
    uint64_t old_name_cycles = rdtsc() - old_name_start;

    // NEW: Lookup table
    uint64_t new_name_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (int tag : ALL_KNOWN_TAGS) {
            do_not_optimize(nfx::tag::tag_name(tag));
        }
    }
    uint64_t new_name_cycles = rdtsc() - new_name_start;

    double name_ops = static_cast<double>(ITERATIONS) * ALL_KNOWN_TAGS.size();
    double old_name_cpop = static_cast<double>(old_name_cycles) / name_ops;
    double new_name_cpop = static_cast<double>(new_name_cycles) / name_ops;
    double name_improvement = (old_name_cpop - new_name_cpop) / old_name_cpop * 100;

    std::cout << "  OLD (switch):     " << old_name_cpop << " cycles/op\n";
    std::cout << "  NEW (lookup):     " << new_name_cpop << " cycles/op\n";
    std::cout << "  Improvement:      " << name_improvement << "%\n\n";

    // ========================================================================
    // Benchmark 2: is_header_tag() - All known tags
    // ========================================================================

    std::cout << "--- is_header_tag() (39 tags, " << ITERATIONS << " iterations) ---\n\n";

    // OLD: Switch-based
    uint64_t old_header_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (int tag : ALL_KNOWN_TAGS) {
            do_not_optimize(old_impl::is_header_tag(tag));
        }
    }
    uint64_t old_header_cycles = rdtsc() - old_header_start;

    // NEW: Lookup table
    uint64_t new_header_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (int tag : ALL_KNOWN_TAGS) {
            do_not_optimize(nfx::tag::is_header_tag(tag));
        }
    }
    uint64_t new_header_cycles = rdtsc() - new_header_start;

    double header_ops = static_cast<double>(ITERATIONS) * ALL_KNOWN_TAGS.size();
    double old_header_cpop = static_cast<double>(old_header_cycles) / header_ops;
    double new_header_cpop = static_cast<double>(new_header_cycles) / header_ops;
    double header_improvement = (old_header_cpop - new_header_cpop) / old_header_cpop * 100;

    std::cout << "  OLD (switch):     " << old_header_cpop << " cycles/op\n";
    std::cout << "  NEW (lookup):     " << new_header_cpop << " cycles/op\n";
    std::cout << "  Improvement:      " << header_improvement << "%\n\n";

    // ========================================================================
    // Benchmark 3: is_required_tag() - All known tags
    // ========================================================================

    std::cout << "--- is_required_tag() (39 tags, " << ITERATIONS << " iterations) ---\n\n";

    // OLD: Switch-based
    uint64_t old_req_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (int tag : ALL_KNOWN_TAGS) {
            do_not_optimize(old_impl::is_required_tag(tag));
        }
    }
    uint64_t old_req_cycles = rdtsc() - old_req_start;

    // NEW: Lookup table
    uint64_t new_req_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (int tag : ALL_KNOWN_TAGS) {
            do_not_optimize(nfx::tag::is_required_tag(tag));
        }
    }
    uint64_t new_req_cycles = rdtsc() - new_req_start;

    double req_ops = static_cast<double>(ITERATIONS) * ALL_KNOWN_TAGS.size();
    double old_req_cpop = static_cast<double>(old_req_cycles) / req_ops;
    double new_req_cpop = static_cast<double>(new_req_cycles) / req_ops;
    double req_improvement = (old_req_cpop - new_req_cpop) / old_req_cpop * 100;

    std::cout << "  OLD (switch):     " << old_req_cpop << " cycles/op\n";
    std::cout << "  NEW (lookup):     " << new_req_cpop << " cycles/op\n";
    std::cout << "  Improvement:      " << req_improvement << "%\n\n";

    // ========================================================================
    // Benchmark 4: Hot path - header tags only
    // ========================================================================

    std::cout << "--- Hot Path (7 header tags, " << ITERATIONS << " iterations) ---\n\n";

    // OLD: Hot path
    uint64_t old_hot_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (int tag : HOT_TAGS) {
            do_not_optimize(old_impl::tag_name(tag));
        }
    }
    uint64_t old_hot_cycles = rdtsc() - old_hot_start;

    // NEW: Hot path
    uint64_t new_hot_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (int tag : HOT_TAGS) {
            do_not_optimize(nfx::tag::tag_name(tag));
        }
    }
    uint64_t new_hot_cycles = rdtsc() - new_hot_start;

    double hot_ops = static_cast<double>(ITERATIONS) * HOT_TAGS.size();
    double old_hot_cpop = static_cast<double>(old_hot_cycles) / hot_ops;
    double new_hot_cpop = static_cast<double>(new_hot_cycles) / hot_ops;
    double hot_improvement = (old_hot_cpop - new_hot_cpop) / old_hot_cpop * 100;

    std::cout << "  OLD (switch):     " << old_hot_cpop << " cycles/op\n";
    std::cout << "  NEW (lookup):     " << new_hot_cpop << " cycles/op\n";
    std::cout << "  Improvement:      " << hot_improvement << "%\n\n";

    // ========================================================================
    // Benchmark 5: Random access pattern
    // ========================================================================

    std::cout << "--- Random Access Pattern (" << ITERATIONS << " iterations) ---\n\n";

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, ALL_KNOWN_TAGS.size() - 1);
    std::array<int, 1024> random_tags;
    for (auto& t : random_tags) {
        t = ALL_KNOWN_TAGS[dist(rng)];
    }

    // OLD: Random access
    uint64_t old_rand_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (int tag : random_tags) {
            do_not_optimize(old_impl::tag_name(tag));
        }
    }
    uint64_t old_rand_cycles = rdtsc() - old_rand_start;

    // NEW: Random access
    uint64_t new_rand_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (int tag : random_tags) {
            do_not_optimize(nfx::tag::tag_name(tag));
        }
    }
    uint64_t new_rand_cycles = rdtsc() - new_rand_start;

    double rand_ops = static_cast<double>(ITERATIONS) * random_tags.size();
    double old_rand_cpop = static_cast<double>(old_rand_cycles) / rand_ops;
    double new_rand_cpop = static_cast<double>(new_rand_cycles) / rand_ops;
    double rand_improvement = (old_rand_cpop - new_rand_cpop) / old_rand_cpop * 100;

    std::cout << "  OLD (switch):     " << old_rand_cpop << " cycles/op\n";
    std::cout << "  NEW (lookup):     " << new_rand_cpop << " cycles/op\n";
    std::cout << "  Improvement:      " << rand_improvement << "%\n\n";

    // ========================================================================
    // Summary
    // ========================================================================

    double avg_improvement = (name_improvement + header_improvement + req_improvement +
                              hot_improvement + rand_improvement) / 5.0;

    std::cout << "============================================================\n";
    std::cout << "SUMMARY\n";
    std::cout << "============================================================\n\n";

    std::cout << "| Function           | OLD (cycles) | NEW (cycles) | Improvement |\n";
    std::cout << "|--------------------|--------------|--------------|-------------|\n";
    std::cout << "| tag_name()         | " << old_name_cpop << "       | " << new_name_cpop << "       | " << name_improvement << "% |\n";
    std::cout << "| is_header_tag()    | " << old_header_cpop << "       | " << new_header_cpop << "       | " << header_improvement << "% |\n";
    std::cout << "| is_required_tag()  | " << old_req_cpop << "       | " << new_req_cpop << "       | " << req_improvement << "% |\n";
    std::cout << "| Hot path           | " << old_hot_cpop << "       | " << new_hot_cpop << "       | " << hot_improvement << "% |\n";
    std::cout << "| Random access      | " << old_rand_cpop << "       | " << new_rand_cpop << "       | " << rand_improvement << "% |\n";
    std::cout << "|--------------------|--------------|--------------|-------------|\n";
    std::cout << "| Average            |              |              | " << avg_improvement << "% |\n";

    std::cout << "\nSwitch cases eliminated: 39 (tag_name) + 10 (is_header) + 8 (is_required) = 57\n";
    std::cout << "Sparse lookup table size: 200 entries x 24 bytes = 4.8KB\n";

    return 0;
}
