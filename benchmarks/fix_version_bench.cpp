// ============================================================================
// TICKET_023: FIX Version Detection Benchmark
// Compare: Switch-based vs Compile-time Lookup Table
// ============================================================================

#include <iostream>
#include <random>
#include <array>
#include <cstdint>
#include <string_view>

// Include the new implementation
#include "nexusfix/types/fix_version.hpp"

// ============================================================================
// OLD Implementation (switch-based) - for comparison
// ============================================================================

namespace old_impl {

[[nodiscard]] inline std::string_view appl_ver_to_string(char ver) noexcept {
    switch (ver) {
        case '0': return "FIX.2.7";
        case '1': return "FIX.3.0";
        case '2': return "FIX.4.0";
        case '3': return "FIX.4.1";
        case '4': return "FIX.4.2";
        case '5': return "FIX.4.3";
        case '6': return "FIX.4.4";
        case '7': return "FIX.5.0";
        case '8': return "FIX.5.0SP1";
        case '9': return "FIX.5.0SP2";
        default:  return "Unknown";
    }
}

[[nodiscard]] inline nfx::FixVersion detect_version(std::string_view begin_string) noexcept {
    if (begin_string == "FIX.4.0") return nfx::FixVersion::FIX_4_0;
    if (begin_string == "FIX.4.1") return nfx::FixVersion::FIX_4_1;
    if (begin_string == "FIX.4.2") return nfx::FixVersion::FIX_4_2;
    if (begin_string == "FIX.4.3") return nfx::FixVersion::FIX_4_3;
    if (begin_string == "FIX.4.4") return nfx::FixVersion::FIX_4_4;
    if (begin_string == "FIXT.1.1") return nfx::FixVersion::FIXT_1_1;
    return nfx::FixVersion::Unknown;
}

[[nodiscard]] inline bool is_fixt_version(nfx::FixVersion ver) noexcept {
    return ver == nfx::FixVersion::FIXT_1_1 ||
           ver == nfx::FixVersion::FIX_5_0 ||
           ver == nfx::FixVersion::FIX_5_0_SP1 ||
           ver == nfx::FixVersion::FIX_5_0_SP2;
}

[[nodiscard]] inline bool is_fix4_version(nfx::FixVersion ver) noexcept {
    return ver >= nfx::FixVersion::FIX_4_0 && ver <= nfx::FixVersion::FIX_4_4;
}

[[nodiscard]] inline std::string_view version_string(nfx::FixVersion ver) noexcept {
    switch (ver) {
        case nfx::FixVersion::FIX_4_0:     return "FIX.4.0";
        case nfx::FixVersion::FIX_4_1:     return "FIX.4.1";
        case nfx::FixVersion::FIX_4_2:     return "FIX.4.2";
        case nfx::FixVersion::FIX_4_3:     return "FIX.4.3";
        case nfx::FixVersion::FIX_4_4:     return "FIX.4.4";
        case nfx::FixVersion::FIXT_1_1:    return "FIXT.1.1";
        case nfx::FixVersion::FIX_5_0:     return "FIX.5.0";
        case nfx::FixVersion::FIX_5_0_SP1: return "FIX.5.0SP1";
        case nfx::FixVersion::FIX_5_0_SP2: return "FIX.5.0SP2";
        default:                            return "Unknown";
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

constexpr std::array<char, 10> ALL_APPL_VER = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'
};

constexpr std::array<nfx::FixVersion, 10> ALL_VERSIONS = {
    nfx::FixVersion::Unknown,
    nfx::FixVersion::FIX_4_0,
    nfx::FixVersion::FIX_4_1,
    nfx::FixVersion::FIX_4_2,
    nfx::FixVersion::FIX_4_3,
    nfx::FixVersion::FIX_4_4,
    nfx::FixVersion::FIXT_1_1,
    nfx::FixVersion::FIX_5_0,
    nfx::FixVersion::FIX_5_0_SP1,
    nfx::FixVersion::FIX_5_0_SP2
};

constexpr std::array<std::string_view, 7> BEGIN_STRINGS = {
    "FIX.4.0", "FIX.4.1", "FIX.4.2", "FIX.4.3", "FIX.4.4", "FIXT.1.1", "INVALID"
};

// Hot path: most common versions
constexpr std::array<nfx::FixVersion, 3> HOT_VERSIONS = {
    nfx::FixVersion::FIX_4_4,
    nfx::FixVersion::FIX_4_2,
    nfx::FixVersion::FIXT_1_1
};

// ============================================================================
// Benchmark
// ============================================================================

int main() {
    std::cout << "============================================================\n";
    std::cout << "TICKET_023: FIX Version Detection Benchmark\n";
    std::cout << "============================================================\n\n";

    constexpr int ITERATIONS = 10'000'000;
    constexpr int WARMUP = 100'000;

    // ========================================================================
    // Benchmark 1: appl_ver_id::to_string() - 10 cases
    // ========================================================================

    std::cout << "--- appl_ver_id::to_string() (10 versions, " << ITERATIONS << " iterations) ---\n\n";

    // Warmup
    for (int i = 0; i < WARMUP; ++i) {
        for (char ver : ALL_APPL_VER) {
            do_not_optimize(old_impl::appl_ver_to_string(ver));
            do_not_optimize(nfx::appl_ver_id::to_string(ver));
        }
    }

    // OLD: Switch-based
    uint64_t old_appl_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (char ver : ALL_APPL_VER) {
            do_not_optimize(old_impl::appl_ver_to_string(ver));
        }
    }
    uint64_t old_appl_cycles = rdtsc() - old_appl_start;

    // NEW: Lookup table
    uint64_t new_appl_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (char ver : ALL_APPL_VER) {
            do_not_optimize(nfx::appl_ver_id::to_string(ver));
        }
    }
    uint64_t new_appl_cycles = rdtsc() - new_appl_start;

    double appl_ops = static_cast<double>(ITERATIONS) * ALL_APPL_VER.size();
    double old_appl_cpop = static_cast<double>(old_appl_cycles) / appl_ops;
    double new_appl_cpop = static_cast<double>(new_appl_cycles) / appl_ops;
    double appl_improvement = (old_appl_cpop - new_appl_cpop) / old_appl_cpop * 100;

    std::cout << "  OLD (switch):     " << old_appl_cpop << " cycles/op\n";
    std::cout << "  NEW (lookup):     " << new_appl_cpop << " cycles/op\n";
    std::cout << "  Improvement:      " << appl_improvement << "%\n\n";

    // ========================================================================
    // Benchmark 2: version_string() - 10 cases
    // ========================================================================

    std::cout << "--- version_string() (10 versions, " << ITERATIONS << " iterations) ---\n\n";

    // OLD: Switch-based
    uint64_t old_ver_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto ver : ALL_VERSIONS) {
            do_not_optimize(old_impl::version_string(ver));
        }
    }
    uint64_t old_ver_cycles = rdtsc() - old_ver_start;

    // NEW: Lookup table
    uint64_t new_ver_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto ver : ALL_VERSIONS) {
            do_not_optimize(nfx::version_string(ver));
        }
    }
    uint64_t new_ver_cycles = rdtsc() - new_ver_start;

    double ver_ops = static_cast<double>(ITERATIONS) * ALL_VERSIONS.size();
    double old_ver_cpop = static_cast<double>(old_ver_cycles) / ver_ops;
    double new_ver_cpop = static_cast<double>(new_ver_cycles) / ver_ops;
    double ver_improvement = (old_ver_cpop - new_ver_cpop) / old_ver_cpop * 100;

    std::cout << "  OLD (switch):     " << old_ver_cpop << " cycles/op\n";
    std::cout << "  NEW (lookup):     " << new_ver_cpop << " cycles/op\n";
    std::cout << "  Improvement:      " << ver_improvement << "%\n\n";

    // ========================================================================
    // Benchmark 3: is_fixt_version() - 4 comparisons
    // ========================================================================

    std::cout << "--- is_fixt_version() (10 versions, " << ITERATIONS << " iterations) ---\n\n";

    // OLD: if-chain
    uint64_t old_fixt_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto ver : ALL_VERSIONS) {
            do_not_optimize(old_impl::is_fixt_version(ver));
        }
    }
    uint64_t old_fixt_cycles = rdtsc() - old_fixt_start;

    // NEW: Lookup table
    uint64_t new_fixt_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto ver : ALL_VERSIONS) {
            do_not_optimize(nfx::is_fixt_version(ver));
        }
    }
    uint64_t new_fixt_cycles = rdtsc() - new_fixt_start;

    double fixt_ops = static_cast<double>(ITERATIONS) * ALL_VERSIONS.size();
    double old_fixt_cpop = static_cast<double>(old_fixt_cycles) / fixt_ops;
    double new_fixt_cpop = static_cast<double>(new_fixt_cycles) / fixt_ops;
    double fixt_improvement = (old_fixt_cpop - new_fixt_cpop) / old_fixt_cpop * 100;

    std::cout << "  OLD (if-chain):   " << old_fixt_cpop << " cycles/op\n";
    std::cout << "  NEW (lookup):     " << new_fixt_cpop << " cycles/op\n";
    std::cout << "  Improvement:      " << fixt_improvement << "%\n\n";

    // ========================================================================
    // Benchmark 4: is_fix4_version() - range check
    // ========================================================================

    std::cout << "--- is_fix4_version() (10 versions, " << ITERATIONS << " iterations) ---\n\n";

    // OLD: Range check
    uint64_t old_fix4_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto ver : ALL_VERSIONS) {
            do_not_optimize(old_impl::is_fix4_version(ver));
        }
    }
    uint64_t old_fix4_cycles = rdtsc() - old_fix4_start;

    // NEW: Lookup table
    uint64_t new_fix4_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto ver : ALL_VERSIONS) {
            do_not_optimize(nfx::is_fix4_version(ver));
        }
    }
    uint64_t new_fix4_cycles = rdtsc() - new_fix4_start;

    double fix4_ops = static_cast<double>(ITERATIONS) * ALL_VERSIONS.size();
    double old_fix4_cpop = static_cast<double>(old_fix4_cycles) / fix4_ops;
    double new_fix4_cpop = static_cast<double>(new_fix4_cycles) / fix4_ops;
    double fix4_improvement = (old_fix4_cpop - new_fix4_cpop) / old_fix4_cpop * 100;

    std::cout << "  OLD (range):      " << old_fix4_cpop << " cycles/op\n";
    std::cout << "  NEW (lookup):     " << new_fix4_cpop << " cycles/op\n";
    std::cout << "  Improvement:      " << fix4_improvement << "%\n\n";

    // ========================================================================
    // Benchmark 5: detect_version() - if-chain with string comparison
    // ========================================================================

    std::cout << "--- detect_version() (7 strings, " << ITERATIONS << " iterations) ---\n\n";

    // OLD: if-chain
    uint64_t old_detect_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto str : BEGIN_STRINGS) {
            do_not_optimize(old_impl::detect_version(str));
        }
    }
    uint64_t old_detect_cycles = rdtsc() - old_detect_start;

    // NEW: Optimized detection
    uint64_t new_detect_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto str : BEGIN_STRINGS) {
            do_not_optimize(nfx::detect_version(str));
        }
    }
    uint64_t new_detect_cycles = rdtsc() - new_detect_start;

    double detect_ops = static_cast<double>(ITERATIONS) * BEGIN_STRINGS.size();
    double old_detect_cpop = static_cast<double>(old_detect_cycles) / detect_ops;
    double new_detect_cpop = static_cast<double>(new_detect_cycles) / detect_ops;
    double detect_improvement = (old_detect_cpop - new_detect_cpop) / old_detect_cpop * 100;

    std::cout << "  OLD (if-chain):   " << old_detect_cpop << " cycles/op\n";
    std::cout << "  NEW (optimized):  " << new_detect_cpop << " cycles/op\n";
    std::cout << "  Improvement:      " << detect_improvement << "%\n\n";

    // ========================================================================
    // Benchmark 6: Random access pattern
    // ========================================================================

    std::cout << "--- Random Access Pattern (" << ITERATIONS << " iterations) ---\n\n";

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, ALL_VERSIONS.size() - 1);
    std::array<nfx::FixVersion, 1024> random_versions;
    for (auto& v : random_versions) {
        v = ALL_VERSIONS[dist(rng)];
    }

    // OLD: Random access
    uint64_t old_rand_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto ver : random_versions) {
            do_not_optimize(old_impl::version_string(ver));
        }
    }
    uint64_t old_rand_cycles = rdtsc() - old_rand_start;

    // NEW: Random access
    uint64_t new_rand_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto ver : random_versions) {
            do_not_optimize(nfx::version_string(ver));
        }
    }
    uint64_t new_rand_cycles = rdtsc() - new_rand_start;

    double rand_ops = static_cast<double>(ITERATIONS) * random_versions.size();
    double old_rand_cpop = static_cast<double>(old_rand_cycles) / rand_ops;
    double new_rand_cpop = static_cast<double>(new_rand_cycles) / rand_ops;
    double rand_improvement = (old_rand_cpop - new_rand_cpop) / old_rand_cpop * 100;

    std::cout << "  OLD (switch):     " << old_rand_cpop << " cycles/op\n";
    std::cout << "  NEW (lookup):     " << new_rand_cpop << " cycles/op\n";
    std::cout << "  Improvement:      " << rand_improvement << "%\n\n";

    // ========================================================================
    // Summary
    // ========================================================================

    double avg_improvement = (appl_improvement + ver_improvement + fixt_improvement +
                              fix4_improvement + detect_improvement + rand_improvement) / 6.0;

    std::cout << "============================================================\n";
    std::cout << "SUMMARY\n";
    std::cout << "============================================================\n\n";

    std::cout << "| Function           | OLD (cycles) | NEW (cycles) | Improvement |\n";
    std::cout << "|--------------------|--------------|--------------|-------------|\n";
    std::cout << "| appl_ver::to_string| " << old_appl_cpop << "       | " << new_appl_cpop << "       | " << appl_improvement << "% |\n";
    std::cout << "| version_string()   | " << old_ver_cpop << "       | " << new_ver_cpop << "       | " << ver_improvement << "% |\n";
    std::cout << "| is_fixt_version()  | " << old_fixt_cpop << "       | " << new_fixt_cpop << "       | " << fixt_improvement << "% |\n";
    std::cout << "| is_fix4_version()  | " << old_fix4_cpop << "       | " << new_fix4_cpop << "       | " << fix4_improvement << "% |\n";
    std::cout << "| detect_version()   | " << old_detect_cpop << "       | " << new_detect_cpop << "       | " << detect_improvement << "% |\n";
    std::cout << "| Random access      | " << old_rand_cpop << "       | " << new_rand_cpop << "       | " << rand_improvement << "% |\n";
    std::cout << "|--------------------|--------------|--------------|-------------|\n";
    std::cout << "| Average            |              |              | " << avg_improvement << "% |\n";

    std::cout << "\nSwitch/if-chain cases eliminated: 10 (appl_ver) + 10 (version_string) + 4 (is_fixt) + 6 (detect) = 30\n";

    return 0;
}
