/*
    TICKET_204: Aeron IPC Benchmark

    Compares:
    - Aeron IPC (shared memory) vs MPSC Queue (baseline)
    - Aeron UDP (loopback) vs baseline

    Test scenarios:
    - One-way latency (publication -> subscription)
    - Round-trip latency (ping-pong)
    - Message sizes: 64B, 256B, 1024B

    REQUIRES: External Aeron Media Driver running
    Start with: java -cp aeron-all.jar io.aeron.driver.MediaDriver

    Alternatively, install aeron-driver package and run: aeronmd
*/

#include <nexusfix/memory/spsc_queue.hpp>
#include <nexusfix/memory/mpsc_queue.hpp>
#include <nexusfix/memory/wait_strategy.hpp>

#include <Aeron.h>
#include <Context.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>

using namespace std::chrono;
using namespace nfx::memory;

// ============================================================================
// Configuration
// ============================================================================

constexpr size_t WARMUP_ITERATIONS = 1'000;
constexpr size_t BENCHMARK_ITERATIONS = 100'000;
constexpr size_t QUEUE_CAPACITY = 4096;

// Aeron channels
const std::string IPC_CHANNEL = "aeron:ipc";
const std::string UDP_CHANNEL = "aeron:udp?endpoint=127.0.0.1:40123";
constexpr int32_t STREAM_ID = 1001;
constexpr int32_t PING_STREAM_ID = 1002;
constexpr int32_t PONG_STREAM_ID = 1003;

// Message sizes to test
constexpr std::array<size_t, 3> MESSAGE_SIZES = {64, 256, 1024};

// ============================================================================
// Test Messages
// ============================================================================

struct alignas(64) TestMessage64 {
    uint64_t sequence;
    uint64_t timestamp;
    uint64_t producer_id;
    uint64_t payload[5];
};
static_assert(sizeof(TestMessage64) == 64);

struct alignas(64) TestMessage256 {
    uint64_t sequence;
    uint64_t timestamp;
    uint64_t producer_id;
    uint64_t payload[29];
};
static_assert(sizeof(TestMessage256) == 256);

struct alignas(64) TestMessage1024 {
    uint64_t sequence;
    uint64_t timestamp;
    uint64_t producer_id;
    uint64_t payload[125];
};
static_assert(sizeof(TestMessage1024) == 1024);

// ============================================================================
// Timing Utilities
// ============================================================================

inline uint64_t rdtsc() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
    uint64_t lo, hi;
    asm volatile("lfence; rdtsc; lfence" : "=a"(lo), "=d"(hi));
    return (hi << 32) | lo;
#else
    return static_cast<uint64_t>(
        steady_clock::now().time_since_epoch().count());
#endif
}

double get_cpu_freq_ghz() {
    constexpr int calibration_ms = 100;
    auto start_time = steady_clock::now();
    uint64_t start_tsc = rdtsc();

    std::this_thread::sleep_for(milliseconds(calibration_ms));

    uint64_t end_tsc = rdtsc();
    auto end_time = steady_clock::now();

    auto elapsed_ns = duration_cast<nanoseconds>(end_time - start_time).count();
    return static_cast<double>(end_tsc - start_tsc) / static_cast<double>(elapsed_ns);
}

// ============================================================================
// Results
// ============================================================================

struct LatencyResult {
    double min_ns;
    double p50_ns;
    double p90_ns;
    double p99_ns;
    double p999_ns;
    double max_ns;
    double throughput_mps;
    size_t message_size;
    std::string transport;
};

LatencyResult compute_stats(std::vector<uint64_t>& latencies, double cpu_freq_ghz,
                            size_t msg_size, const std::string& transport) {
    if (latencies.empty()) {
        return {0, 0, 0, 0, 0, 0, 0, msg_size, transport};
    }

    std::sort(latencies.begin(), latencies.end());
    size_t n = latencies.size();

    auto cycles_to_ns = [cpu_freq_ghz](uint64_t cycles) {
        return static_cast<double>(cycles) / cpu_freq_ghz;
    };

    double total_ns = 0;
    for (auto lat : latencies) {
        total_ns += cycles_to_ns(lat);
    }

    return {
        .min_ns = cycles_to_ns(latencies.front()),
        .p50_ns = cycles_to_ns(latencies[n / 2]),
        .p90_ns = cycles_to_ns(latencies[n * 90 / 100]),
        .p99_ns = cycles_to_ns(latencies[n * 99 / 100]),
        .p999_ns = cycles_to_ns(latencies[n * 999 / 1000]),
        .max_ns = cycles_to_ns(latencies.back()),
        .throughput_mps = static_cast<double>(n) / total_ns * 1e3,
        .message_size = msg_size,
        .transport = transport
    };
}

// ============================================================================
// MPSC Queue Baseline Benchmark
// ============================================================================

template<typename MessageT>
LatencyResult benchmark_mpsc_oneway(size_t iterations, double cpu_freq_ghz) {
    using QueueType = MPSCQueue<MessageT, QUEUE_CAPACITY, BusySpinWait>;
    auto queue_ptr = std::make_unique<QueueType>();
    auto& queue = *queue_ptr;

    std::vector<uint64_t> latencies;
    latencies.reserve(iterations);

    std::atomic<bool> done{false};
    std::atomic<bool> consumer_ready{false};

    // Consumer thread - start immediately
    std::thread consumer([&]() {
        consumer_ready.store(true, std::memory_order_release);

        MessageT msg;
        size_t count = 0;
        while (count < iterations + WARMUP_ITERATIONS) {
            if (queue.try_pop(msg)) {
                if (count >= WARMUP_ITERATIONS) {
                    uint64_t recv_ts = rdtsc();
                    latencies.push_back(recv_ts - msg.timestamp);
                }
                ++count;
            } else {
                _mm_pause();
            }
        }
        done.store(true, std::memory_order_release);
    });

    // Wait for consumer to be ready
    while (!consumer_ready.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    // Warmup
    for (size_t i = 0; i < WARMUP_ITERATIONS; ++i) {
        MessageT msg{};
        msg.sequence = i;
        msg.timestamp = rdtsc();
        while (!queue.try_push(msg)) {
            _mm_pause();
        }
    }

    // Small pause to let warmup complete
    std::this_thread::sleep_for(std::chrono::microseconds(100));

    // Producer - benchmark iterations
    for (size_t i = 0; i < iterations; ++i) {
        MessageT m{};
        m.sequence = i;
        m.timestamp = rdtsc();
        while (!queue.try_push(m)) {
            _mm_pause();
        }
    }

    consumer.join();

    return compute_stats(latencies, cpu_freq_ghz, sizeof(MessageT), "MPSC Queue");
}

template<typename MessageT>
LatencyResult benchmark_mpsc_roundtrip(size_t iterations, double cpu_freq_ghz) {
    using QueueType = MPSCQueue<MessageT, QUEUE_CAPACITY, BusySpinWait>;

    auto ping_queue = std::make_unique<QueueType>();
    auto pong_queue = std::make_unique<QueueType>();

    std::vector<uint64_t> latencies;
    latencies.reserve(iterations);

    std::atomic<bool> done{false};
    std::atomic<bool> start{false};

    // Responder thread
    std::thread responder([&]() {
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        MessageT msg;
        while (!done.load(std::memory_order_acquire)) {
            if (ping_queue->try_pop(msg)) {
                while (!pong_queue->try_push(msg)) {
                    _mm_pause();
                }
            } else {
                _mm_pause();
            }
        }
    });

    // Warmup
    for (size_t i = 0; i < WARMUP_ITERATIONS / 10; ++i) {
        MessageT msg{};
        msg.sequence = i;
        msg.timestamp = rdtsc();
        while (!ping_queue->try_push(msg)) {
            _mm_pause();
        }
    }
    std::this_thread::sleep_for(milliseconds(10));
    MessageT msg;
    while (pong_queue->try_pop(msg)) {}
    while (ping_queue->try_pop(msg)) {}

    // Start benchmark
    start.store(true, std::memory_order_release);

    // Ping-pong
    for (size_t i = 0; i < iterations; ++i) {
        MessageT m{};
        m.sequence = i;
        m.timestamp = rdtsc();

        while (!ping_queue->try_push(m)) {
            _mm_pause();
        }

        while (!pong_queue->try_pop(m)) {
            _mm_pause();
        }

        uint64_t rtt = rdtsc() - m.timestamp;
        latencies.push_back(rtt / 2);  // One-way from RTT
    }

    done.store(true, std::memory_order_release);
    responder.join();

    return compute_stats(latencies, cpu_freq_ghz, sizeof(MessageT), "MPSC RTT");
}

// ============================================================================
// Aeron Media Driver Management
// ============================================================================

class MediaDriverProcess {
public:
    MediaDriverProcess() : driver_pid_(-1), aeron_dir_("/dev/shm/aeron-nexusfix-bench") {
        // Clean up any previous run
        cleanup_aeron_dir();

        // Try to start the media driver
        if (!start_driver()) {
            std::cerr << "WARNING: Could not start Aeron media driver.\n";
            std::cerr << "Please ensure 'aeronmd' is installed or Java Aeron driver is available.\n";
            std::cerr << "Install: apt install aeron-driver OR use Java driver\n";
            available_ = false;
            return;
        }

        // Wait for driver to be ready
        std::this_thread::sleep_for(milliseconds(500));

        // Verify driver is running
        if (!check_driver_running()) {
            std::cerr << "WARNING: Aeron media driver failed to start.\n";
            available_ = false;
            return;
        }

        available_ = true;
    }

    ~MediaDriverProcess() {
        stop_driver();
        cleanup_aeron_dir();
    }

    bool is_available() const { return available_; }
    std::string aeronDir() const { return aeron_dir_; }

private:
    bool start_driver() {
        // First try C media driver (aeronmd)
        driver_pid_ = fork();
        if (driver_pid_ == 0) {
            // Child process
            // Redirect output to /dev/null
            freopen("/dev/null", "w", stdout);
            freopen("/dev/null", "w", stderr);

            execlp("aeronmd", "aeronmd",
                   "-Daeron.dir=/dev/shm/aeron-nexusfix-bench",
                   "-Daeron.threading.mode=SHARED",
                   "-Daeron.dir.delete.on.start=true",
                   nullptr);

            // If exec fails, exit child
            _exit(1);
        } else if (driver_pid_ < 0) {
            return false;
        }

        // Give it time to start
        std::this_thread::sleep_for(milliseconds(200));

        // Check if it started successfully
        int status;
        pid_t result = waitpid(driver_pid_, &status, WNOHANG);
        if (result == driver_pid_) {
            // Process exited, aeronmd not available
            driver_pid_ = -1;
            return false;
        }

        return true;
    }

    bool check_driver_running() {
        if (driver_pid_ <= 0) return false;

        // Check if CnC file exists (indicates driver is ready)
        std::string cnc_file = aeron_dir_ + "/cnc.dat";
        std::ifstream f(cnc_file);
        return f.good();
    }

    void stop_driver() {
        if (driver_pid_ > 0) {
            kill(driver_pid_, SIGTERM);
            std::this_thread::sleep_for(milliseconds(100));
            kill(driver_pid_, SIGKILL);
            waitpid(driver_pid_, nullptr, 0);
            driver_pid_ = -1;
        }
    }

    void cleanup_aeron_dir() {
        std::string cmd = "rm -rf " + aeron_dir_ + " 2>/dev/null";
        (void)system(cmd.c_str());
    }

    pid_t driver_pid_;
    std::string aeron_dir_;
    bool available_ = false;
};

std::shared_ptr<aeron::Aeron> create_aeron_client(const std::string& aeron_dir) {
    aeron::Context ctx;
    ctx.aeronDir(aeron_dir);
    ctx.preTouchMappedMemory(true);

    return aeron::Aeron::connect(ctx);
}

template<size_t MsgSize>
LatencyResult benchmark_aeron_oneway(const std::string& channel, size_t iterations,
                                     double cpu_freq_ghz, MediaDriverProcess& driver) {
    auto aeron = create_aeron_client(driver.aeronDir());

    // Add publication
    int64_t pub_id = aeron->addPublication(channel, STREAM_ID);
    std::shared_ptr<aeron::Publication> pub;
    while (!(pub = aeron->findPublication(pub_id))) {
        std::this_thread::yield();
    }

    // Add subscription
    int64_t sub_id = aeron->addSubscription(channel, STREAM_ID);
    std::shared_ptr<aeron::Subscription> sub;
    while (!(sub = aeron->findSubscription(sub_id))) {
        std::this_thread::yield();
    }

    // Wait for connection
    while (!pub->isConnected()) {
        std::this_thread::yield();
    }

    std::vector<uint64_t> latencies;
    latencies.reserve(iterations);

    std::atomic<bool> done{false};
    std::atomic<size_t> received{0};

    // Consumer thread
    std::thread consumer([&]() {
        aeron::fragment_handler_t handler =
            [&](aeron::concurrent::AtomicBuffer& buffer,
                aeron::util::index_t offset,
                aeron::util::index_t length,
                aeron::Header& header) {
                uint64_t recv_ts = rdtsc();
                uint64_t send_ts;
                buffer.getBytes(offset + 8, reinterpret_cast<uint8_t*>(&send_ts), sizeof(send_ts));
                latencies.push_back(recv_ts - send_ts);
                received.fetch_add(1, std::memory_order_relaxed);
            };

        while (received.load(std::memory_order_relaxed) < iterations) {
            sub->poll(handler, 10);
        }
        done.store(true, std::memory_order_release);
    });

    // Buffer for sending
    std::array<uint8_t, MsgSize> msg_buffer{};
    aeron::concurrent::AtomicBuffer src_buffer(msg_buffer.data(), msg_buffer.size());

    // Warmup
    for (size_t i = 0; i < WARMUP_ITERATIONS; ++i) {
        uint64_t ts = rdtsc();
        src_buffer.putInt64(0, static_cast<int64_t>(i));
        src_buffer.putInt64(8, static_cast<int64_t>(ts));

        while (pub->offer(src_buffer) < 0) {
            std::this_thread::yield();
        }
    }
    // Wait for warmup to be consumed
    while (received.load(std::memory_order_relaxed) < WARMUP_ITERATIONS) {
        std::this_thread::yield();
    }

    // Reset for actual benchmark
    latencies.clear();
    received.store(0, std::memory_order_relaxed);

    // Producer
    for (size_t i = 0; i < iterations; ++i) {
        uint64_t ts = rdtsc();
        src_buffer.putInt64(0, static_cast<int64_t>(i));
        src_buffer.putInt64(8, static_cast<int64_t>(ts));

        while (pub->offer(src_buffer) < 0) {
            _mm_pause();
        }
    }

    consumer.join();

    std::string transport = (channel == IPC_CHANNEL) ? "Aeron IPC" : "Aeron UDP";
    return compute_stats(latencies, cpu_freq_ghz, MsgSize, transport);
}

template<size_t MsgSize>
LatencyResult benchmark_aeron_roundtrip(const std::string& channel, size_t iterations,
                                        double cpu_freq_ghz, MediaDriverProcess& driver) {
    auto aeron = create_aeron_client(driver.aeronDir());

    // Ping publication/subscription
    int64_t ping_pub_id = aeron->addPublication(channel, PING_STREAM_ID);
    int64_t ping_sub_id = aeron->addSubscription(channel, PING_STREAM_ID);

    // Pong publication/subscription
    int64_t pong_pub_id = aeron->addPublication(channel, PONG_STREAM_ID);
    int64_t pong_sub_id = aeron->addSubscription(channel, PONG_STREAM_ID);

    std::shared_ptr<aeron::Publication> ping_pub, pong_pub;
    std::shared_ptr<aeron::Subscription> ping_sub, pong_sub;

    while (!(ping_pub = aeron->findPublication(ping_pub_id))) std::this_thread::yield();
    while (!(ping_sub = aeron->findSubscription(ping_sub_id))) std::this_thread::yield();
    while (!(pong_pub = aeron->findPublication(pong_pub_id))) std::this_thread::yield();
    while (!(pong_sub = aeron->findSubscription(pong_sub_id))) std::this_thread::yield();

    while (!ping_pub->isConnected() || !pong_pub->isConnected()) {
        std::this_thread::yield();
    }

    std::vector<uint64_t> latencies;
    latencies.reserve(iterations);

    std::atomic<bool> done{false};

    // Responder thread
    std::thread responder([&]() {
        std::array<uint8_t, MsgSize> echo_buffer{};
        aeron::concurrent::AtomicBuffer echo_src(echo_buffer.data(), echo_buffer.size());

        aeron::fragment_handler_t handler =
            [&](aeron::concurrent::AtomicBuffer& buffer,
                aeron::util::index_t offset,
                aeron::util::index_t length,
                aeron::Header& header) {
                // Copy received data
                buffer.getBytes(offset, echo_buffer.data(), length);

                // Echo back
                while (pong_pub->offer(echo_src, 0, length) < 0) {
                    _mm_pause();
                }
            };

        while (!done.load(std::memory_order_acquire)) {
            ping_sub->poll(handler, 10);
        }
    });

    std::array<uint8_t, MsgSize> msg_buffer{};
    aeron::concurrent::AtomicBuffer src_buffer(msg_buffer.data(), msg_buffer.size());

    std::atomic<bool> pong_received{false};

    aeron::fragment_handler_t pong_handler =
        [&](aeron::concurrent::AtomicBuffer& buffer,
            aeron::util::index_t offset,
            aeron::util::index_t length,
            aeron::Header& header) {
            pong_received.store(true, std::memory_order_release);
        };

    // Warmup
    for (size_t i = 0; i < WARMUP_ITERATIONS / 10; ++i) {
        pong_received.store(false, std::memory_order_release);

        uint64_t ts = rdtsc();
        src_buffer.putInt64(0, static_cast<int64_t>(i));
        src_buffer.putInt64(8, static_cast<int64_t>(ts));

        while (ping_pub->offer(src_buffer) < 0) {
            std::this_thread::yield();
        }

        while (!pong_received.load(std::memory_order_acquire)) {
            pong_sub->poll(pong_handler, 10);
        }
    }

    // Benchmark
    for (size_t i = 0; i < iterations; ++i) {
        pong_received.store(false, std::memory_order_release);

        uint64_t send_ts = rdtsc();
        src_buffer.putInt64(0, static_cast<int64_t>(i));
        src_buffer.putInt64(8, static_cast<int64_t>(send_ts));

        while (ping_pub->offer(src_buffer) < 0) {
            _mm_pause();
        }

        while (!pong_received.load(std::memory_order_acquire)) {
            pong_sub->poll(pong_handler, 10);
        }

        uint64_t rtt = rdtsc() - send_ts;
        latencies.push_back(rtt / 2);  // One-way from RTT
    }

    done.store(true, std::memory_order_release);
    responder.join();

    std::string transport = (channel == IPC_CHANNEL) ? "Aeron IPC RTT" : "Aeron UDP RTT";
    return compute_stats(latencies, cpu_freq_ghz, MsgSize, transport);
}

// ============================================================================
// Output Formatting
// ============================================================================

void print_separator() {
    std::cout << std::string(90, '=') << "\n";
}

void print_header(const char* title) {
    std::cout << "\n";
    print_separator();
    std::cout << "  " << title << "\n";
    print_separator();
}

void print_result(const LatencyResult& r) {
    std::cout << std::left << std::setw(20) << r.transport
              << std::right
              << std::setw(8) << r.message_size << "B"
              << std::setw(12) << std::fixed << std::setprecision(1) << r.p50_ns
              << std::setw(12) << r.p99_ns
              << std::setw(12) << r.p999_ns
              << std::setw(15) << std::setprecision(2) << r.throughput_mps
              << "\n";
}

void print_result_table_header() {
    std::cout << std::left << std::setw(20) << "Transport"
              << std::right
              << std::setw(9) << "Size"
              << std::setw(12) << "P50 (ns)"
              << std::setw(12) << "P99 (ns)"
              << std::setw(12) << "P999 (ns)"
              << std::setw(15) << "Tput (M/s)"
              << "\n";
    std::cout << std::string(80, '-') << "\n";
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << std::fixed << std::setprecision(2);

    print_header("TICKET_204: Aeron IPC Benchmark");

    // CPU calibration
    double cpu_freq_ghz = get_cpu_freq_ghz();
    std::cout << "\nCPU Frequency: " << cpu_freq_ghz << " GHz\n";
    std::cout << "Iterations: " << BENCHMARK_ITERATIONS / 1000 << "K\n";
    std::cout << "Warmup: " << WARMUP_ITERATIONS << "\n\n";

    // Start media driver
    std::cout << "Starting Aeron media driver...\n";
    MediaDriverProcess driver;

    bool aeron_available = driver.is_available();
    if (aeron_available) {
        std::cout << "Media driver started at: " << driver.aeronDir() << "\n\n";
    } else {
        std::cout << "\nAeron media driver NOT available - running MPSC baseline only.\n";
        std::cout << "To enable Aeron benchmarks, install aeronmd:\n";
        std::cout << "  sudo apt install aeron-driver\n\n";
    }

    std::vector<LatencyResult> results;

    // ========================================================================
    // One-Way Latency Tests
    // ========================================================================

    print_header("One-Way Latency (Producer -> Consumer)");
    print_result_table_header();

    // MPSC Queue baseline (64B)
    {
        auto r = benchmark_mpsc_oneway<TestMessage64>(BENCHMARK_ITERATIONS, cpu_freq_ghz);
        results.push_back(r);
        print_result(r);
    }

    // Aeron IPC (64B)
    if (aeron_available) {
        auto r = benchmark_aeron_oneway<64>(IPC_CHANNEL, BENCHMARK_ITERATIONS, cpu_freq_ghz, driver);
        results.push_back(r);
        print_result(r);
    }

    std::cout << std::string(80, '-') << "\n";

    // MPSC Queue (256B)
    {
        auto r = benchmark_mpsc_oneway<TestMessage256>(BENCHMARK_ITERATIONS, cpu_freq_ghz);
        results.push_back(r);
        print_result(r);
    }

    // Aeron IPC (256B)
    if (aeron_available) {
        auto r = benchmark_aeron_oneway<256>(IPC_CHANNEL, BENCHMARK_ITERATIONS, cpu_freq_ghz, driver);
        results.push_back(r);
        print_result(r);
    }

    std::cout << std::string(80, '-') << "\n";

    // MPSC Queue (1024B)
    {
        auto r = benchmark_mpsc_oneway<TestMessage1024>(BENCHMARK_ITERATIONS, cpu_freq_ghz);
        results.push_back(r);
        print_result(r);
    }

    // Aeron IPC (1024B)
    if (aeron_available) {
        auto r = benchmark_aeron_oneway<1024>(IPC_CHANNEL, BENCHMARK_ITERATIONS, cpu_freq_ghz, driver);
        results.push_back(r);
        print_result(r);
    }

    // ========================================================================
    // Round-Trip Latency Tests
    // ========================================================================

    print_header("Round-Trip Latency (Ping-Pong, half RTT shown)");
    print_result_table_header();

    // Reduce iterations for RTT tests (they're slower)
    constexpr size_t RTT_ITERATIONS = 10'000;

    // MPSC RTT (64B)
    {
        auto r = benchmark_mpsc_roundtrip<TestMessage64>(RTT_ITERATIONS, cpu_freq_ghz);
        results.push_back(r);
        print_result(r);
    }

    // Aeron IPC RTT (64B)
    if (aeron_available) {
        auto r = benchmark_aeron_roundtrip<64>(IPC_CHANNEL, RTT_ITERATIONS, cpu_freq_ghz, driver);
        results.push_back(r);
        print_result(r);
    }

    // ========================================================================
    // Aeron UDP (optional, for completeness)
    // ========================================================================

    if (aeron_available) {
        print_header("Aeron UDP Loopback (for reference)");
        print_result_table_header();

        // Aeron UDP (64B)
        auto r = benchmark_aeron_oneway<64>(UDP_CHANNEL, BENCHMARK_ITERATIONS / 10, cpu_freq_ghz, driver);
        results.push_back(r);
        print_result(r);
    }

    // ========================================================================
    // Summary
    // ========================================================================

    print_header("Summary Comparison");

    std::cout << "\n| Transport | Size | P50 | P99 | vs MPSC |\n";
    std::cout << "|-----------|------|-----|-----|--------|\n";

    // Find baseline for each size
    for (size_t size : MESSAGE_SIZES) {
        double mpsc_p50 = 0;
        for (const auto& r : results) {
            if (r.message_size == size && r.transport == "MPSC Queue") {
                mpsc_p50 = r.p50_ns;
                std::cout << "| " << std::left << std::setw(9) << r.transport
                          << " | " << std::setw(4) << size << "B"
                          << " | " << std::right << std::setw(6) << std::setprecision(1) << r.p50_ns
                          << " | " << std::setw(6) << r.p99_ns
                          << " | baseline |\n";
                break;
            }
        }

        for (const auto& r : results) {
            if (r.message_size == size && r.transport == "Aeron IPC") {
                double ratio = r.p50_ns / mpsc_p50;
                std::cout << "| " << std::left << std::setw(9) << r.transport
                          << " | " << std::setw(4) << size << "B"
                          << " | " << std::right << std::setw(6) << std::setprecision(1) << r.p50_ns
                          << " | " << std::setw(6) << r.p99_ns
                          << " | " << std::setprecision(1) << ratio << "x |\n";
            }
        }
    }

    // ========================================================================
    // Decision Criteria
    // ========================================================================

    print_header("Decision Criteria");

    double aeron_ipc_64_p50 = 0;
    double mpsc_64_p50 = 0;

    for (const auto& r : results) {
        if (r.message_size == 64 && r.transport == "Aeron IPC") {
            aeron_ipc_64_p50 = r.p50_ns;
        }
        if (r.message_size == 64 && r.transport == "MPSC Queue") {
            mpsc_64_p50 = r.p50_ns;
        }
    }

    std::cout << "\nMPSC Queue P50 (64B): " << mpsc_64_p50 << " ns\n";

    if (aeron_available && aeron_ipc_64_p50 > 0) {
        std::cout << "Aeron IPC P50 (64B): " << aeron_ipc_64_p50 << " ns\n";
        std::cout << "Overhead ratio: " << (aeron_ipc_64_p50 / mpsc_64_p50) << "x\n\n";

        if (aeron_ipc_64_p50 < 200) {
            std::cout << "RESULT: Aeron IPC < 200ns - VIABLE for internal messaging\n";
        } else if (aeron_ipc_64_p50 / mpsc_64_p50 > 5.0) {
            std::cout << "RESULT: Aeron IPC overhead > 5x MPSC\n";
            std::cout << "RECOMMENDATION: Use MPSC for in-process, Aeron for cross-process only\n";
        } else {
            std::cout << "RESULT: Aeron IPC acceptable for cross-process messaging\n";
        }
    } else {
        std::cout << "\nAeron benchmarks not run (driver unavailable).\n";
        std::cout << "MPSC Queue is the baseline for in-process messaging.\n";
    }

    print_separator();
    std::cout << "\nBenchmark complete.\n";

    return 0;
}
