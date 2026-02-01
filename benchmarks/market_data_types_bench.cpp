// TICKET_023: Market Data Types Compile-time Optimization Benchmark
// Compares switch-based vs lookup table-based enum-to-string conversion

#include <array>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <random>

#include "nexusfix/types/market_data_types.hpp"

// ============================================================================
// RDTSC timing
// ============================================================================

inline uint64_t rdtsc() {
    uint64_t lo, hi;
    asm volatile (
        "lfence\n\t"
        "rdtsc\n\t"
        "lfence\n\t"
        : "=a"(lo), "=d"(hi)
    );
    return (hi << 32) | lo;
}

// ============================================================================
// OLD implementations (switch-based) for comparison
// ============================================================================

namespace old {

[[nodiscard]] constexpr std::string_view md_entry_type_name(nfx::MDEntryType t) noexcept {
    switch (t) {
        case nfx::MDEntryType::Bid: return "Bid";
        case nfx::MDEntryType::Offer: return "Offer";
        case nfx::MDEntryType::Trade: return "Trade";
        case nfx::MDEntryType::IndexValue: return "IndexValue";
        case nfx::MDEntryType::OpeningPrice: return "OpeningPrice";
        case nfx::MDEntryType::ClosingPrice: return "ClosingPrice";
        case nfx::MDEntryType::SettlementPrice: return "SettlementPrice";
        case nfx::MDEntryType::TradingSessionHighPrice: return "SessionHigh";
        case nfx::MDEntryType::TradingSessionLowPrice: return "SessionLow";
        case nfx::MDEntryType::TradingSessionVWAPPrice: return "VWAP";
        case nfx::MDEntryType::Imbalance: return "Imbalance";
        case nfx::MDEntryType::TradeVolume: return "TradeVolume";
        case nfx::MDEntryType::OpenInterest: return "OpenInterest";
        default: return "Unknown";
    }
}

[[nodiscard]] constexpr std::string_view md_update_action_name(nfx::MDUpdateAction a) noexcept {
    switch (a) {
        case nfx::MDUpdateAction::New: return "New";
        case nfx::MDUpdateAction::Change: return "Change";
        case nfx::MDUpdateAction::Delete: return "Delete";
        case nfx::MDUpdateAction::DeleteThru: return "DeleteThru";
        case nfx::MDUpdateAction::DeleteFrom: return "DeleteFrom";
        default: return "Unknown";
    }
}

[[nodiscard]] constexpr std::string_view subscription_type_name(nfx::SubscriptionRequestType t) noexcept {
    switch (t) {
        case nfx::SubscriptionRequestType::Snapshot: return "Snapshot";
        case nfx::SubscriptionRequestType::SnapshotPlusUpdates: return "Subscribe";
        case nfx::SubscriptionRequestType::DisablePreviousSnapshot: return "Unsubscribe";
        default: return "Unknown";
    }
}

[[nodiscard]] constexpr std::string_view md_rej_reason_name(nfx::MDReqRejReason r) noexcept {
    switch (r) {
        case nfx::MDReqRejReason::UnknownSymbol: return "UnknownSymbol";
        case nfx::MDReqRejReason::DuplicateMDReqID: return "DuplicateMDReqID";
        case nfx::MDReqRejReason::InsufficientPermissions: return "InsufficientPermissions";
        case nfx::MDReqRejReason::UnsupportedSubscriptionType: return "UnsupportedSubscriptionType";
        case nfx::MDReqRejReason::UnsupportedMarketDepth: return "UnsupportedMarketDepth";
        case nfx::MDReqRejReason::UnsupportedMDUpdateType: return "UnsupportedMDUpdateType";
        case nfx::MDReqRejReason::UnsupportedAggregatedBook: return "UnsupportedAggregatedBook";
        case nfx::MDReqRejReason::UnsupportedMDEntryType: return "UnsupportedMDEntryType";
        case nfx::MDReqRejReason::UnsupportedTradingSessionID: return "UnsupportedTradingSessionID";
        case nfx::MDReqRejReason::UnsupportedScope: return "UnsupportedScope";
        case nfx::MDReqRejReason::UnsupportedOpenCloseSettleFlag: return "UnsupportedOpenCloseSettleFlag";
        case nfx::MDReqRejReason::UnsupportedMDImplicitDelete: return "UnsupportedMDImplicitDelete";
        case nfx::MDReqRejReason::InsufficientCredit: return "InsufficientCredit";
        case nfx::MDReqRejReason::Other: return "Other";
        default: return "Unknown";
    }
}

} // namespace old

// ============================================================================
// Test data
// ============================================================================

constexpr std::array<nfx::MDEntryType, 13> ALL_MD_ENTRY_TYPES = {
    nfx::MDEntryType::Bid,
    nfx::MDEntryType::Offer,
    nfx::MDEntryType::Trade,
    nfx::MDEntryType::IndexValue,
    nfx::MDEntryType::OpeningPrice,
    nfx::MDEntryType::ClosingPrice,
    nfx::MDEntryType::SettlementPrice,
    nfx::MDEntryType::TradingSessionHighPrice,
    nfx::MDEntryType::TradingSessionLowPrice,
    nfx::MDEntryType::TradingSessionVWAPPrice,
    nfx::MDEntryType::Imbalance,
    nfx::MDEntryType::TradeVolume,
    nfx::MDEntryType::OpenInterest
};

constexpr std::array<nfx::MDUpdateAction, 5> ALL_UPDATE_ACTIONS = {
    nfx::MDUpdateAction::New,
    nfx::MDUpdateAction::Change,
    nfx::MDUpdateAction::Delete,
    nfx::MDUpdateAction::DeleteThru,
    nfx::MDUpdateAction::DeleteFrom
};

constexpr std::array<nfx::SubscriptionRequestType, 3> ALL_SUBSCRIPTION_TYPES = {
    nfx::SubscriptionRequestType::Snapshot,
    nfx::SubscriptionRequestType::SnapshotPlusUpdates,
    nfx::SubscriptionRequestType::DisablePreviousSnapshot
};

constexpr std::array<nfx::MDReqRejReason, 14> ALL_REJ_REASONS = {
    nfx::MDReqRejReason::UnknownSymbol,
    nfx::MDReqRejReason::DuplicateMDReqID,
    nfx::MDReqRejReason::InsufficientPermissions,
    nfx::MDReqRejReason::UnsupportedSubscriptionType,
    nfx::MDReqRejReason::UnsupportedMarketDepth,
    nfx::MDReqRejReason::UnsupportedMDUpdateType,
    nfx::MDReqRejReason::UnsupportedAggregatedBook,
    nfx::MDReqRejReason::UnsupportedMDEntryType,
    nfx::MDReqRejReason::UnsupportedTradingSessionID,
    nfx::MDReqRejReason::UnsupportedScope,
    nfx::MDReqRejReason::UnsupportedOpenCloseSettleFlag,
    nfx::MDReqRejReason::UnsupportedMDImplicitDelete,
    nfx::MDReqRejReason::InsufficientCredit,
    nfx::MDReqRejReason::Other
};

// ============================================================================
// Benchmark functions
// ============================================================================

template<typename Func, typename Array>
double benchmark_all(Func func, const Array& values, size_t iterations) {
    // Warmup
    for (size_t i = 0; i < 1000; ++i) {
        for (const auto& v : values) {
            volatile auto _ = func(v);
            (void)_;
        }
    }

    uint64_t start = rdtsc();
    for (size_t i = 0; i < iterations; ++i) {
        for (const auto& v : values) {
            volatile auto _ = func(v);
            (void)_;
        }
    }
    uint64_t end = rdtsc();

    return static_cast<double>(end - start) / (iterations * values.size());
}

template<typename Func, typename Array>
double benchmark_random(Func func, const Array& values, size_t iterations) {
    std::mt19937 rng(42);
    std::uniform_int_distribution<size_t> dist(0, values.size() - 1);

    // Pre-generate random indices
    std::vector<size_t> indices(iterations);
    for (size_t i = 0; i < iterations; ++i) {
        indices[i] = dist(rng);
    }

    // Warmup
    for (size_t i = 0; i < 1000; ++i) {
        volatile auto _ = func(values[indices[i % iterations]]);
        (void)_;
    }

    uint64_t start = rdtsc();
    for (size_t i = 0; i < iterations; ++i) {
        volatile auto _ = func(values[indices[i]]);
        (void)_;
    }
    uint64_t end = rdtsc();

    return static_cast<double>(end - start) / iterations;
}

// Wrapper lambdas to disambiguate overloaded functions
auto old_md_entry_type = [](nfx::MDEntryType t) { return old::md_entry_type_name(t); };
auto new_md_entry_type = [](nfx::MDEntryType t) { return nfx::md_entry_type_name(t); };
auto old_md_update_action = [](nfx::MDUpdateAction a) { return old::md_update_action_name(a); };
auto new_md_update_action = [](nfx::MDUpdateAction a) { return nfx::md_update_action_name(a); };
auto old_subscription_type = [](nfx::SubscriptionRequestType t) { return old::subscription_type_name(t); };
auto new_subscription_type = [](nfx::SubscriptionRequestType t) { return nfx::subscription_type_name(t); };
auto old_md_rej_reason = [](nfx::MDReqRejReason r) { return old::md_rej_reason_name(r); };
auto new_md_rej_reason = [](nfx::MDReqRejReason r) { return nfx::md_rej_reason_name(r); };

int main() {
    constexpr size_t ITERATIONS = 10'000'000;

    std::cout << "=== Market Data Types Compile-time Optimization Benchmark ===\n";
    std::cout << "Iterations: " << ITERATIONS << "\n\n";

    // MDEntryType (13 cases)
    {
        double old_cycles = benchmark_all(old_md_entry_type, ALL_MD_ENTRY_TYPES, ITERATIONS);
        double new_cycles = benchmark_all(new_md_entry_type, ALL_MD_ENTRY_TYPES, ITERATIONS);
        double improvement = (old_cycles - new_cycles) / old_cycles * 100.0;
        std::cout << "md_entry_type_name (13 cases):\n";
        std::cout << "  OLD (switch): " << old_cycles << " cycles/call\n";
        std::cout << "  NEW (lookup): " << new_cycles << " cycles/call\n";
        std::cout << "  Improvement:  " << improvement << "%\n\n";
    }

    // MDUpdateAction (5 cases)
    {
        double old_cycles = benchmark_all(old_md_update_action, ALL_UPDATE_ACTIONS, ITERATIONS);
        double new_cycles = benchmark_all(new_md_update_action, ALL_UPDATE_ACTIONS, ITERATIONS);
        double improvement = (old_cycles - new_cycles) / old_cycles * 100.0;
        std::cout << "md_update_action_name (5 cases):\n";
        std::cout << "  OLD (switch): " << old_cycles << " cycles/call\n";
        std::cout << "  NEW (lookup): " << new_cycles << " cycles/call\n";
        std::cout << "  Improvement:  " << improvement << "%\n\n";
    }

    // SubscriptionRequestType (3 cases)
    {
        double old_cycles = benchmark_all(old_subscription_type, ALL_SUBSCRIPTION_TYPES, ITERATIONS);
        double new_cycles = benchmark_all(new_subscription_type, ALL_SUBSCRIPTION_TYPES, ITERATIONS);
        double improvement = (old_cycles - new_cycles) / old_cycles * 100.0;
        std::cout << "subscription_type_name (3 cases):\n";
        std::cout << "  OLD (switch): " << old_cycles << " cycles/call\n";
        std::cout << "  NEW (lookup): " << new_cycles << " cycles/call\n";
        std::cout << "  Improvement:  " << improvement << "%\n\n";
    }

    // MDReqRejReason (14 cases)
    {
        double old_cycles = benchmark_all(old_md_rej_reason, ALL_REJ_REASONS, ITERATIONS);
        double new_cycles = benchmark_all(new_md_rej_reason, ALL_REJ_REASONS, ITERATIONS);
        double improvement = (old_cycles - new_cycles) / old_cycles * 100.0;
        std::cout << "md_rej_reason_name (14 cases):\n";
        std::cout << "  OLD (switch): " << old_cycles << " cycles/call\n";
        std::cout << "  NEW (lookup): " << new_cycles << " cycles/call\n";
        std::cout << "  Improvement:  " << improvement << "%\n\n";
    }

    // Random access pattern (most realistic for market data)
    std::cout << "=== Random Access Pattern ===\n\n";

    {
        double old_cycles = benchmark_random(old_md_entry_type, ALL_MD_ENTRY_TYPES, ITERATIONS);
        double new_cycles = benchmark_random(new_md_entry_type, ALL_MD_ENTRY_TYPES, ITERATIONS);
        double improvement = (old_cycles - new_cycles) / old_cycles * 100.0;
        std::cout << "md_entry_type_name (random):\n";
        std::cout << "  OLD (switch): " << old_cycles << " cycles/call\n";
        std::cout << "  NEW (lookup): " << new_cycles << " cycles/call\n";
        std::cout << "  Improvement:  " << improvement << "%\n\n";
    }

    {
        double old_cycles = benchmark_random(old_md_rej_reason, ALL_REJ_REASONS, ITERATIONS);
        double new_cycles = benchmark_random(new_md_rej_reason, ALL_REJ_REASONS, ITERATIONS);
        double improvement = (old_cycles - new_cycles) / old_cycles * 100.0;
        std::cout << "md_rej_reason_name (random):\n";
        std::cout << "  OLD (switch): " << old_cycles << " cycles/call\n";
        std::cout << "  NEW (lookup): " << new_cycles << " cycles/call\n";
        std::cout << "  Improvement:  " << improvement << "%\n\n";
    }

    return 0;
}
