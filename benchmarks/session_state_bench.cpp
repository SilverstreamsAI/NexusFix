// ============================================================================
// TICKET_023: Session State Machine Benchmark
// Compare: Switch-based vs Compile-time Lookup Table
// ============================================================================

#include <iostream>
#include <random>
#include <array>
#include <cstdint>
#include <string_view>

// Include the new implementation
#include "nexusfix/session/state.hpp"

// ============================================================================
// OLD Implementation (switch-based) - for comparison
// ============================================================================

namespace old_impl {

[[nodiscard]] inline std::string_view state_name(nfx::SessionState state) noexcept {
    switch (state) {
        case nfx::SessionState::Disconnected:    return "Disconnected";
        case nfx::SessionState::SocketConnected: return "SocketConnected";
        case nfx::SessionState::LogonSent:       return "LogonSent";
        case nfx::SessionState::LogonReceived:   return "LogonReceived";
        case nfx::SessionState::Active:          return "Active";
        case nfx::SessionState::LogoutPending:   return "LogoutPending";
        case nfx::SessionState::LogoutReceived:  return "LogoutReceived";
        case nfx::SessionState::Reconnecting:    return "Reconnecting";
        case nfx::SessionState::Error:           return "Error";
    }
    return "Unknown";
}

[[nodiscard]] inline bool is_connected(nfx::SessionState state) noexcept {
    return state == nfx::SessionState::SocketConnected ||
           state == nfx::SessionState::LogonSent ||
           state == nfx::SessionState::LogonReceived ||
           state == nfx::SessionState::Active ||
           state == nfx::SessionState::LogoutPending;
}

[[nodiscard]] inline std::string_view event_name(nfx::SessionEvent event) noexcept {
    switch (event) {
        case nfx::SessionEvent::Connect:            return "Connect";
        case nfx::SessionEvent::Disconnect:         return "Disconnect";
        case nfx::SessionEvent::LogonSent:          return "LogonSent";
        case nfx::SessionEvent::LogonReceived:      return "LogonReceived";
        case nfx::SessionEvent::LogonAcknowledged:  return "LogonAcknowledged";
        case nfx::SessionEvent::LogonRejected:      return "LogonRejected";
        case nfx::SessionEvent::LogoutSent:         return "LogoutSent";
        case nfx::SessionEvent::LogoutReceived:     return "LogoutReceived";
        case nfx::SessionEvent::HeartbeatTimeout:   return "HeartbeatTimeout";
        case nfx::SessionEvent::TestRequestSent:    return "TestRequestSent";
        case nfx::SessionEvent::TestRequestReceived: return "TestRequestReceived";
        case nfx::SessionEvent::MessageReceived:    return "MessageReceived";
        case nfx::SessionEvent::Error:              return "Error";
    }
    return "Unknown";
}

[[nodiscard]] inline nfx::SessionState next_state(
    nfx::SessionState current,
    nfx::SessionEvent event) noexcept
{
    switch (current) {
        case nfx::SessionState::Disconnected:
            if (event == nfx::SessionEvent::Connect) return nfx::SessionState::SocketConnected;
            break;

        case nfx::SessionState::SocketConnected:
            if (event == nfx::SessionEvent::LogonSent) return nfx::SessionState::LogonSent;
            if (event == nfx::SessionEvent::LogonReceived) return nfx::SessionState::LogonReceived;
            if (event == nfx::SessionEvent::Disconnect) return nfx::SessionState::Disconnected;
            break;

        case nfx::SessionState::LogonSent:
            if (event == nfx::SessionEvent::LogonReceived) return nfx::SessionState::Active;
            if (event == nfx::SessionEvent::LogonRejected) return nfx::SessionState::Disconnected;
            if (event == nfx::SessionEvent::Disconnect) return nfx::SessionState::Disconnected;
            if (event == nfx::SessionEvent::HeartbeatTimeout) return nfx::SessionState::Error;
            break;

        case nfx::SessionState::LogonReceived:
            if (event == nfx::SessionEvent::LogonAcknowledged) return nfx::SessionState::Active;
            if (event == nfx::SessionEvent::LogonRejected) return nfx::SessionState::Disconnected;
            if (event == nfx::SessionEvent::Disconnect) return nfx::SessionState::Disconnected;
            break;

        case nfx::SessionState::Active:
            if (event == nfx::SessionEvent::LogoutSent) return nfx::SessionState::LogoutPending;
            if (event == nfx::SessionEvent::LogoutReceived) return nfx::SessionState::LogoutReceived;
            if (event == nfx::SessionEvent::Disconnect) return nfx::SessionState::Reconnecting;
            if (event == nfx::SessionEvent::HeartbeatTimeout) return nfx::SessionState::Error;
            if (event == nfx::SessionEvent::Error) return nfx::SessionState::Error;
            break;

        case nfx::SessionState::LogoutPending:
            if (event == nfx::SessionEvent::LogoutReceived) return nfx::SessionState::Disconnected;
            if (event == nfx::SessionEvent::HeartbeatTimeout) return nfx::SessionState::Disconnected;
            if (event == nfx::SessionEvent::Disconnect) return nfx::SessionState::Disconnected;
            break;

        case nfx::SessionState::LogoutReceived:
            if (event == nfx::SessionEvent::LogoutSent) return nfx::SessionState::Disconnected;
            if (event == nfx::SessionEvent::Disconnect) return nfx::SessionState::Disconnected;
            break;

        case nfx::SessionState::Reconnecting:
            if (event == nfx::SessionEvent::Connect) return nfx::SessionState::SocketConnected;
            if (event == nfx::SessionEvent::Error) return nfx::SessionState::Error;
            break;

        case nfx::SessionState::Error:
            if (event == nfx::SessionEvent::Connect) return nfx::SessionState::SocketConnected;
            break;
    }

    return current;
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

constexpr std::array<nfx::SessionState, 9> ALL_STATES = {
    nfx::SessionState::Disconnected,
    nfx::SessionState::SocketConnected,
    nfx::SessionState::LogonSent,
    nfx::SessionState::LogonReceived,
    nfx::SessionState::Active,
    nfx::SessionState::LogoutPending,
    nfx::SessionState::LogoutReceived,
    nfx::SessionState::Reconnecting,
    nfx::SessionState::Error
};

constexpr std::array<nfx::SessionEvent, 13> ALL_EVENTS = {
    nfx::SessionEvent::Connect,
    nfx::SessionEvent::Disconnect,
    nfx::SessionEvent::LogonSent,
    nfx::SessionEvent::LogonReceived,
    nfx::SessionEvent::LogonAcknowledged,
    nfx::SessionEvent::LogonRejected,
    nfx::SessionEvent::LogoutSent,
    nfx::SessionEvent::LogoutReceived,
    nfx::SessionEvent::HeartbeatTimeout,
    nfx::SessionEvent::TestRequestSent,
    nfx::SessionEvent::TestRequestReceived,
    nfx::SessionEvent::MessageReceived,
    nfx::SessionEvent::Error
};

// Hot path: common states during normal operation
constexpr std::array<nfx::SessionState, 3> HOT_STATES = {
    nfx::SessionState::Active,
    nfx::SessionState::SocketConnected,
    nfx::SessionState::LogonSent
};

// Hot path: common events during normal operation
constexpr std::array<nfx::SessionEvent, 4> HOT_EVENTS = {
    nfx::SessionEvent::MessageReceived,
    nfx::SessionEvent::LogonReceived,
    nfx::SessionEvent::LogoutReceived,
    nfx::SessionEvent::Disconnect
};

// ============================================================================
// Benchmark
// ============================================================================

int main() {
    std::cout << "============================================================\n";
    std::cout << "TICKET_023: Session State Machine Benchmark\n";
    std::cout << "============================================================\n\n";

    constexpr int ITERATIONS = 10'000'000;
    constexpr int WARMUP = 100'000;

    // ========================================================================
    // Benchmark 1: state_name() - All states
    // ========================================================================

    std::cout << "--- state_name() (9 states, " << ITERATIONS << " iterations) ---\n\n";

    // Warmup
    for (int i = 0; i < WARMUP; ++i) {
        for (auto state : ALL_STATES) {
            do_not_optimize(old_impl::state_name(state));
            do_not_optimize(nfx::state_name(state));
        }
    }

    // OLD: Switch-based
    uint64_t old_state_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto state : ALL_STATES) {
            do_not_optimize(old_impl::state_name(state));
        }
    }
    uint64_t old_state_cycles = rdtsc() - old_state_start;

    // NEW: Lookup table
    uint64_t new_state_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto state : ALL_STATES) {
            do_not_optimize(nfx::state_name(state));
        }
    }
    uint64_t new_state_cycles = rdtsc() - new_state_start;

    double state_ops = static_cast<double>(ITERATIONS) * ALL_STATES.size();
    double old_state_cpop = static_cast<double>(old_state_cycles) / state_ops;
    double new_state_cpop = static_cast<double>(new_state_cycles) / state_ops;
    double state_improvement = (old_state_cpop - new_state_cpop) / old_state_cpop * 100;

    std::cout << "  OLD (switch):     " << old_state_cpop << " cycles/op\n";
    std::cout << "  NEW (lookup):     " << new_state_cpop << " cycles/op\n";
    std::cout << "  Improvement:      " << state_improvement << "%\n\n";

    // ========================================================================
    // Benchmark 2: is_connected() - All states
    // ========================================================================

    std::cout << "--- is_connected() (9 states, " << ITERATIONS << " iterations) ---\n\n";

    // OLD: if-chain
    uint64_t old_conn_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto state : ALL_STATES) {
            do_not_optimize(old_impl::is_connected(state));
        }
    }
    uint64_t old_conn_cycles = rdtsc() - old_conn_start;

    // NEW: Lookup table
    uint64_t new_conn_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto state : ALL_STATES) {
            do_not_optimize(nfx::is_connected(state));
        }
    }
    uint64_t new_conn_cycles = rdtsc() - new_conn_start;

    double conn_ops = static_cast<double>(ITERATIONS) * ALL_STATES.size();
    double old_conn_cpop = static_cast<double>(old_conn_cycles) / conn_ops;
    double new_conn_cpop = static_cast<double>(new_conn_cycles) / conn_ops;
    double conn_improvement = (old_conn_cpop - new_conn_cpop) / old_conn_cpop * 100;

    std::cout << "  OLD (if-chain):   " << old_conn_cpop << " cycles/op\n";
    std::cout << "  NEW (lookup):     " << new_conn_cpop << " cycles/op\n";
    std::cout << "  Improvement:      " << conn_improvement << "%\n\n";

    // ========================================================================
    // Benchmark 3: event_name() - All events
    // ========================================================================

    std::cout << "--- event_name() (13 events, " << ITERATIONS << " iterations) ---\n\n";

    // OLD: Switch-based
    uint64_t old_event_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto event : ALL_EVENTS) {
            do_not_optimize(old_impl::event_name(event));
        }
    }
    uint64_t old_event_cycles = rdtsc() - old_event_start;

    // NEW: Lookup table
    uint64_t new_event_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto event : ALL_EVENTS) {
            do_not_optimize(nfx::event_name(event));
        }
    }
    uint64_t new_event_cycles = rdtsc() - new_event_start;

    double event_ops = static_cast<double>(ITERATIONS) * ALL_EVENTS.size();
    double old_event_cpop = static_cast<double>(old_event_cycles) / event_ops;
    double new_event_cpop = static_cast<double>(new_event_cycles) / event_ops;
    double event_improvement = (old_event_cpop - new_event_cpop) / old_event_cpop * 100;

    std::cout << "  OLD (switch):     " << old_event_cpop << " cycles/op\n";
    std::cout << "  NEW (lookup):     " << new_event_cpop << " cycles/op\n";
    std::cout << "  Improvement:      " << event_improvement << "%\n\n";

    // ========================================================================
    // Benchmark 4: next_state() - All state x event combinations
    // ========================================================================

    std::cout << "--- next_state() (9x13=117 combos, " << ITERATIONS << " iterations) ---\n\n";

    // OLD: Nested switch/if
    uint64_t old_next_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto state : ALL_STATES) {
            for (auto event : ALL_EVENTS) {
                do_not_optimize(old_impl::next_state(state, event));
            }
        }
    }
    uint64_t old_next_cycles = rdtsc() - old_next_start;

    // NEW: 2D lookup table
    uint64_t new_next_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto state : ALL_STATES) {
            for (auto event : ALL_EVENTS) {
                do_not_optimize(nfx::next_state(state, event));
            }
        }
    }
    uint64_t new_next_cycles = rdtsc() - new_next_start;

    double next_ops = static_cast<double>(ITERATIONS) * ALL_STATES.size() * ALL_EVENTS.size();
    double old_next_cpop = static_cast<double>(old_next_cycles) / next_ops;
    double new_next_cpop = static_cast<double>(new_next_cycles) / next_ops;
    double next_improvement = (old_next_cpop - new_next_cpop) / old_next_cpop * 100;

    std::cout << "  OLD (switch/if):  " << old_next_cpop << " cycles/op\n";
    std::cout << "  NEW (2D lookup):  " << new_next_cpop << " cycles/op\n";
    std::cout << "  Improvement:      " << next_improvement << "%\n\n";

    // ========================================================================
    // Benchmark 5: Hot path - common state transitions
    // ========================================================================

    std::cout << "--- Hot Path next_state() (common transitions, " << ITERATIONS << " iterations) ---\n\n";

    // OLD: Hot path
    uint64_t old_hot_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto state : HOT_STATES) {
            for (auto event : HOT_EVENTS) {
                do_not_optimize(old_impl::next_state(state, event));
            }
        }
    }
    uint64_t old_hot_cycles = rdtsc() - old_hot_start;

    // NEW: Hot path
    uint64_t new_hot_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto state : HOT_STATES) {
            for (auto event : HOT_EVENTS) {
                do_not_optimize(nfx::next_state(state, event));
            }
        }
    }
    uint64_t new_hot_cycles = rdtsc() - new_hot_start;

    double hot_ops = static_cast<double>(ITERATIONS) * HOT_STATES.size() * HOT_EVENTS.size();
    double old_hot_cpop = static_cast<double>(old_hot_cycles) / hot_ops;
    double new_hot_cpop = static_cast<double>(new_hot_cycles) / hot_ops;
    double hot_improvement = (old_hot_cpop - new_hot_cpop) / old_hot_cpop * 100;

    std::cout << "  OLD (switch/if):  " << old_hot_cpop << " cycles/op\n";
    std::cout << "  NEW (2D lookup):  " << new_hot_cpop << " cycles/op\n";
    std::cout << "  Improvement:      " << hot_improvement << "%\n\n";

    // ========================================================================
    // Benchmark 6: Random access pattern
    // ========================================================================

    std::cout << "--- Random Access Pattern (" << ITERATIONS << " iterations) ---\n\n";

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> state_dist(0, ALL_STATES.size() - 1);
    std::uniform_int_distribution<int> event_dist(0, ALL_EVENTS.size() - 1);

    std::array<std::pair<nfx::SessionState, nfx::SessionEvent>, 1024> random_pairs;
    for (auto& p : random_pairs) {
        p.first = ALL_STATES[state_dist(rng)];
        p.second = ALL_EVENTS[event_dist(rng)];
    }

    // OLD: Random access
    uint64_t old_rand_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (const auto& p : random_pairs) {
            do_not_optimize(old_impl::next_state(p.first, p.second));
        }
    }
    uint64_t old_rand_cycles = rdtsc() - old_rand_start;

    // NEW: Random access
    uint64_t new_rand_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (const auto& p : random_pairs) {
            do_not_optimize(nfx::next_state(p.first, p.second));
        }
    }
    uint64_t new_rand_cycles = rdtsc() - new_rand_start;

    double rand_ops = static_cast<double>(ITERATIONS) * random_pairs.size();
    double old_rand_cpop = static_cast<double>(old_rand_cycles) / rand_ops;
    double new_rand_cpop = static_cast<double>(new_rand_cycles) / rand_ops;
    double rand_improvement = (old_rand_cpop - new_rand_cpop) / old_rand_cpop * 100;

    std::cout << "  OLD (switch/if):  " << old_rand_cpop << " cycles/op\n";
    std::cout << "  NEW (2D lookup):  " << new_rand_cpop << " cycles/op\n";
    std::cout << "  Improvement:      " << rand_improvement << "%\n\n";

    // ========================================================================
    // Summary
    // ========================================================================

    double avg_improvement = (state_improvement + conn_improvement + event_improvement +
                              next_improvement + hot_improvement + rand_improvement) / 6.0;

    std::cout << "============================================================\n";
    std::cout << "SUMMARY\n";
    std::cout << "============================================================\n\n";

    std::cout << "| Function         | OLD (cycles) | NEW (cycles) | Improvement |\n";
    std::cout << "|------------------|--------------|--------------|-------------|\n";
    std::cout << "| state_name()     | " << old_state_cpop << "       | " << new_state_cpop << "       | " << state_improvement << "% |\n";
    std::cout << "| is_connected()   | " << old_conn_cpop << "       | " << new_conn_cpop << "       | " << conn_improvement << "% |\n";
    std::cout << "| event_name()     | " << old_event_cpop << "       | " << new_event_cpop << "       | " << event_improvement << "% |\n";
    std::cout << "| next_state()     | " << old_next_cpop << "       | " << new_next_cpop << "       | " << next_improvement << "% |\n";
    std::cout << "| Hot path         | " << old_hot_cpop << "       | " << new_hot_cpop << "       | " << hot_improvement << "% |\n";
    std::cout << "| Random access    | " << old_rand_cpop << "       | " << new_rand_cpop << "       | " << rand_improvement << "% |\n";
    std::cout << "|------------------|--------------|--------------|-------------|\n";
    std::cout << "| Average          |              |              | " << avg_improvement << "% |\n";

    std::cout << "\nSwitch cases eliminated: 9 (state_name) + 13 (event_name) + ~30 (next_state) = ~52\n";

    return 0;
}
