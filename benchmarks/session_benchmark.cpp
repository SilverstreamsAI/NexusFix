// session_benchmark.cpp
// NexusFIX Session Throughput Benchmark
// Target: 500K messages/second (vs QuickFIX 50K msg/s)

#include <chrono>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <cmath>

#include "nexusfix/nexusfix.hpp"

namespace nfx::bench {

// ============================================================================
// Throughput Statistics
// ============================================================================

struct ThroughputStats {
    double messages_per_sec;
    double bytes_per_sec;
    double avg_latency_ns;
    size_t total_messages;
    size_t total_bytes;
    double duration_sec;
};

void print_throughput_stats(const char* name, const ThroughputStats& stats) {
    std::cout << "\n=== " << name << " ===\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Duration:      " << stats.duration_sec << " sec\n";
    std::cout << "  Messages:      " << stats.total_messages << "\n";
    std::cout << "  Bytes:         " << stats.total_bytes << "\n";
    std::cout << "  Throughput:    " << std::setw(12)
              << stats.messages_per_sec << " msg/sec\n";
    std::cout << "  Bandwidth:     " << std::setw(12)
              << (stats.bytes_per_sec / 1024 / 1024) << " MB/sec\n";
    std::cout << "  Avg Latency:   " << std::setw(12)
              << stats.avg_latency_ns << " ns/msg\n";
}

// ============================================================================
// Test Message Builder (generates messages with correct checksums)
// ============================================================================

/// Build a valid FIX message with correct checksum
std::string build_fix_message(std::string_view body_without_checksum) {
    std::string msg(body_without_checksum);

    // Calculate checksum (sum of all bytes before checksum field)
    uint32_t sum = 0;
    for (char c : msg) {
        sum += static_cast<uint8_t>(c);
    }
    uint8_t checksum = static_cast<uint8_t>(sum % 256);

    // Format checksum as 3-digit string
    char cs[8];
    cs[0] = '1';
    cs[1] = '0';
    cs[2] = '=';
    cs[3] = static_cast<char>('0' + (checksum / 100));
    cs[4] = static_cast<char>('0' + ((checksum / 10) % 10));
    cs[5] = static_cast<char>('0' + (checksum % 10));
    cs[6] = '\x01';
    cs[7] = '\0';

    msg += cs;
    return msg;
}

// ============================================================================
// Test Message Body (without checksum - will be added at runtime)
// ============================================================================

constexpr std::string_view EXEC_REPORT_BODY =
    "8=FIX.4.4\x01"
    "9=200\x01"
    "35=8\x01"
    "49=SENDER\x01"
    "56=TARGET\x01"
    "34=12345\x01"
    "52=20240115-10:30:00.123\x01"
    "37=ORD123456\x01"
    "17=EXEC789012\x01"
    "150=0\x01"
    "39=0\x01"
    "54=1\x01"
    "151=1000\x01"
    "14=0\x01"
    "6=0\x01"
    "55=AAPL\x01"
    "38=1000\x01"
    "44=150.50\x01";

// ============================================================================
// Benchmark: Parse Throughput
// ============================================================================

ThroughputStats benchmark_parse_throughput(size_t num_messages) {
    std::string msg = build_fix_message(EXEC_REPORT_BODY);
    std::span<const char> data{msg.data(), msg.size()};

    // Warmup
    for (size_t i = 0; i < 10000; ++i) {
        auto result = IndexedParser::parse(data);
        (void)result;
    }

    auto start = std::chrono::steady_clock::now();

    size_t successful = 0;
    for (size_t i = 0; i < num_messages; ++i) {
        auto result = IndexedParser::parse(data);
        if (result) ++successful;
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration<double>(end - start);

    ThroughputStats stats{};
    stats.total_messages = successful;
    stats.total_bytes = successful * msg.size();
    stats.duration_sec = duration.count();
    stats.messages_per_sec = successful / duration.count();
    stats.bytes_per_sec = stats.total_bytes / duration.count();
    stats.avg_latency_ns = (duration.count() * 1e9) / successful;

    return stats;
}

// ============================================================================
// Benchmark: Sequence Number Operations
// ============================================================================

ThroughputStats benchmark_sequence_ops(size_t num_ops) {
    SequenceManager seq_mgr;

    // Warmup
    for (size_t i = 0; i < 10000; ++i) {
        [[maybe_unused]] uint32_t seq = seq_mgr.next_outbound();
    }
    seq_mgr.reset();

    auto start = std::chrono::steady_clock::now();

    for (size_t i = 0; i < num_ops; ++i) {
        [[maybe_unused]] uint32_t seq = seq_mgr.next_outbound();
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration<double>(end - start);

    ThroughputStats stats{};
    stats.total_messages = num_ops;
    stats.total_bytes = num_ops * sizeof(uint32_t);
    stats.duration_sec = duration.count();
    stats.messages_per_sec = num_ops / duration.count();
    stats.bytes_per_sec = stats.total_bytes / duration.count();
    stats.avg_latency_ns = (duration.count() * 1e9) / num_ops;

    return stats;
}

// ============================================================================
// Benchmark: Message Building
// ============================================================================

ThroughputStats benchmark_message_building(size_t num_messages) {
    MessageAssembler assembler;

    // Warmup
    for (size_t i = 0; i < 10000; ++i) {
        assembler.start()
            .field(tag::MsgType::value, "0")
            .field(tag::SenderCompID::value, "SENDER")
            .field(tag::TargetCompID::value, "TARGET")
            .field(tag::MsgSeqNum::value, static_cast<int64_t>(i))
            .field(tag::SendingTime::value, "20240115-10:30:00.000");
        auto msg = assembler.finish();
        (void)msg;
    }

    auto start = std::chrono::steady_clock::now();

    size_t total_bytes = 0;
    for (size_t i = 0; i < num_messages; ++i) {
        assembler.start()
            .field(tag::MsgType::value, "0")
            .field(tag::SenderCompID::value, "SENDER")
            .field(tag::TargetCompID::value, "TARGET")
            .field(tag::MsgSeqNum::value, static_cast<int64_t>(i))
            .field(tag::SendingTime::value, "20240115-10:30:00.000");
        auto msg = assembler.finish();
        total_bytes += msg.size();
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration<double>(end - start);

    ThroughputStats stats{};
    stats.total_messages = num_messages;
    stats.total_bytes = total_bytes;
    stats.duration_sec = duration.count();
    stats.messages_per_sec = num_messages / duration.count();
    stats.bytes_per_sec = total_bytes / duration.count();
    stats.avg_latency_ns = (duration.count() * 1e9) / num_messages;

    return stats;
}

// ============================================================================
// Benchmark: Memory Pool Allocation
// ============================================================================

ThroughputStats benchmark_pool_allocation(size_t num_allocs) {
    FixedPool<256, 10000> pool;

    // Warmup
    std::vector<void*> ptrs;
    ptrs.reserve(1000);
    for (size_t i = 0; i < 1000; ++i) {
        ptrs.push_back(pool.allocate());
    }
    for (auto* p : ptrs) {
        pool.deallocate(p);
    }
    ptrs.clear();

    auto start = std::chrono::steady_clock::now();

    size_t successful = 0;
    for (size_t i = 0; i < num_allocs; ++i) {
        void* p = pool.allocate();
        if (p) {
            ++successful;
            pool.deallocate(p);
        }
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration<double>(end - start);

    ThroughputStats stats{};
    stats.total_messages = successful;
    stats.total_bytes = successful * 256;
    stats.duration_sec = duration.count();
    stats.messages_per_sec = successful / duration.count();
    stats.bytes_per_sec = stats.total_bytes / duration.count();
    stats.avg_latency_ns = (duration.count() * 1e9) / successful;

    return stats;
}

// ============================================================================
// Benchmark: Ring Buffer Operations
// ============================================================================

ThroughputStats benchmark_ring_buffer(size_t num_ops) {
    RingBuffer<65536> buffer;
    std::string msg = build_fix_message(EXEC_REPORT_BODY);

    // Warmup
    for (size_t i = 0; i < 1000; ++i) {
        [[maybe_unused]] size_t w = buffer.write(std::span<const char>{msg.data(), msg.size()});
        std::array<char, 256> read_buf;
        [[maybe_unused]] size_t r = buffer.read(std::span<char>{read_buf.data(), msg.size()});
    }

    auto start = std::chrono::steady_clock::now();

    size_t total_bytes = 0;
    for (size_t i = 0; i < num_ops; ++i) {
        size_t written = buffer.write(std::span<const char>{msg.data(), msg.size()});
        std::array<char, 256> read_buf;
        size_t readv = buffer.read(std::span<char>{read_buf.data(), msg.size()});
        total_bytes += written + readv;
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration<double>(end - start);

    ThroughputStats stats{};
    stats.total_messages = num_ops * 2;  // write + read
    stats.total_bytes = total_bytes;
    stats.duration_sec = duration.count();
    stats.messages_per_sec = (num_ops * 2) / duration.count();
    stats.bytes_per_sec = total_bytes / duration.count();
    stats.avg_latency_ns = (duration.count() * 1e9) / (num_ops * 2);

    return stats;
}

// ============================================================================
// Benchmark: Gap Tracker
// ============================================================================

ThroughputStats benchmark_gap_tracker(size_t num_ops) {
    GapTracker tracker;

    auto start = std::chrono::steady_clock::now();

    size_t operations = 0;
    for (size_t i = 0; i < num_ops / 10; ++i) {
        if (tracker.gap_count() < GapTracker::MAX_GAPS - 1) {
            tracker.add_gap(i * 10, i * 10 + 5);
            ++operations;
        }
        for (uint32_t j = i * 10; j <= i * 10 + 5; ++j) {
            tracker.fill(j);
            ++operations;
        }
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration<double>(end - start);

    ThroughputStats stats{};
    stats.total_messages = operations;
    stats.total_bytes = operations * sizeof(uint32_t);
    stats.duration_sec = duration.count();
    stats.messages_per_sec = operations / duration.count();
    stats.bytes_per_sec = stats.total_bytes / duration.count();
    stats.avg_latency_ns = (duration.count() * 1e9) / operations;

    return stats;
}

// ============================================================================
// Benchmark: Message Store
// ============================================================================

ThroughputStats benchmark_message_store(size_t num_ops) {
    MemoryMessageStore<10000, 512> store;
    std::string msg = build_fix_message(EXEC_REPORT_BODY);

    // Warmup
    for (size_t i = 0; i < 1000; ++i) {
        store.store(i, std::span<const char>{msg.data(), msg.size()});
    }
    store.clear();

    auto start = std::chrono::steady_clock::now();

    // Store messages
    for (size_t i = 0; i < num_ops; ++i) {
        store.store(i, std::span<const char>{msg.data(), msg.size()});
    }

    // Retrieve some messages
    size_t retrieved = 0;
    for (size_t i = 0; i < num_ops; i += 10) {
        auto result = store.retrieve(i);
        if (result) ++retrieved;
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration<double>(end - start);

    ThroughputStats stats{};
    stats.total_messages = num_ops + retrieved;
    stats.total_bytes = num_ops * msg.size();
    stats.duration_sec = duration.count();
    stats.messages_per_sec = num_ops / duration.count();
    stats.bytes_per_sec = stats.total_bytes / duration.count();
    stats.avg_latency_ns = (duration.count() * 1e9) / num_ops;

    return stats;
}

// ============================================================================
// Benchmark: Full Session Simulation
// ============================================================================

ThroughputStats benchmark_session_simulation(size_t num_messages) {
    // Simulate receiving and processing messages
    std::string msg = build_fix_message(EXEC_REPORT_BODY);
    std::span<const char> data{msg.data(), msg.size()};

    SequenceManager seq_mgr;

    // Warmup
    for (size_t i = 0; i < 10000; ++i) {
        auto result = IndexedParser::parse(data);
        if (result) {
            uint32_t seq = result->msg_seq_num();
            [[maybe_unused]] auto v = seq_mgr.validate_inbound(seq);
        }
    }
    seq_mgr.reset();

    auto start = std::chrono::steady_clock::now();

    size_t processed = 0;
    for (size_t i = 0; i < num_messages; ++i) {
        auto result = IndexedParser::parse(data);
        if (result) {
            // Validate sequence
            auto validation = seq_mgr.validate_inbound(i + 1);
            if (validation == SequenceManager::SequenceResult::Ok) {
                // Get message type
                char msg_type = result->msg_type();
                if (msg_type != '\0') {
                    ++processed;
                }
            }
        }
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration<double>(end - start);

    ThroughputStats stats{};
    stats.total_messages = processed;
    stats.total_bytes = processed * msg.size();
    stats.duration_sec = duration.count();
    stats.messages_per_sec = processed / duration.count();
    stats.bytes_per_sec = stats.total_bytes / duration.count();
    stats.avg_latency_ns = (duration.count() * 1e9) / processed;

    return stats;
}

} // namespace nfx::bench

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    using namespace nfx::bench;

    size_t num_messages = 1000000;

    if (argc > 1) {
        num_messages = std::stoul(argv[1]);
    }

    std::cout << "NexusFIX Session Throughput Benchmark\n";
    std::cout << "=====================================\n";
    std::cout << "Operations per test: " << num_messages << "\n";

    // Run throughput benchmarks
    {
        auto stats = benchmark_parse_throughput(num_messages);
        print_throughput_stats("Parse Throughput (ExecutionReport)", stats);
    }

    {
        auto stats = benchmark_sequence_ops(num_messages);
        print_throughput_stats("Sequence Number Operations", stats);
    }

    {
        auto stats = benchmark_message_building(num_messages);
        print_throughput_stats("Message Building (Heartbeat)", stats);
    }

    {
        auto stats = benchmark_pool_allocation(num_messages);
        print_throughput_stats("Pool Allocation (256 bytes)", stats);
    }

    {
        auto stats = benchmark_ring_buffer(num_messages);
        print_throughput_stats("Ring Buffer (write+read)", stats);
    }

    {
        auto stats = benchmark_gap_tracker(num_messages);
        print_throughput_stats("Gap Tracker Operations", stats);
    }

    {
        auto stats = benchmark_message_store(num_messages);
        print_throughput_stats("Message Store (store+retrieve)", stats);
    }

    {
        auto stats = benchmark_session_simulation(num_messages);
        print_throughput_stats("Full Session Simulation", stats);
    }

    std::cout << "\n";
    std::cout << "============================================\n";
    std::cout << "Target: Session throughput > 500K msg/sec\n";
    std::cout << "============================================\n";

    return 0;
}
