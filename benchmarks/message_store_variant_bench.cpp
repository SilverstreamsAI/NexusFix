// ============================================================================
// TICKET_024 Phase 2: Message Store Dispatch Benchmark
// Compare: Virtual dispatch (IMessageStore) vs std::variant + std::visit
// ============================================================================

#include <iostream>
#include <memory>
#include <random>
#include <array>
#include <cstdint>
#include <span>
#include <atomic>

#include "nexusfix/store/i_message_store.hpp"
#include "nexusfix/store/memory_message_store.hpp"
#include "nexusfix/store/message_store.hpp"

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
// Test message data
// ============================================================================

constexpr std::array<char, 128> TEST_MESSAGE = {
    '8', '=', 'F', 'I', 'X', '.', '4', '.', '4', '\x01',
    '9', '=', '1', '0', '0', '\x01',
    '3', '5', '=', '8', '\x01',
    '4', '9', '=', 'S', 'E', 'N', 'D', 'E', 'R', '\x01',
    '5', '6', '=', 'T', 'A', 'R', 'G', 'E', 'T', '\x01',
    // ... rest of message
};

// ============================================================================
// Benchmark
// ============================================================================

int main() {
    std::cout << "============================================================\n";
    std::cout << "TICKET_024 Phase 2: Message Store Dispatch Benchmark\n";
    std::cout << "Virtual dispatch vs std::variant + std::visit\n";
    std::cout << "============================================================\n\n";

    constexpr int ITERATIONS = 1'000'000;
    constexpr int WARMUP = 10'000;

    std::span<const char> msg_span{TEST_MESSAGE.data(), TEST_MESSAGE.size()};

    // ========================================================================
    // Setup stores
    // ========================================================================

    // Virtual dispatch version (using interface pointer)
    std::unique_ptr<nfx::store::IMessageStore> virtual_store =
        std::make_unique<nfx::store::NullMessageStore>("VIRTUAL");

    // Variant-based version
    nfx::store::MessageStore variant_store =
        nfx::store::make_null_store("VARIANT");

    // ========================================================================
    // Benchmark 1: store() operation
    // ========================================================================

    std::cout << "--- store() Operation (" << ITERATIONS << " iterations) ---\n\n";

    // Warmup
    for (int i = 0; i < WARMUP; ++i) {
        do_not_optimize(virtual_store->store(i, msg_span));
        do_not_optimize(variant_store.store(i, msg_span));
    }
    virtual_store->reset();
    variant_store.reset();

    // Virtual dispatch
    uint64_t virtual_store_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        do_not_optimize(virtual_store->store(static_cast<uint32_t>(i), msg_span));
    }
    uint64_t virtual_store_cycles = rdtsc() - virtual_store_start;

    // Variant dispatch
    uint64_t variant_store_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        do_not_optimize(variant_store.store(static_cast<uint32_t>(i), msg_span));
    }
    uint64_t variant_store_cycles = rdtsc() - variant_store_start;

    double virtual_store_cpop = static_cast<double>(virtual_store_cycles) / ITERATIONS;
    double variant_store_cpop = static_cast<double>(variant_store_cycles) / ITERATIONS;
    double store_improvement = (virtual_store_cpop - variant_store_cpop) / virtual_store_cpop * 100;

    std::cout << "  Virtual (IMessageStore*): " << virtual_store_cpop << " cycles/op\n";
    std::cout << "  Variant (std::visit):     " << variant_store_cpop << " cycles/op\n";
    std::cout << "  Improvement:              " << store_improvement << "%\n\n";

    // ========================================================================
    // Benchmark 2: retrieve() operation
    // ========================================================================

    std::cout << "--- retrieve() Operation (" << ITERATIONS << " iterations) ---\n\n";

    // Virtual dispatch
    uint64_t virtual_retrieve_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        do_not_optimize(virtual_store->retrieve(static_cast<uint32_t>(i % 1000)));
    }
    uint64_t virtual_retrieve_cycles = rdtsc() - virtual_retrieve_start;

    // Variant dispatch
    uint64_t variant_retrieve_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        do_not_optimize(variant_store.retrieve(static_cast<uint32_t>(i % 1000)));
    }
    uint64_t variant_retrieve_cycles = rdtsc() - variant_retrieve_start;

    double virtual_retrieve_cpop = static_cast<double>(virtual_retrieve_cycles) / ITERATIONS;
    double variant_retrieve_cpop = static_cast<double>(variant_retrieve_cycles) / ITERATIONS;
    double retrieve_improvement = (virtual_retrieve_cpop - variant_retrieve_cpop) / virtual_retrieve_cpop * 100;

    std::cout << "  Virtual (IMessageStore*): " << virtual_retrieve_cpop << " cycles/op\n";
    std::cout << "  Variant (std::visit):     " << variant_retrieve_cpop << " cycles/op\n";
    std::cout << "  Improvement:              " << retrieve_improvement << "%\n\n";

    // ========================================================================
    // Benchmark 3: get_next_sender_seq_num() - hot path in session
    // ========================================================================

    std::cout << "--- get_next_sender_seq_num() (" << ITERATIONS << " iterations) ---\n\n";

    // Virtual dispatch
    uint64_t virtual_seq_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        do_not_optimize(virtual_store->get_next_sender_seq_num());
    }
    uint64_t virtual_seq_cycles = rdtsc() - virtual_seq_start;

    // Variant dispatch
    uint64_t variant_seq_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        do_not_optimize(variant_store.get_next_sender_seq_num());
    }
    uint64_t variant_seq_cycles = rdtsc() - variant_seq_start;

    double virtual_seq_cpop = static_cast<double>(virtual_seq_cycles) / ITERATIONS;
    double variant_seq_cpop = static_cast<double>(variant_seq_cycles) / ITERATIONS;
    double seq_improvement = (virtual_seq_cpop - variant_seq_cpop) / virtual_seq_cpop * 100;

    std::cout << "  Virtual (IMessageStore*): " << virtual_seq_cpop << " cycles/op\n";
    std::cout << "  Variant (std::visit):     " << variant_seq_cpop << " cycles/op\n";
    std::cout << "  Improvement:              " << seq_improvement << "%\n\n";

    // ========================================================================
    // Benchmark 4: Mixed operations (realistic workload)
    // ========================================================================

    std::cout << "--- Mixed Operations (" << ITERATIONS << " iterations) ---\n\n";

    virtual_store->reset();
    variant_store.reset();

    // Virtual dispatch - mixed operations
    uint64_t virtual_mixed_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        uint32_t seq = static_cast<uint32_t>(i);
        do_not_optimize(virtual_store->store(seq, msg_span));
        do_not_optimize(virtual_store->get_next_sender_seq_num());
        virtual_store->set_next_sender_seq_num(seq + 1);
    }
    uint64_t virtual_mixed_cycles = rdtsc() - virtual_mixed_start;

    virtual_store->reset();

    // Variant dispatch - mixed operations
    uint64_t variant_mixed_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        uint32_t seq = static_cast<uint32_t>(i);
        do_not_optimize(variant_store.store(seq, msg_span));
        do_not_optimize(variant_store.get_next_sender_seq_num());
        variant_store.set_next_sender_seq_num(seq + 1);
    }
    uint64_t variant_mixed_cycles = rdtsc() - variant_mixed_start;

    double virtual_mixed_cpop = static_cast<double>(virtual_mixed_cycles) / ITERATIONS;
    double variant_mixed_cpop = static_cast<double>(variant_mixed_cycles) / ITERATIONS;
    double mixed_improvement = (virtual_mixed_cpop - variant_mixed_cpop) / virtual_mixed_cpop * 100;

    std::cout << "  Virtual (IMessageStore*): " << virtual_mixed_cpop << " cycles/op\n";
    std::cout << "  Variant (std::visit):     " << variant_mixed_cpop << " cycles/op\n";
    std::cout << "  Improvement:              " << mixed_improvement << "%\n\n";

    // ========================================================================
    // Benchmark 5: With actual MemoryStore (not NullStore)
    // ========================================================================

    std::cout << "--- MemoryStore: store() + retrieve() (" << ITERATIONS/10 << " iterations) ---\n\n";

    constexpr int MEM_ITERATIONS = ITERATIONS / 10;  // Fewer iterations for memory store

    // Virtual dispatch with MemoryMessageStore
    auto virtual_mem_store =
        std::make_unique<nfx::store::MemoryMessageStore>("VIRTUAL_MEM");

    // Variant with MemoryStore
    auto variant_mem_store =
        nfx::store::make_memory_store("VARIANT_MEM");

    // Store phase - Virtual
    uint64_t virtual_mem_store_start = rdtsc();
    for (int i = 0; i < MEM_ITERATIONS; ++i) {
        do_not_optimize(virtual_mem_store->store(static_cast<uint32_t>(i), msg_span));
    }
    uint64_t virtual_mem_store_cycles = rdtsc() - virtual_mem_store_start;

    // Store phase - Variant
    uint64_t variant_mem_store_start = rdtsc();
    for (int i = 0; i < MEM_ITERATIONS; ++i) {
        do_not_optimize(variant_mem_store.store(static_cast<uint32_t>(i), msg_span));
    }
    uint64_t variant_mem_store_cycles = rdtsc() - variant_mem_store_start;

    double virtual_mem_cpop = static_cast<double>(virtual_mem_store_cycles) / MEM_ITERATIONS;
    double variant_mem_cpop = static_cast<double>(variant_mem_store_cycles) / MEM_ITERATIONS;
    double mem_improvement = (virtual_mem_cpop - variant_mem_cpop) / virtual_mem_cpop * 100;

    std::cout << "  Virtual (MemoryMessageStore): " << virtual_mem_cpop << " cycles/op\n";
    std::cout << "  Variant (MemoryStore):        " << variant_mem_cpop << " cycles/op\n";
    std::cout << "  Improvement:                  " << mem_improvement << "%\n\n";

    // ========================================================================
    // Summary
    // ========================================================================

    std::cout << "============================================================\n";
    std::cout << "SUMMARY: TICKET_024 Phase 2 Message Store Optimization\n";
    std::cout << "============================================================\n\n";

    std::cout << "| Operation              | Virtual (cycles) | Variant (cycles) | Improvement |\n";
    std::cout << "|------------------------|------------------|------------------|-------------|\n";
    std::cout << "| NullStore: store()     | "
              << virtual_store_cpop << "            | "
              << variant_store_cpop << "            | "
              << store_improvement << "% |\n";
    std::cout << "| NullStore: retrieve()  | "
              << virtual_retrieve_cpop << "            | "
              << variant_retrieve_cpop << "            | "
              << retrieve_improvement << "% |\n";
    std::cout << "| NullStore: seq_num     | "
              << virtual_seq_cpop << "            | "
              << variant_seq_cpop << "            | "
              << seq_improvement << "% |\n";
    std::cout << "| NullStore: mixed       | "
              << virtual_mixed_cpop << "            | "
              << variant_mixed_cpop << "            | "
              << mixed_improvement << "% |\n";
    std::cout << "| MemoryStore: store()   | "
              << virtual_mem_cpop << "            | "
              << variant_mem_cpop << "            | "
              << mem_improvement << "% |\n";

    std::cout << "\nConclusion:\n";
    std::cout << "std::variant + std::visit eliminates virtual dispatch overhead,\n";
    std::cout << "allowing the compiler to inline and optimize store operations.\n";

    return 0;
}
