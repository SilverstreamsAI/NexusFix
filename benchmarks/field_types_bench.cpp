// ============================================================================
// TICKET_023: Field Types Benchmark
// Compare: Switch-based vs Compile-time Lookup Table
// ============================================================================

#include <iostream>
#include <random>
#include <array>
#include <cstdint>
#include <string_view>

// Include the new implementation
#include "nexusfix/types/field_types.hpp"

// ============================================================================
// OLD Implementation (switch-based) - for comparison
// ============================================================================

namespace old_impl {

[[nodiscard]] inline std::string_view side_name(nfx::Side s) noexcept {
    switch (s) {
        case nfx::Side::Buy:             return "Buy";
        case nfx::Side::Sell:            return "Sell";
        case nfx::Side::BuyMinus:        return "BuyMinus";
        case nfx::Side::SellPlus:        return "SellPlus";
        case nfx::Side::SellShort:       return "SellShort";
        case nfx::Side::SellShortExempt: return "SellShortExempt";
        case nfx::Side::Undisclosed:     return "Undisclosed";
        case nfx::Side::Cross:           return "Cross";
        case nfx::Side::CrossShort:      return "CrossShort";
        default:                          return "Unknown";
    }
}

[[nodiscard]] inline std::string_view ord_type_name(nfx::OrdType t) noexcept {
    switch (t) {
        case nfx::OrdType::Market:            return "Market";
        case nfx::OrdType::Limit:             return "Limit";
        case nfx::OrdType::Stop:              return "Stop";
        case nfx::OrdType::StopLimit:         return "StopLimit";
        case nfx::OrdType::MarketOnClose:     return "MarketOnClose";
        case nfx::OrdType::WithOrWithout:     return "WithOrWithout";
        case nfx::OrdType::LimitOrBetter:     return "LimitOrBetter";
        case nfx::OrdType::LimitWithOrWithout: return "LimitWithOrWithout";
        case nfx::OrdType::OnBasis:           return "OnBasis";
        case nfx::OrdType::PreviouslyQuoted:  return "PreviouslyQuoted";
        case nfx::OrdType::PreviouslyIndicated: return "PreviouslyIndicated";
        case nfx::OrdType::Pegged:            return "Pegged";
        default:                               return "Unknown";
    }
}

[[nodiscard]] inline std::string_view ord_status_name(nfx::OrdStatus s) noexcept {
    switch (s) {
        case nfx::OrdStatus::New:              return "New";
        case nfx::OrdStatus::PartiallyFilled:  return "PartiallyFilled";
        case nfx::OrdStatus::Filled:           return "Filled";
        case nfx::OrdStatus::DoneForDay:       return "DoneForDay";
        case nfx::OrdStatus::Canceled:         return "Canceled";
        case nfx::OrdStatus::Replaced:         return "Replaced";
        case nfx::OrdStatus::PendingCancel:    return "PendingCancel";
        case nfx::OrdStatus::Stopped:          return "Stopped";
        case nfx::OrdStatus::Rejected:         return "Rejected";
        case nfx::OrdStatus::Suspended:        return "Suspended";
        case nfx::OrdStatus::PendingNew:       return "PendingNew";
        case nfx::OrdStatus::Calculated:       return "Calculated";
        case nfx::OrdStatus::Expired:          return "Expired";
        case nfx::OrdStatus::AcceptedForBidding: return "AcceptedForBidding";
        case nfx::OrdStatus::PendingReplace:   return "PendingReplace";
        default:                                return "Unknown";
    }
}

[[nodiscard]] inline std::string_view exec_type_name(nfx::ExecType e) noexcept {
    switch (e) {
        case nfx::ExecType::New:           return "New";
        case nfx::ExecType::PartialFill:   return "PartialFill";
        case nfx::ExecType::Fill:          return "Fill";
        case nfx::ExecType::DoneForDay:    return "DoneForDay";
        case nfx::ExecType::Canceled:      return "Canceled";
        case nfx::ExecType::Replaced:      return "Replaced";
        case nfx::ExecType::PendingCancel: return "PendingCancel";
        case nfx::ExecType::Stopped:       return "Stopped";
        case nfx::ExecType::Rejected:      return "Rejected";
        case nfx::ExecType::Suspended:     return "Suspended";
        case nfx::ExecType::PendingNew:    return "PendingNew";
        case nfx::ExecType::Calculated:    return "Calculated";
        case nfx::ExecType::Expired:       return "Expired";
        case nfx::ExecType::Restated:      return "Restated";
        case nfx::ExecType::PendingReplace: return "PendingReplace";
        case nfx::ExecType::Trade:         return "Trade";
        case nfx::ExecType::TradeCorrect:  return "TradeCorrect";
        case nfx::ExecType::TradeCancel:   return "TradeCancel";
        case nfx::ExecType::OrderStatus:   return "OrderStatus";
        default:                            return "Unknown";
    }
}

[[nodiscard]] inline std::string_view time_in_force_name(nfx::TimeInForce t) noexcept {
    switch (t) {
        case nfx::TimeInForce::Day:              return "Day";
        case nfx::TimeInForce::GoodTillCancel:   return "GoodTillCancel";
        case nfx::TimeInForce::AtTheOpening:     return "AtTheOpening";
        case nfx::TimeInForce::ImmediateOrCancel: return "ImmediateOrCancel";
        case nfx::TimeInForce::FillOrKill:       return "FillOrKill";
        case nfx::TimeInForce::GoodTillCrossing: return "GoodTillCrossing";
        case nfx::TimeInForce::GoodTillDate:     return "GoodTillDate";
        case nfx::TimeInForce::AtTheClose:       return "AtTheClose";
        default:                                  return "Unknown";
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

constexpr std::array<nfx::Side, 9> ALL_SIDES = {
    nfx::Side::Buy, nfx::Side::Sell, nfx::Side::BuyMinus,
    nfx::Side::SellPlus, nfx::Side::SellShort, nfx::Side::SellShortExempt,
    nfx::Side::Undisclosed, nfx::Side::Cross, nfx::Side::CrossShort
};

constexpr std::array<nfx::OrdType, 12> ALL_ORD_TYPES = {
    nfx::OrdType::Market, nfx::OrdType::Limit, nfx::OrdType::Stop,
    nfx::OrdType::StopLimit, nfx::OrdType::MarketOnClose, nfx::OrdType::WithOrWithout,
    nfx::OrdType::LimitOrBetter, nfx::OrdType::LimitWithOrWithout, nfx::OrdType::OnBasis,
    nfx::OrdType::PreviouslyQuoted, nfx::OrdType::PreviouslyIndicated, nfx::OrdType::Pegged
};

constexpr std::array<nfx::OrdStatus, 15> ALL_ORD_STATUSES = {
    nfx::OrdStatus::New, nfx::OrdStatus::PartiallyFilled, nfx::OrdStatus::Filled,
    nfx::OrdStatus::DoneForDay, nfx::OrdStatus::Canceled, nfx::OrdStatus::Replaced,
    nfx::OrdStatus::PendingCancel, nfx::OrdStatus::Stopped, nfx::OrdStatus::Rejected,
    nfx::OrdStatus::Suspended, nfx::OrdStatus::PendingNew, nfx::OrdStatus::Calculated,
    nfx::OrdStatus::Expired, nfx::OrdStatus::AcceptedForBidding, nfx::OrdStatus::PendingReplace
};

constexpr std::array<nfx::ExecType, 19> ALL_EXEC_TYPES = {
    nfx::ExecType::New, nfx::ExecType::PartialFill, nfx::ExecType::Fill,
    nfx::ExecType::DoneForDay, nfx::ExecType::Canceled, nfx::ExecType::Replaced,
    nfx::ExecType::PendingCancel, nfx::ExecType::Stopped, nfx::ExecType::Rejected,
    nfx::ExecType::Suspended, nfx::ExecType::PendingNew, nfx::ExecType::Calculated,
    nfx::ExecType::Expired, nfx::ExecType::Restated, nfx::ExecType::PendingReplace,
    nfx::ExecType::Trade, nfx::ExecType::TradeCorrect, nfx::ExecType::TradeCancel,
    nfx::ExecType::OrderStatus
};

constexpr std::array<nfx::TimeInForce, 8> ALL_TIME_IN_FORCE = {
    nfx::TimeInForce::Day, nfx::TimeInForce::GoodTillCancel, nfx::TimeInForce::AtTheOpening,
    nfx::TimeInForce::ImmediateOrCancel, nfx::TimeInForce::FillOrKill,
    nfx::TimeInForce::GoodTillCrossing, nfx::TimeInForce::GoodTillDate, nfx::TimeInForce::AtTheClose
};

// Hot path: most common execution report values
constexpr std::array<nfx::ExecType, 4> HOT_EXEC_TYPES = {
    nfx::ExecType::New, nfx::ExecType::Fill, nfx::ExecType::PartialFill, nfx::ExecType::Canceled
};

// ============================================================================
// Benchmark
// ============================================================================

int main() {
    std::cout << "============================================================\n";
    std::cout << "TICKET_023: Field Types Benchmark\n";
    std::cout << "============================================================\n\n";

    constexpr int ITERATIONS = 10'000'000;
    constexpr int WARMUP = 100'000;

    // ========================================================================
    // Benchmark 1: side_name() - 9 cases
    // ========================================================================

    std::cout << "--- side_name() (9 values, " << ITERATIONS << " iterations) ---\n\n";

    for (int i = 0; i < WARMUP; ++i) {
        for (auto s : ALL_SIDES) {
            do_not_optimize(old_impl::side_name(s));
            do_not_optimize(nfx::side_name(s));
        }
    }

    uint64_t old_side_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto s : ALL_SIDES) {
            do_not_optimize(old_impl::side_name(s));
        }
    }
    uint64_t old_side_cycles = rdtsc() - old_side_start;

    uint64_t new_side_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto s : ALL_SIDES) {
            do_not_optimize(nfx::side_name(s));
        }
    }
    uint64_t new_side_cycles = rdtsc() - new_side_start;

    double side_ops = static_cast<double>(ITERATIONS) * ALL_SIDES.size();
    double old_side_cpop = static_cast<double>(old_side_cycles) / side_ops;
    double new_side_cpop = static_cast<double>(new_side_cycles) / side_ops;
    double side_improvement = (old_side_cpop - new_side_cpop) / old_side_cpop * 100;

    std::cout << "  OLD (switch):     " << old_side_cpop << " cycles/op\n";
    std::cout << "  NEW (lookup):     " << new_side_cpop << " cycles/op\n";
    std::cout << "  Improvement:      " << side_improvement << "%\n\n";

    // ========================================================================
    // Benchmark 2: ord_type_name() - 12 cases
    // ========================================================================

    std::cout << "--- ord_type_name() (12 values, " << ITERATIONS << " iterations) ---\n\n";

    uint64_t old_ot_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto t : ALL_ORD_TYPES) {
            do_not_optimize(old_impl::ord_type_name(t));
        }
    }
    uint64_t old_ot_cycles = rdtsc() - old_ot_start;

    uint64_t new_ot_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto t : ALL_ORD_TYPES) {
            do_not_optimize(nfx::ord_type_name(t));
        }
    }
    uint64_t new_ot_cycles = rdtsc() - new_ot_start;

    double ot_ops = static_cast<double>(ITERATIONS) * ALL_ORD_TYPES.size();
    double old_ot_cpop = static_cast<double>(old_ot_cycles) / ot_ops;
    double new_ot_cpop = static_cast<double>(new_ot_cycles) / ot_ops;
    double ot_improvement = (old_ot_cpop - new_ot_cpop) / old_ot_cpop * 100;

    std::cout << "  OLD (switch):     " << old_ot_cpop << " cycles/op\n";
    std::cout << "  NEW (lookup):     " << new_ot_cpop << " cycles/op\n";
    std::cout << "  Improvement:      " << ot_improvement << "%\n\n";

    // ========================================================================
    // Benchmark 3: ord_status_name() - 15 cases
    // ========================================================================

    std::cout << "--- ord_status_name() (15 values, " << ITERATIONS << " iterations) ---\n\n";

    uint64_t old_os_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto s : ALL_ORD_STATUSES) {
            do_not_optimize(old_impl::ord_status_name(s));
        }
    }
    uint64_t old_os_cycles = rdtsc() - old_os_start;

    uint64_t new_os_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto s : ALL_ORD_STATUSES) {
            do_not_optimize(nfx::ord_status_name(s));
        }
    }
    uint64_t new_os_cycles = rdtsc() - new_os_start;

    double os_ops = static_cast<double>(ITERATIONS) * ALL_ORD_STATUSES.size();
    double old_os_cpop = static_cast<double>(old_os_cycles) / os_ops;
    double new_os_cpop = static_cast<double>(new_os_cycles) / os_ops;
    double os_improvement = (old_os_cpop - new_os_cpop) / old_os_cpop * 100;

    std::cout << "  OLD (switch):     " << old_os_cpop << " cycles/op\n";
    std::cout << "  NEW (lookup):     " << new_os_cpop << " cycles/op\n";
    std::cout << "  Improvement:      " << os_improvement << "%\n\n";

    // ========================================================================
    // Benchmark 4: exec_type_name() - 19 cases (largest)
    // ========================================================================

    std::cout << "--- exec_type_name() (19 values, " << ITERATIONS << " iterations) ---\n\n";

    uint64_t old_et_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto e : ALL_EXEC_TYPES) {
            do_not_optimize(old_impl::exec_type_name(e));
        }
    }
    uint64_t old_et_cycles = rdtsc() - old_et_start;

    uint64_t new_et_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto e : ALL_EXEC_TYPES) {
            do_not_optimize(nfx::exec_type_name(e));
        }
    }
    uint64_t new_et_cycles = rdtsc() - new_et_start;

    double et_ops = static_cast<double>(ITERATIONS) * ALL_EXEC_TYPES.size();
    double old_et_cpop = static_cast<double>(old_et_cycles) / et_ops;
    double new_et_cpop = static_cast<double>(new_et_cycles) / et_ops;
    double et_improvement = (old_et_cpop - new_et_cpop) / old_et_cpop * 100;

    std::cout << "  OLD (switch):     " << old_et_cpop << " cycles/op\n";
    std::cout << "  NEW (lookup):     " << new_et_cpop << " cycles/op\n";
    std::cout << "  Improvement:      " << et_improvement << "%\n\n";

    // ========================================================================
    // Benchmark 5: time_in_force_name() - 8 cases
    // ========================================================================

    std::cout << "--- time_in_force_name() (8 values, " << ITERATIONS << " iterations) ---\n\n";

    uint64_t old_tif_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto t : ALL_TIME_IN_FORCE) {
            do_not_optimize(old_impl::time_in_force_name(t));
        }
    }
    uint64_t old_tif_cycles = rdtsc() - old_tif_start;

    uint64_t new_tif_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto t : ALL_TIME_IN_FORCE) {
            do_not_optimize(nfx::time_in_force_name(t));
        }
    }
    uint64_t new_tif_cycles = rdtsc() - new_tif_start;

    double tif_ops = static_cast<double>(ITERATIONS) * ALL_TIME_IN_FORCE.size();
    double old_tif_cpop = static_cast<double>(old_tif_cycles) / tif_ops;
    double new_tif_cpop = static_cast<double>(new_tif_cycles) / tif_ops;
    double tif_improvement = (old_tif_cpop - new_tif_cpop) / old_tif_cpop * 100;

    std::cout << "  OLD (switch):     " << old_tif_cpop << " cycles/op\n";
    std::cout << "  NEW (lookup):     " << new_tif_cpop << " cycles/op\n";
    std::cout << "  Improvement:      " << tif_improvement << "%\n\n";

    // ========================================================================
    // Benchmark 6: Random access pattern (ExecType)
    // ========================================================================

    std::cout << "--- Random Access Pattern (ExecType, " << ITERATIONS << " iterations) ---\n\n";

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, ALL_EXEC_TYPES.size() - 1);
    std::array<nfx::ExecType, 1024> random_exec;
    for (auto& e : random_exec) {
        e = ALL_EXEC_TYPES[dist(rng)];
    }

    uint64_t old_rand_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto e : random_exec) {
            do_not_optimize(old_impl::exec_type_name(e));
        }
    }
    uint64_t old_rand_cycles = rdtsc() - old_rand_start;

    uint64_t new_rand_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto e : random_exec) {
            do_not_optimize(nfx::exec_type_name(e));
        }
    }
    uint64_t new_rand_cycles = rdtsc() - new_rand_start;

    double rand_ops = static_cast<double>(ITERATIONS) * random_exec.size();
    double old_rand_cpop = static_cast<double>(old_rand_cycles) / rand_ops;
    double new_rand_cpop = static_cast<double>(new_rand_cycles) / rand_ops;
    double rand_improvement = (old_rand_cpop - new_rand_cpop) / old_rand_cpop * 100;

    std::cout << "  OLD (switch):     " << old_rand_cpop << " cycles/op\n";
    std::cout << "  NEW (lookup):     " << new_rand_cpop << " cycles/op\n";
    std::cout << "  Improvement:      " << rand_improvement << "%\n\n";

    // ========================================================================
    // Summary
    // ========================================================================

    double avg_improvement = (side_improvement + ot_improvement + os_improvement +
                              et_improvement + tif_improvement + rand_improvement) / 6.0;

    std::cout << "============================================================\n";
    std::cout << "SUMMARY\n";
    std::cout << "============================================================\n\n";

    std::cout << "| Function             | Cases | OLD (cycles) | NEW (cycles) | Improvement |\n";
    std::cout << "|----------------------|-------|--------------|--------------|-------------|\n";
    std::cout << "| side_name()          | 9     | " << old_side_cpop << "       | " << new_side_cpop << "       | " << side_improvement << "% |\n";
    std::cout << "| ord_type_name()      | 12    | " << old_ot_cpop << "       | " << new_ot_cpop << "       | " << ot_improvement << "% |\n";
    std::cout << "| ord_status_name()    | 15    | " << old_os_cpop << "       | " << new_os_cpop << "       | " << os_improvement << "% |\n";
    std::cout << "| exec_type_name()     | 19    | " << old_et_cpop << "       | " << new_et_cpop << "       | " << et_improvement << "% |\n";
    std::cout << "| time_in_force_name() | 8     | " << old_tif_cpop << "       | " << new_tif_cpop << "       | " << tif_improvement << "% |\n";
    std::cout << "| Random access        | -     | " << old_rand_cpop << "       | " << new_rand_cpop << "       | " << rand_improvement << "% |\n";
    std::cout << "|----------------------|-------|--------------|--------------|-------------|\n";
    std::cout << "| Average              |       |              |              | " << avg_improvement << "% |\n";

    std::cout << "\nTotal enum values with to_string: 9 + 12 + 15 + 19 + 8 = 63\n";

    return 0;
}
