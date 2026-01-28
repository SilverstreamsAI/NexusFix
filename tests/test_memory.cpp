#include <catch2/catch_test_macros.hpp>

#include "nexusfix/memory/buffer_pool.hpp"

using namespace nfx;

// ============================================================================
// AlignedBuffer Tests
// ============================================================================

TEST_CASE("AlignedBuffer", "[memory][aligned]") {
    SECTION("Small buffer alignment") {
        SmallBuffer buf;
        auto ptr = reinterpret_cast<uintptr_t>(buf.data.data());
        REQUIRE((ptr % CACHE_LINE_SIZE) == 0);
        REQUIRE(buf.size() == 256);
    }

    SECTION("Medium buffer") {
        MediumBuffer buf;
        REQUIRE(buf.size() == 1024);

        auto span = buf.as_span();
        REQUIRE(span.size() == 1024);
    }

    SECTION("Large buffer") {
        LargeBuffer buf;
        REQUIRE(buf.size() == 4096);
    }
}

// ============================================================================
// FixedPool Tests
// ============================================================================

TEST_CASE("FixedPool allocation", "[memory][pool]") {
    FixedPool<64, 16> pool;

    SECTION("Initial state") {
        REQUIRE(pool.allocated() == 0);
        REQUIRE(pool.available() == 16);
        REQUIRE(pool.block_size() == 64);
        REQUIRE(pool.capacity() == 16);
    }

    SECTION("Single allocation") {
        void* ptr = pool.allocate();
        REQUIRE(ptr != nullptr);
        REQUIRE(pool.allocated() == 1);
        REQUIRE(pool.available() == 15);
        REQUIRE(pool.owns(ptr));
    }

    SECTION("Allocate and deallocate") {
        void* ptr = pool.allocate();
        REQUIRE(pool.allocated() == 1);

        pool.deallocate(ptr);
        REQUIRE(pool.allocated() == 0);
        REQUIRE(pool.available() == 16);
    }

    SECTION("Multiple allocations") {
        std::array<void*, 16> ptrs;

        for (size_t i = 0; i < 16; ++i) {
            ptrs[i] = pool.allocate();
            REQUIRE(ptrs[i] != nullptr);
        }

        REQUIRE(pool.allocated() == 16);
        REQUIRE(pool.available() == 0);

        // Pool exhausted
        void* extra = pool.allocate();
        REQUIRE(extra == nullptr);

        // Deallocate all
        for (auto ptr : ptrs) {
            pool.deallocate(ptr);
        }

        REQUIRE(pool.allocated() == 0);
        REQUIRE(pool.available() == 16);
    }

    SECTION("Ownership check") {
        void* ptr = pool.allocate();
        REQUIRE(pool.owns(ptr));

        char external[64];
        REQUIRE(!pool.owns(external));
    }
}

// ============================================================================
// MessagePool Tests
// ============================================================================

TEST_CASE("MessagePool tiered allocation", "[memory][pool]") {
    MessagePool pool;

    SECTION("Small allocation") {
        auto buf = pool.allocate(100);
        REQUIRE(!buf.empty());
        REQUIRE(buf.size() == MessagePool::SMALL_SIZE);

        pool.deallocate(buf);
    }

    SECTION("Medium allocation") {
        auto buf = pool.allocate(500);
        REQUIRE(!buf.empty());
        REQUIRE(buf.size() == MessagePool::MEDIUM_SIZE);

        pool.deallocate(buf);
    }

    SECTION("Large allocation") {
        auto buf = pool.allocate(2000);
        REQUIRE(!buf.empty());
        REQUIRE(buf.size() == MessagePool::LARGE_SIZE);

        pool.deallocate(buf);
    }

    SECTION("Oversized allocation fails") {
        auto buf = pool.allocate(10000);
        REQUIRE(buf.empty());
    }

    SECTION("Stats") {
        auto buf1 = pool.allocate(100);
        auto buf2 = pool.allocate(500);

        auto stats = pool.stats();
        REQUIRE(stats.small_allocated == 1);
        REQUIRE(stats.medium_allocated == 1);
        REQUIRE(stats.large_allocated == 0);

        pool.deallocate(buf1);
        pool.deallocate(buf2);

        stats = pool.stats();
        REQUIRE(stats.small_allocated == 0);
        REQUIRE(stats.medium_allocated == 0);
    }
}

// ============================================================================
// PooledBuffer Tests
// ============================================================================

TEST_CASE("PooledBuffer RAII", "[memory][pool]") {
    MessagePool pool;

    SECTION("Auto deallocation") {
        {
            auto buf = pool.allocate(100);
            PooledBuffer pb{pool, buf};

            REQUIRE(!pb.empty());
            REQUIRE(pb.size() == MessagePool::SMALL_SIZE);

            auto stats = pool.stats();
            REQUIRE(stats.small_allocated == 1);
        }

        // Buffer should be returned after scope exit
        auto stats = pool.stats();
        REQUIRE(stats.small_allocated == 0);
    }

    SECTION("Move semantics") {
        PooledBuffer pb1;
        {
            auto buf = pool.allocate(100);
            PooledBuffer pb2{pool, buf};
            pb1 = std::move(pb2);

            REQUIRE(pb2.empty());
            REQUIRE(!pb1.empty());
        }

        // Buffer still held by pb1
        auto stats = pool.stats();
        REQUIRE(stats.small_allocated == 1);
    }

    SECTION("Move constructor") {
        auto buf = pool.allocate(100);
        PooledBuffer pb1{pool, buf};
        PooledBuffer pb2{std::move(pb1)};

        REQUIRE(pb1.empty());
        REQUIRE(!pb2.empty());

        auto stats = pool.stats();
        REQUIRE(stats.small_allocated == 1);
    }

    SECTION("Bool conversion") {
        PooledBuffer empty;
        REQUIRE(!empty);

        auto buf = pool.allocate(100);
        PooledBuffer valid{pool, buf};
        REQUIRE(valid);

        pool.deallocate(buf);
    }
}

// ============================================================================
// MonotonicPool Tests
// ============================================================================

TEST_CASE("MonotonicPool", "[memory][pmr]") {
    MonotonicPool<4096> pool;

    SECTION("Allocation") {
        auto alloc = pool.allocator();
        char* ptr = alloc.allocate(100);
        REQUIRE(ptr != nullptr);

        // Write to allocated memory
        std::memset(ptr, 0, 100);
    }

    SECTION("Reset") {
        auto alloc = pool.allocator();

        // Allocate some memory
        (void)alloc.allocate(1000);
        (void)alloc.allocate(1000);

        // Reset should allow reuse
        pool.reset();

        // Should be able to allocate again
        char* ptr = alloc.allocate(2000);
        REQUIRE(ptr != nullptr);
    }
}

// ============================================================================
// Cache Line Tests
// ============================================================================

TEST_CASE("Cache line alignment", "[memory][cache]") {
    REQUIRE(CACHE_LINE_SIZE >= 64);

    // Check that our pools are cache-line aligned
    FixedPool<64, 4> pool;
    void* ptr = pool.allocate();
    auto addr = reinterpret_cast<uintptr_t>(ptr);

    // First allocation should be aligned
    REQUIRE((addr % CACHE_LINE_SIZE) == 0);
}
