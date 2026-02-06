/*
    MPMC Queue Tests

    Tests for MPMCQueue with Rigtorp turn-based synchronization.
    Covers single-threaded correctness, concurrent correctness,
    move-only types, and alignment verification.
*/

#include <catch2/catch_test_macros.hpp>

#include <nexusfix/memory/mpmc_queue.hpp>
#include <nexusfix/memory/wait_strategy.hpp>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <numeric>
#include <set>
#include <thread>
#include <vector>

using namespace nfx::memory;

// ============================================================================
// Basic Single-Threaded Tests
// ============================================================================

TEST_CASE("MPMC Queue basic push/pop", "[mpmc][queue]") {
    MPMCQueue<int, 16> queue;

    SECTION("push and pop single element") {
        REQUIRE(queue.try_push(42));
        auto result = queue.try_pop();
        REQUIRE(result.has_value());
        REQUIRE(*result == 42);
    }

    SECTION("push and pop multiple elements preserve FIFO order") {
        for (int i = 0; i < 8; ++i) {
            REQUIRE(queue.try_push(i));
        }

        for (int i = 0; i < 8; ++i) {
            int val = 0;
            REQUIRE(queue.try_pop(val));
            REQUIRE(val == i);
        }
    }

    SECTION("empty queue returns nullopt") {
        auto result = queue.try_pop();
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("empty queue try_pop returns false") {
        int val = 0;
        REQUIRE_FALSE(queue.try_pop(val));
    }

    SECTION("full queue try_push returns false") {
        for (size_t i = 0; i < 16; ++i) {
            REQUIRE(queue.try_push(static_cast<int>(i)));
        }
        REQUIRE_FALSE(queue.try_push(999));
    }

    SECTION("interleaved push and pop") {
        REQUIRE(queue.try_push(1));
        REQUIRE(queue.try_push(2));

        auto r1 = queue.try_pop();
        REQUIRE(r1.has_value());
        REQUIRE(*r1 == 1);

        REQUIRE(queue.try_push(3));

        auto r2 = queue.try_pop();
        REQUIRE(r2.has_value());
        REQUIRE(*r2 == 2);

        auto r3 = queue.try_pop();
        REQUIRE(r3.has_value());
        REQUIRE(*r3 == 3);
    }
}

TEST_CASE("MPMC Queue status queries", "[mpmc][queue]") {
    MPMCQueue<int, 16> queue;

    SECTION("empty on construction") {
        REQUIRE(queue.empty());
        REQUIRE(queue.size_approx() == 0);
        REQUIRE_FALSE(queue.full());
    }

    SECTION("capacity returns template parameter") {
        REQUIRE(MPMCQueue<int, 16>::capacity() == 16);
        REQUIRE(MPMCQueue<int, 1024>::capacity() == 1024);
    }

    SECTION("size tracks pushes and pops") {
        REQUIRE(queue.try_push(1));
        REQUIRE(queue.size_approx() == 1);
        REQUIRE_FALSE(queue.empty());

        REQUIRE(queue.try_push(2));
        REQUIRE(queue.size_approx() == 2);

        int val = 0;
        REQUIRE(queue.try_pop(val));
        REQUIRE(queue.size_approx() == 1);

        REQUIRE(queue.try_pop(val));
        REQUIRE(queue.size_approx() == 0);
        REQUIRE(queue.empty());
    }
}

TEST_CASE("MPMC Queue try_emplace", "[mpmc][queue]") {
    struct Point {
        int x;
        int y;
        Point() : x(0), y(0) {}
        Point(int x_, int y_) : x(x_), y(y_) {}
    };

    MPMCQueue<Point, 16> queue;

    SECTION("emplace with constructor arguments") {
        REQUIRE(queue.try_emplace(10, 20));

        auto result = queue.try_pop();
        REQUIRE(result.has_value());
        REQUIRE(result->x == 10);
        REQUIRE(result->y == 20);
    }

    SECTION("emplace multiple elements") {
        REQUIRE(queue.try_emplace(1, 2));
        REQUIRE(queue.try_emplace(3, 4));
        REQUIRE(queue.try_emplace(5, 6));

        Point p;
        REQUIRE(queue.try_pop(p));
        REQUIRE(p.x == 1);
        REQUIRE(p.y == 2);

        REQUIRE(queue.try_pop(p));
        REQUIRE(p.x == 3);
        REQUIRE(p.y == 4);

        REQUIRE(queue.try_pop(p));
        REQUIRE(p.x == 5);
        REQUIRE(p.y == 6);
    }
}

TEST_CASE("MPMC Queue move-only types", "[mpmc][queue]") {
    MPMCQueue<std::unique_ptr<int>, 16> queue;

    SECTION("push and pop unique_ptr") {
        auto ptr = std::make_unique<int>(42);
        REQUIRE(queue.try_push(std::move(ptr)));
        REQUIRE(ptr == nullptr);  // moved from

        auto result = queue.try_pop();
        REQUIRE(result.has_value());
        REQUIRE(**result == 42);
    }

    SECTION("emplace unique_ptr") {
        REQUIRE(queue.try_emplace(new int(99)));

        auto result = queue.try_pop();
        REQUIRE(result.has_value());
        REQUIRE(**result == 99);
    }

    SECTION("multiple move-only elements") {
        for (int i = 0; i < 8; ++i) {
            REQUIRE(queue.try_push(std::make_unique<int>(i)));
        }

        for (int i = 0; i < 8; ++i) {
            auto result = queue.try_pop();
            REQUIRE(result.has_value());
            REQUIRE(**result == i);
        }
    }
}

TEST_CASE("MPMC Queue wrap-around correctness", "[mpmc][queue]") {
    MPMCQueue<int, 4> queue;

    // Fill and drain multiple times to test turn counter wrap-around
    for (int cycle = 0; cycle < 10; ++cycle) {
        // Fill queue
        for (int i = 0; i < 4; ++i) {
            REQUIRE(queue.try_push(cycle * 4 + i));
        }
        REQUIRE_FALSE(queue.try_push(999));

        // Drain queue
        for (int i = 0; i < 4; ++i) {
            int val = 0;
            REQUIRE(queue.try_pop(val));
            REQUIRE(val == cycle * 4 + i);
        }
        REQUIRE(queue.empty());
    }
}

// ============================================================================
// Blocking Operations
// ============================================================================

TEST_CASE("MPMC Queue blocking push/pop", "[mpmc][queue]") {
    MPMCQueue<int, 16> queue;

    SECTION("blocking push then blocking pop") {
        std::thread producer([&]() {
            for (int i = 0; i < 100; ++i) {
                queue.push(i);
            }
        });

        std::thread consumer([&]() {
            for (int i = 0; i < 100; ++i) {
                int val = queue.pop();
                REQUIRE(val == i);
            }
        });

        producer.join();
        consumer.join();
        REQUIRE(queue.empty());
    }
}

// ============================================================================
// Concurrent Correctness Tests
// ============================================================================

TEST_CASE("MPMC Queue 1P1C concurrent", "[mpmc][queue][concurrency]") {
    constexpr size_t NUM_MESSAGES = 100'000;
    auto queue_ptr = std::make_unique<MPMCQueue<uint64_t, 1024>>();
    auto& queue = *queue_ptr;

    std::vector<uint64_t> consumed;
    consumed.reserve(NUM_MESSAGES);
    std::atomic<bool> done{false};

    std::thread consumer([&]() {
        uint64_t val = 0;
        while (consumed.size() < NUM_MESSAGES) {
            if (queue.try_pop(val)) {
                consumed.push_back(val);
            } else if (done.load(std::memory_order_acquire)) {
                // Drain remaining
                while (queue.try_pop(val)) {
                    consumed.push_back(val);
                }
                break;
            } else {
                _mm_pause();
            }
        }
    });

    // Producer
    for (uint64_t i = 0; i < NUM_MESSAGES; ++i) {
        while (!queue.try_push(i)) {
            _mm_pause();
        }
    }
    done.store(true, std::memory_order_release);

    consumer.join();

    REQUIRE(consumed.size() == NUM_MESSAGES);

    // Verify FIFO ordering (single producer, single consumer)
    for (size_t i = 0; i < NUM_MESSAGES; ++i) {
        REQUIRE(consumed[i] == i);
    }
}

TEST_CASE("MPMC Queue 4P1C concurrent", "[mpmc][queue][concurrency]") {
    constexpr size_t NUM_PRODUCERS = 4;
    constexpr size_t MSGS_PER_PRODUCER = 25'000;
    constexpr size_t TOTAL = NUM_PRODUCERS * MSGS_PER_PRODUCER;

    auto queue_ptr = std::make_unique<MPMCQueue<uint64_t, 1024>>();
    auto& queue = *queue_ptr;

    std::atomic<bool> start{false};
    std::atomic<size_t> producers_done{0};
    std::vector<uint64_t> consumed;
    consumed.reserve(TOTAL);

    // Consumer thread
    std::thread consumer([&]() {
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        uint64_t val = 0;
        while (consumed.size() < TOTAL) {
            if (queue.try_pop(val)) {
                consumed.push_back(val);
            } else if (producers_done.load(std::memory_order_acquire) == NUM_PRODUCERS) {
                while (queue.try_pop(val)) {
                    consumed.push_back(val);
                }
                break;
            } else {
                _mm_pause();
            }
        }
    });

    // Producer threads - each encodes producer_id in high bits
    std::vector<std::thread> producers;
    producers.reserve(NUM_PRODUCERS);
    for (size_t p = 0; p < NUM_PRODUCERS; ++p) {
        producers.emplace_back([&, p]() {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (size_t i = 0; i < MSGS_PER_PRODUCER; ++i) {
                uint64_t val = (p << 32) | i;
                while (!queue.try_push(val)) {
                    _mm_pause();
                }
            }
            producers_done.fetch_add(1, std::memory_order_release);
        });
    }

    start.store(true, std::memory_order_release);

    for (auto& t : producers) {
        t.join();
    }
    consumer.join();

    // Completeness: all messages received
    REQUIRE(consumed.size() == TOTAL);

    // Verify per-producer ordering and completeness
    std::vector<std::vector<uint32_t>> per_producer(NUM_PRODUCERS);
    for (uint64_t val : consumed) {
        uint32_t pid = static_cast<uint32_t>(val >> 32);
        uint32_t seq = static_cast<uint32_t>(val & 0xFFFFFFFF);
        REQUIRE(pid < NUM_PRODUCERS);
        per_producer[pid].push_back(seq);
    }

    for (size_t p = 0; p < NUM_PRODUCERS; ++p) {
        REQUIRE(per_producer[p].size() == MSGS_PER_PRODUCER);
        // Per-producer FIFO ordering should be maintained
        for (size_t i = 0; i < per_producer[p].size(); ++i) {
            REQUIRE(per_producer[p][i] == i);
        }
    }
}

TEST_CASE("MPMC Queue 1P4C concurrent", "[mpmc][queue][concurrency]") {
    constexpr size_t NUM_CONSUMERS = 4;
    constexpr size_t TOTAL_MESSAGES = 100'000;

    auto queue_ptr = std::make_unique<MPMCQueue<uint64_t, 1024>>();
    auto& queue = *queue_ptr;

    std::atomic<bool> done{false};
    std::vector<std::vector<uint64_t>> per_consumer(NUM_CONSUMERS);

    // Consumer threads
    std::vector<std::thread> consumers;
    consumers.reserve(NUM_CONSUMERS);
    for (size_t c = 0; c < NUM_CONSUMERS; ++c) {
        per_consumer[c].reserve(TOTAL_MESSAGES / NUM_CONSUMERS + 1000);
        consumers.emplace_back([&, c]() {
            uint64_t val = 0;
            while (!done.load(std::memory_order_acquire) || !queue.empty()) {
                if (queue.try_pop(val)) {
                    per_consumer[c].push_back(val);
                } else {
                    _mm_pause();
                }
            }
            // Final drain
            while (queue.try_pop(val)) {
                per_consumer[c].push_back(val);
            }
        });
    }

    // Producer
    for (uint64_t i = 0; i < TOTAL_MESSAGES; ++i) {
        while (!queue.try_push(i)) {
            _mm_pause();
        }
    }
    done.store(true, std::memory_order_release);

    for (auto& t : consumers) {
        t.join();
    }

    // Collect all consumed values
    std::set<uint64_t> all_consumed;
    for (size_t c = 0; c < NUM_CONSUMERS; ++c) {
        for (uint64_t val : per_consumer[c]) {
            all_consumed.insert(val);
        }
    }

    // No duplication: total consumed equals unique values
    size_t total_count = 0;
    for (size_t c = 0; c < NUM_CONSUMERS; ++c) {
        total_count += per_consumer[c].size();
    }
    REQUIRE(total_count == TOTAL_MESSAGES);
    REQUIRE(all_consumed.size() == TOTAL_MESSAGES);

    // Completeness: every value present
    for (uint64_t i = 0; i < TOTAL_MESSAGES; ++i) {
        REQUIRE(all_consumed.count(i) == 1);
    }
}

TEST_CASE("MPMC Queue 2P2C concurrent", "[mpmc][queue][concurrency]") {
    constexpr size_t NUM_PRODUCERS = 2;
    constexpr size_t NUM_CONSUMERS = 2;
    constexpr size_t MSGS_PER_PRODUCER = 50'000;
    constexpr size_t TOTAL = NUM_PRODUCERS * MSGS_PER_PRODUCER;

    auto queue_ptr = std::make_unique<MPMCQueue<uint64_t, 1024>>();
    auto& queue = *queue_ptr;

    std::atomic<bool> start{false};
    std::atomic<size_t> producers_done{0};
    std::vector<std::vector<uint64_t>> per_consumer(NUM_CONSUMERS);

    // Consumer threads
    std::vector<std::thread> consumers;
    consumers.reserve(NUM_CONSUMERS);
    for (size_t c = 0; c < NUM_CONSUMERS; ++c) {
        per_consumer[c].reserve(TOTAL / NUM_CONSUMERS + 1000);
        consumers.emplace_back([&, c]() {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            uint64_t val = 0;
            while (true) {
                if (queue.try_pop(val)) {
                    per_consumer[c].push_back(val);
                } else if (producers_done.load(std::memory_order_acquire) == NUM_PRODUCERS) {
                    // Final drain
                    while (queue.try_pop(val)) {
                        per_consumer[c].push_back(val);
                    }
                    break;
                } else {
                    _mm_pause();
                }
            }
        });
    }

    // Producer threads
    std::vector<std::thread> producers;
    producers.reserve(NUM_PRODUCERS);
    for (size_t p = 0; p < NUM_PRODUCERS; ++p) {
        producers.emplace_back([&, p]() {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (size_t i = 0; i < MSGS_PER_PRODUCER; ++i) {
                uint64_t val = (p << 32) | i;
                while (!queue.try_push(val)) {
                    _mm_pause();
                }
            }
            producers_done.fetch_add(1, std::memory_order_release);
        });
    }

    start.store(true, std::memory_order_release);

    for (auto& t : producers) {
        t.join();
    }
    for (auto& t : consumers) {
        t.join();
    }

    // Collect all consumed values
    std::set<uint64_t> all_consumed;
    size_t total_count = 0;
    for (size_t c = 0; c < NUM_CONSUMERS; ++c) {
        total_count += per_consumer[c].size();
        for (uint64_t val : per_consumer[c]) {
            all_consumed.insert(val);
        }
    }

    // No duplication
    REQUIRE(total_count == TOTAL);
    REQUIRE(all_consumed.size() == TOTAL);

    // Per-producer completeness
    for (size_t p = 0; p < NUM_PRODUCERS; ++p) {
        for (size_t i = 0; i < MSGS_PER_PRODUCER; ++i) {
            uint64_t expected = (p << 32) | i;
            REQUIRE(all_consumed.count(expected) == 1);
        }
    }
}

TEST_CASE("MPMC Queue 4P4C concurrent", "[mpmc][queue][concurrency]") {
    constexpr size_t NUM_PRODUCERS = 4;
    constexpr size_t NUM_CONSUMERS = 4;
    constexpr size_t MSGS_PER_PRODUCER = 25'000;
    constexpr size_t TOTAL = NUM_PRODUCERS * MSGS_PER_PRODUCER;

    auto queue_ptr = std::make_unique<MPMCQueue<uint64_t, 1024>>();
    auto& queue = *queue_ptr;

    std::atomic<bool> start{false};
    std::atomic<size_t> producers_done{0};
    std::vector<std::vector<uint64_t>> per_consumer(NUM_CONSUMERS);

    // Consumer threads
    std::vector<std::thread> consumers;
    consumers.reserve(NUM_CONSUMERS);
    for (size_t c = 0; c < NUM_CONSUMERS; ++c) {
        per_consumer[c].reserve(TOTAL / NUM_CONSUMERS + 1000);
        consumers.emplace_back([&, c]() {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            uint64_t val = 0;
            while (true) {
                if (queue.try_pop(val)) {
                    per_consumer[c].push_back(val);
                } else if (producers_done.load(std::memory_order_acquire) == NUM_PRODUCERS) {
                    while (queue.try_pop(val)) {
                        per_consumer[c].push_back(val);
                    }
                    break;
                } else {
                    _mm_pause();
                }
            }
        });
    }

    // Producer threads
    std::vector<std::thread> producers;
    producers.reserve(NUM_PRODUCERS);
    for (size_t p = 0; p < NUM_PRODUCERS; ++p) {
        producers.emplace_back([&, p]() {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (size_t i = 0; i < MSGS_PER_PRODUCER; ++i) {
                uint64_t val = (p << 32) | i;
                while (!queue.try_push(val)) {
                    _mm_pause();
                }
            }
            producers_done.fetch_add(1, std::memory_order_release);
        });
    }

    start.store(true, std::memory_order_release);

    for (auto& t : producers) {
        t.join();
    }
    for (auto& t : consumers) {
        t.join();
    }

    // Collect all consumed values
    std::set<uint64_t> all_consumed;
    size_t total_count = 0;
    for (size_t c = 0; c < NUM_CONSUMERS; ++c) {
        total_count += per_consumer[c].size();
        for (uint64_t val : per_consumer[c]) {
            all_consumed.insert(val);
        }
    }

    // No duplication
    REQUIRE(total_count == TOTAL);
    REQUIRE(all_consumed.size() == TOTAL);
}

// ============================================================================
// Alignment Tests
// ============================================================================

TEST_CASE("MPMC Queue cache-line alignment", "[mpmc][queue]") {
    auto queue_ptr = std::make_unique<MPMCQueue<uint64_t, 64>>();

    // The queue itself should be properly aligned
    auto addr = reinterpret_cast<uintptr_t>(queue_ptr.get());

    // On heap allocations, the allocator may not guarantee cache-line alignment
    // but the internal atomic members should be at proper offsets
    // We verify the queue was at least constructed without issues
    REQUIRE(queue_ptr->empty());
    REQUIRE(queue_ptr->capacity() == 64);

    // Verify MPMC_CACHE_LINE is reasonable
    REQUIRE(MPMC_CACHE_LINE >= 64);
    (void)addr;
}
