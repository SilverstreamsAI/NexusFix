#include <catch2/catch_test_macros.hpp>

#include "nexusfix/memory/mimalloc_resource.hpp"
#include "nexusfix/memory/buffer_pool.hpp"
#include "nexusfix/store/memory_message_store.hpp"

#include <vector>
#include <string>
#include <memory_resource>
#include <cstring>

using namespace nfx::memory;
using namespace nfx;

// ============================================================================
// Basic Allocation Tests
// ============================================================================

TEST_CASE("MimallocMemoryResource basic allocation", "[mimalloc][resource]") {
    MimallocMemoryResource resource;
    REQUIRE(resource.valid());

    SECTION("Single allocation and deallocation") {
        auto alloc = resource.allocator();
        char* ptr = alloc.allocate(256);
        REQUIRE(ptr != nullptr);

        // Write to verify usability
        std::memset(ptr, 0xAB, 256);
        REQUIRE(static_cast<unsigned char>(ptr[0]) == 0xAB);
        REQUIRE(static_cast<unsigned char>(ptr[255]) == 0xAB);

        alloc.deallocate(ptr, 256);
    }

    SECTION("Multiple allocations") {
        auto alloc = resource.allocator();
        constexpr size_t NUM_ALLOCS = 100;
        std::array<char*, NUM_ALLOCS> ptrs{};

        for (size_t i = 0; i < NUM_ALLOCS; ++i) {
            ptrs[i] = alloc.allocate(64);
            REQUIRE(ptrs[i] != nullptr);
        }

        // All pointers should be distinct
        for (size_t i = 0; i < NUM_ALLOCS; ++i) {
            for (size_t j = i + 1; j < NUM_ALLOCS; ++j) {
                REQUIRE(ptrs[i] != ptrs[j]);
            }
        }

        for (size_t i = 0; i < NUM_ALLOCS; ++i) {
            alloc.deallocate(ptrs[i], 64);
        }
    }

    SECTION("Various sizes") {
        auto alloc = resource.allocator();

        char* small = alloc.allocate(1);
        char* medium = alloc.allocate(1024);
        char* large = alloc.allocate(65536);

        REQUIRE(small != nullptr);
        REQUIRE(medium != nullptr);
        REQUIRE(large != nullptr);

        alloc.deallocate(large, 65536);
        alloc.deallocate(medium, 1024);
        alloc.deallocate(small, 1);
    }
}

// ============================================================================
// Alignment Tests
// ============================================================================

TEST_CASE("MimallocMemoryResource alignment", "[mimalloc][resource]") {
    MimallocMemoryResource resource;

    SECTION("Cache-line aligned allocations") {
        constexpr size_t ALIGNMENT = CACHE_LINE_SIZE;

        // Allocate through the PMR interface with alignment
        void* ptr = resource.allocate(256, ALIGNMENT);
        REQUIRE(ptr != nullptr);

        auto addr = reinterpret_cast<uintptr_t>(ptr);
        REQUIRE((addr % ALIGNMENT) == 0);

        resource.deallocate(ptr, 256, ALIGNMENT);
    }

    SECTION("Various alignments") {
        for (size_t align : {8, 16, 32, 64, 128, 256}) {
            void* ptr = resource.allocate(128, align);
            REQUIRE(ptr != nullptr);

            auto addr = reinterpret_cast<uintptr_t>(ptr);
            REQUIRE((addr % align) == 0);

            resource.deallocate(ptr, 128, align);
        }
    }
}

// ============================================================================
// Per-Heap Isolation Tests
// ============================================================================

TEST_CASE("MimallocMemoryResource heap isolation", "[mimalloc][heap]") {
    SECTION("Allocations from different heaps are independent") {
        MimallocMemoryResource heap_a;
        MimallocMemoryResource heap_b;

        auto alloc_a = heap_a.allocator();
        auto alloc_b = heap_b.allocator();

        char* ptr_a = alloc_a.allocate(256);
        char* ptr_b = alloc_b.allocate(256);

        // Write distinct patterns
        std::memset(ptr_a, 0xAA, 256);
        std::memset(ptr_b, 0xBB, 256);

        // Verify patterns are independent
        REQUIRE(static_cast<unsigned char>(ptr_a[0]) == 0xAA);
        REQUIRE(static_cast<unsigned char>(ptr_b[0]) == 0xBB);

        // Stats are independent
        REQUIRE(heap_a.bytes_allocated() == 256);
        REQUIRE(heap_b.bytes_allocated() == 256);

        alloc_a.deallocate(ptr_a, 256);
        alloc_b.deallocate(ptr_b, 256);
    }
}

TEST_CASE("MimallocMemoryResource heap destruction", "[mimalloc][heap]") {
    SECTION("Heap destruction releases all memory") {
        auto* resource = new MimallocMemoryResource();
        auto alloc = resource->allocator();

        // Allocate many blocks without individual deallocation
        for (int i = 0; i < 1000; ++i) {
            char* ptr = alloc.allocate(128);
            std::memset(ptr, 0, 128);
            (void)ptr;  // Intentionally not deallocating
        }

        REQUIRE(resource->allocation_count() == 1000);
        REQUIRE(resource->bytes_allocated() == 128000);

        // Destroying the resource should free everything via mi_heap_destroy
        delete resource;
        // If we reach here without crash/leak, heap destruction worked
        REQUIRE(true);
    }
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_CASE("MimallocMemoryResource statistics", "[mimalloc][stats]") {
    MimallocMemoryResource resource;

    SECTION("Initial stats are zero") {
        auto s = resource.stats();
        REQUIRE(s.bytes_allocated == 0);
        REQUIRE(s.allocation_count == 0);
        REQUIRE(s.peak_bytes == 0);
    }

    SECTION("Stats track allocations") {
        auto alloc = resource.allocator();

        char* p1 = alloc.allocate(100);
        REQUIRE(resource.bytes_allocated() == 100);
        REQUIRE(resource.allocation_count() == 1);

        char* p2 = alloc.allocate(200);
        REQUIRE(resource.bytes_allocated() == 300);
        REQUIRE(resource.allocation_count() == 2);

        alloc.deallocate(p1, 100);
        REQUIRE(resource.bytes_allocated() == 200);
        REQUIRE(resource.allocation_count() == 1);

        alloc.deallocate(p2, 200);
        REQUIRE(resource.bytes_allocated() == 0);
        REQUIRE(resource.allocation_count() == 0);
    }

    SECTION("Peak bytes tracks high water mark") {
        auto alloc = resource.allocator();

        char* p1 = alloc.allocate(500);
        char* p2 = alloc.allocate(500);
        REQUIRE(resource.peak_bytes() == 1000);

        alloc.deallocate(p1, 500);
        REQUIRE(resource.peak_bytes() == 1000);  // Peak unchanged

        alloc.deallocate(p2, 500);
        REQUIRE(resource.peak_bytes() == 1000);  // Peak unchanged
        REQUIRE(resource.bytes_allocated() == 0);
    }
}

// ============================================================================
// PMR Container Compatibility Tests
// ============================================================================

TEST_CASE("MimallocMemoryResource PMR compatibility", "[mimalloc][pmr]") {
    MimallocMemoryResource resource;

    SECTION("std::pmr::vector") {
        std::pmr::vector<int> vec(&resource);
        vec.reserve(100);

        for (int i = 0; i < 100; ++i) {
            vec.push_back(i);
        }

        REQUIRE(vec.size() == 100);
        REQUIRE(vec[0] == 0);
        REQUIRE(vec[99] == 99);
        REQUIRE(resource.allocation_count() > 0);
    }

    SECTION("std::pmr::string") {
        std::pmr::string str(&resource);
        str = "8=FIX.4.4\x01" "9=150\x01" "35=8\x01" "49=SENDER\x01" "56=TARGET\x01";

        REQUIRE(str.size() > 0);
        REQUIRE(str.find("FIX.4.4") != std::pmr::string::npos);
    }

    SECTION("Nested PMR containers") {
        std::pmr::polymorphic_allocator<char> alloc(&resource);
        std::pmr::vector<std::pmr::string> messages(&resource);

        for (int i = 0; i < 10; ++i) {
            messages.push_back(std::pmr::string("8=FIX.4.4\x01" "35=D\x01", alloc));
        }

        REQUIRE(messages.size() == 10);
        for (const auto& msg : messages) {
            REQUIRE(msg.find("FIX.4.4") != std::pmr::string::npos);
        }
    }
}

// ============================================================================
// is_equal Tests
// ============================================================================

TEST_CASE("MimallocMemoryResource is_equal", "[mimalloc][resource]") {
    MimallocMemoryResource resource_a;
    MimallocMemoryResource resource_b;

    REQUIRE(resource_a.is_equal(resource_a));
    REQUIRE(!resource_a.is_equal(resource_b));
}

// ============================================================================
// SessionHeap Tests
// ============================================================================

TEST_CASE("SessionHeap basic allocation", "[mimalloc][session]") {
    SessionHeap session(1024 * 1024);  // 1MB for tests
    REQUIRE(session.valid());
    REQUIRE(session.initial_buffer_size() == 1024 * 1024);

    SECTION("Single allocation via bump path") {
        auto alloc = session.allocator();
        char* ptr = alloc.allocate(256);
        REQUIRE(ptr != nullptr);

        // Write to verify usability
        std::memset(ptr, 0xCD, 256);
        REQUIRE(static_cast<unsigned char>(ptr[0]) == 0xCD);
        REQUIRE(static_cast<unsigned char>(ptr[255]) == 0xCD);
    }

    SECTION("Multiple allocations within initial buffer") {
        auto alloc = session.allocator();
        constexpr size_t NUM_ALLOCS = 100;
        std::array<char*, NUM_ALLOCS> ptrs{};

        for (size_t i = 0; i < NUM_ALLOCS; ++i) {
            ptrs[i] = alloc.allocate(64);
            REQUIRE(ptrs[i] != nullptr);
        }

        // All pointers should be distinct
        for (size_t i = 0; i < NUM_ALLOCS; ++i) {
            for (size_t j = i + 1; j < NUM_ALLOCS; ++j) {
                REQUIRE(ptrs[i] != ptrs[j]);
            }
        }
    }
}

TEST_CASE("SessionHeap overflow to mimalloc", "[mimalloc][session]") {
    // Small initial buffer to force overflow
    constexpr size_t SMALL_BUFFER = 1024;  // 1KB
    SessionHeap session(SMALL_BUFFER);
    REQUIRE(session.valid());

    auto alloc = session.allocator();

    // Allocate more than the initial buffer
    constexpr size_t ALLOC_SIZE = 256;
    constexpr size_t NUM_ALLOCS = 10;  // 2560 bytes > 1024 buffer
    std::array<char*, NUM_ALLOCS> ptrs{};

    for (size_t i = 0; i < NUM_ALLOCS; ++i) {
        ptrs[i] = alloc.allocate(ALLOC_SIZE);
        REQUIRE(ptrs[i] != nullptr);
        std::memset(ptrs[i], static_cast<int>(i), ALLOC_SIZE);
    }

    // Verify all data is intact (overflow didn't corrupt earlier allocations)
    for (size_t i = 0; i < NUM_ALLOCS; ++i) {
        REQUIRE(static_cast<unsigned char>(ptrs[i][0]) == static_cast<unsigned char>(i));
    }

    // The mimalloc heap should have been used for overflow
    auto heap_stats = session.stats();
    REQUIRE(heap_stats.bytes_allocated > SMALL_BUFFER);
}

TEST_CASE("SessionHeap reset", "[mimalloc][session]") {
    SessionHeap session(1024 * 1024);
    auto alloc = session.allocator();

    // First round of allocations
    for (int i = 0; i < 100; ++i) {
        char* ptr = alloc.allocate(128);
        REQUIRE(ptr != nullptr);
        std::memset(ptr, 0, 128);
    }

    // Reset - should reuse the initial buffer
    session.reset();

    // Second round should succeed (reusing initial buffer)
    for (int i = 0; i < 100; ++i) {
        char* ptr = alloc.allocate(128);
        REQUIRE(ptr != nullptr);
        std::memset(ptr, 0xFF, 128);
    }
}

TEST_CASE("SessionHeap with std::pmr::vector", "[mimalloc][session]") {
    SessionHeap session(1024 * 1024);

    std::pmr::vector<int> vec(&session);
    vec.reserve(1000);

    for (int i = 0; i < 1000; ++i) {
        vec.push_back(i);
    }

    REQUIRE(vec.size() == 1000);
    REQUIRE(vec[0] == 0);
    REQUIRE(vec[999] == 999);
}

TEST_CASE("SessionHeap destruction releases all memory", "[mimalloc][session]") {
    // Allocate heavily, then destroy - should not leak
    {
        SessionHeap session(1024 * 1024);
        auto alloc = session.allocator();

        for (int i = 0; i < 10000; ++i) {
            char* ptr = alloc.allocate(64);
            std::memset(ptr, 0, 64);
            (void)ptr;  // Intentionally not deallocating
        }
        // session destructor runs here:
        // 1. pool_ destroyed (releases overflow chunks to heap_)
        // 2. heap_ destroyed (mi_heap_destroy releases everything)
    }
    // If we reach here without crash/ASAN error, destruction worked
    REQUIRE(true);
}

TEST_CASE("SessionHeap as MemoryMessageStore upstream", "[mimalloc][session]") {
    SessionHeap session(4 * 1024 * 1024);  // 4MB

    nfx::store::MemoryMessageStore::Config config{
        .session_id = "TEST-SESSION",
        .max_messages = 1000,
        .max_bytes = 2 * 1024 * 1024,
        .pool_size_bytes = 1024 * 1024,
        .upstream_resource = &session
    };

    nfx::store::MemoryMessageStore store(config);

    // Store messages
    constexpr char MSG[] = "8=FIX.4.4\x01" "35=8\x01" "49=SENDER\x01";
    std::span<const char> msg_span(MSG, sizeof(MSG) - 1);

    for (uint32_t i = 1; i <= 100; ++i) {
        REQUIRE(store.store(i, msg_span));
    }

    REQUIRE(store.message_count() == 100);

    // Retrieve and verify
    auto retrieved = store.retrieve(1);
    REQUIRE(retrieved.has_value());
    REQUIRE(retrieved->size() == msg_span.size());

    // Reset store
    store.reset();
    REQUIRE(store.message_count() == 0);
}
