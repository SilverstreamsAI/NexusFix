<p align="center">
  <h1 align="center">NexusFIX</h1>
  <p align="center">
    <strong>Ultra-Low Latency FIX Protocol Engine for High-Frequency Trading</strong>
  </p>
  <p align="center">
    Modern C++23 | Zero-Copy | SIMD-Accelerated | 3x Faster than QuickFIX
  </p>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-23-blue.svg" alt="C++23">
  <img src="https://img.shields.io/badge/Library-Header--Only-blue.svg" alt="Header-Only">
  <img src="https://img.shields.io/badge/License-MIT-green.svg" alt="MIT License">
  <img src="https://img.shields.io/badge/Platform-Linux%20|%20macOS%20|%20Windows-lightgrey.svg" alt="Cross-Platform">
  <img src="https://img.shields.io/badge/Speed-3x%20QuickFIX-orange.svg" alt="3x Faster">
  <img src="https://img.shields.io/badge/Allocations-Zero%20on%20Hot%20Path-brightgreen.svg" alt="Zero Allocation">
</p>

<p align="center">
  <a href="#performance">Performance</a> |
  <a href="#architecture-influences">Architecture</a> |
  <a href="#features">Features</a> |
  <a href="#quick-start">Quick Start</a> |
  <a href="#documentation">Docs</a> |
  <a href="#commercial-support">Enterprise</a>
</p>

---

## Why NexusFIX?

**NexusFIX** is a high-performance **FIX protocol** (Financial Information eXchange) engine built for **ultra-low latency quantitative trading**, **sub-microsecond algorithmic execution**, and **high-frequency trading (HFT)** systems. It solves the **performance bottlenecks** of traditional FIX engines by utilizing **hardware-aware C++ programming**.

NexusFIX serves as a modern, faster alternative to QuickFIX with **zero heap allocations** on the critical path.

> *"If you're building a low-latency trading system and QuickFIX is your bottleneck, NexusFIX is your solution."*

---

## Performance

### NexusFIX vs QuickFIX Benchmark

Tested on Linux with GCC 13.3, 100,000 iterations:

| Metric | QuickFIX | NexusFIX | Improvement |
|--------|----------|----------|-------------|
| **ExecutionReport Parse** | 730 ns | 246 ns | **3.0x faster** |
| **NewOrderSingle Parse** | 661 ns | 229 ns | **2.9x faster** |
| **Field Access (4 fields)** | 31 ns | 11 ns | **2.9x faster** |
| **Throughput** | 1.19M msg/sec | 4.17M msg/sec | **3.5x higher** |
| **P99 Latency** | 784 ns | 258 ns | **3.0x lower** |

### Why is NexusFIX Faster?

| Technique | QuickFIX | NexusFIX |
|-----------|----------|----------|
| Memory | Heap allocation per message | Zero-copy `std::span` views |
| Field Lookup | O(log n) `std::map` | O(1) direct array indexing |
| Parsing | Byte-by-byte scanning | AVX2 SIMD vectorized |
| Field Offsets | Runtime calculation | `consteval` compile-time |
| Enum/Type Conversion | Runtime switch chains (~300 branches) | 22 compile-time lookup tables (55-97% faster) |
| Error Handling | Exceptions | `std::expected` (no throw) |

### Zero Allocation Proof

Processing a **NewOrderSingle** message on the hot path:

| Operation | QuickFIX | NexusFIX |
|-----------|----------|----------|
| **Heap Allocations** | ~12 (`std::string`, `std::map` nodes) | **0** |
| **Field Storage** | `std::map<int, std::string>` copies | `std::span` views into original buffer |
| **Parsing Logic** | Runtime map insertion | Compile-time offset table |
| **Memory Footprint** | Dynamic, unpredictable | Static, pre-allocated PMR pool |
| **Destructor Overhead** | ~12 `std::string` destructors | **0** (no owned memory) |

*Verified via custom allocator instrumentation. See [Optimization Diary](docs/optimization_diary.md).*

*For kernel bypass (DPDK/AF_XDP) and FPGA acceleration, see [Enterprise](#commercial-support).*

---

## Architecture Influences

NexusFIX stands on the shoulders of giants. We systematically studied **11 industry-leading Modern C++ libraries** and applied their techniques to ultra-low latency FIX processing. Below is our learning journey: what we learned, what we built, and what improvement we measured.

### Learning → Implementation → Verification

| Source Library | Engineering Evaluation | What We Changed | Benchmark Result |
|----------------|------------------------|-----------------|------------------|
| [hffix](https://github.com/jamesdbrock/hffix) | O(n) iterator-based field lookup is suboptimal for dense FIX packets; lacks compile-time optimization and type safety | `[Optimized]` `consteval` field offsets + `std::span` zero-copy views + O(1) direct indexing | **14ns** field access vs ~50ns iterator scan |
| [Abseil](https://github.com/abseil/abseil-cpp) | Swiss Tables offer SIMD-accelerated probing with 7-bit H2 fingerprints; superior cache locality for session maps | `[Adopted]` `absl::flat_hash_map` for session store | **[31% faster](docs/compare/ABSEIL_FLAT_HASH_MAP_BENCHMARK.md)** (20ns → 15ns) |
| [Quill](https://github.com/odygrd/quill) | Lock-free SPSC queue with deferred formatting; only viable approach for hot-path logging without blocking | `[Adopted]` Quill as logging backend | **8ns** median latency; zero blocking |
| [NanoLog](https://github.com/PlatformLab/NanoLog) | Binary encoding + background thread achieves 7ns; compile-time format validation essential for type safety | `[Synthesized]` `DeferredProcessor<T>` with static type-safe binary serialization | **[84% reduction](docs/compare/DEFERRED_PROCESSOR_BENCHMARK.md)** (75ns → 12ns) |
| [liburing](https://github.com/axboe/liburing) | `DEFER_TASKRUN` defers completion to userspace, eliminating kernel task wakeups; registered buffers avoid per-op mapping | `[Adopted]` io_uring + DEFER_TASKRUN + registered buffers + multishot | **[7-27% faster](docs/compare/DEFER_TASKRUN_BENCHMARK.md)**; ~30% fewer syscalls |
| [Highway](https://github.com/google/highway) | Portable SIMD abstraction across AVX2/AVX-512/NEON/SVE; slight overhead vs direct intrinsics | `[Evaluated]` Retained hand-tuned intrinsics for FIX-specific patterns | **13x throughput**; Highway deferred for ARM |
| [Seastar](https://github.com/scylladb/seastar) | Share-nothing reactor optimal for high-concurrency I/O; high abstraction overhead for single-threaded tick-to-trade paths | `[Influenced]` Extracted core-pinning + lock-free pipelining without framework | **[8% P99 improvement](docs/compare/CPU_AFFINITY_BENCHMARK.md)** (18.8ns → 17.3ns) |
| [Folly](https://github.com/facebook/folly) | Advanced memory fencing patterns and lock-free primitives; `folly::Function` overhead acceptable for cold path only | `[Influenced]` Native SPSC queue + bit-masking for tag validation | Comparable performance; zero dependency |
| [Rigtorp](https://github.com/rigtorp/SPSCQueue) | Cache-line padding (`alignas(64)`) eliminates false sharing; simplest correct SPSC implementation | `[Synthesized]` Native `SPSCQueue` with identical techniques | **88M ops/sec**; 11ns median |
| [xsimd](https://github.com/xtensor-stack/xsimd) | Generic SIMD wrappers useful for math, but FIX parsing requires byte-level shuffle control | `[Evaluated]` Direct Intel intrinsics for SOH/delimiter scanning | **2x faster** than generic wrappers |
| [Boost.PMR](https://www.boost.org/doc/libs/release/libs/container/doc/html/container/polymorphic_memory_resources.html) | Standard allocators induce non-deterministic jitter; monotonic buffer enables arena allocation per message | `[Adopted]` `std::pmr::monotonic_buffer_resource` | **Zero heap allocation** on hot path |

### What We Built

| Component | Inspired By | Implementation |
|-----------|-------------|----------------|
| `TagOffsetMap` | hffix | Compile-time generated O(1) field lookup table |
| `DeferredProcessor<T>` | NanoLog | SPSC queue + background thread for async processing |
| `ThreadLocalPool<T>` | NanoLog, Folly | Per-thread object pool, zero lock contention |
| `SPSCQueue<T>` | Rigtorp, Folly | Cache-line aligned lock-free queue |
| `simd_scanner` | xsimd (concept) | Hand-tuned AVX2/AVX-512 SOH and delimiter scanning |
| `IoUringTransport` | liburing | DEFER_TASKRUN + registered buffers + multishot recv |
| `CpuAffinity` | Seastar | Thread-to-core pinning utility |

### Cumulative Impact

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| ExecutionReport Parse | 730 ns | 246 ns | **3.0x faster** |
| Hot Path Latency | 361 ns | 213 ns | **41% reduction** |
| SIMD SOH Scan | ~150 ns | 11.8 ns | **~13x faster** |
| Hash Map Lookup | 20 ns | 15 ns | **31% faster** |
| P99 Tail Latency | 784 ns | 258 ns | **3.0x lower** |

*Detailed benchmarks: [Optimization Summary](docs/compare/OPTIMIZATION_SUMMARY_BEFORE_AFTER.md)*

### Attribution

NexusFIX is MIT licensed. We gratefully acknowledge these open source projects:

| Dependency | License | Usage |
|------------|---------|-------|
| [Abseil](https://github.com/abseil/abseil-cpp) | Apache 2.0 | `flat_hash_map` for session lookups |
| [Quill](https://github.com/odygrd/quill) | MIT | Async logging infrastructure |
| [liburing](https://github.com/axboe/liburing) | MIT/LGPL | io_uring C wrapper |

---

## Features

### Core Capabilities

- **Zero-Copy Parsing** - `std::span<const char>` views into original buffer, no `memcpy`
- **SIMD Acceleration** - AVX2/AVX-512 instructions for delimiter scanning
- **Compile-Time Optimization** - `consteval` field offsets, 22 lookup tables for enum/type conversion, ~300 runtime branches eliminated
- **O(1) Field Access** - Pre-indexed lookup table by FIX tag number
- **Zero Heap Allocation** - PMR pools and stack allocation on hot path
- **Type-Safe API** - Strong types for Price, Quantity, Side, OrdType

### Modern C++23

- `std::expected` for error handling (no exceptions on hot path)
- `std::span` for zero-copy data views
- Concepts for compile-time interface validation
- `consteval` for compile-time computation
- `[[likely]]` / `[[unlikely]]` branch hints

### Supported FIX Versions

| Version | Status | Notes |
|---------|--------|-------|
| FIX 4.4 | Full Support | Most common in production |
| FIX 5.0 + FIXT 1.1 | Full Support | Only 2% overhead vs 4.4 |

### Supported Message Types

| MsgType | Name | Category |
|---------|------|----------|
| A | Logon | Session |
| 5 | Logout | Session |
| 0 | Heartbeat | Session |
| D | NewOrderSingle | Order Entry |
| F | OrderCancelRequest | Order Entry |
| 8 | ExecutionReport | Order Entry |
| V | MarketDataRequest | Market Data |
| W | MarketDataSnapshotFullRefresh | Market Data |
| X | MarketDataIncrementalRefresh | Market Data |

### Optimization Guide

How we achieved sub-300ns latency with Modern C++23:

- [Optimization Diary](docs/optimization_diary.md) - Step-by-step journey from 730ns to 246ns
- [Modern C++ Quant Techniques](docs/modernc_quant.md) - Cache-line alignment, SIMD, PMR strategies, branch hints

---

## Quick Start

### Installation

```bash
git clone https://github.com/SilverstreamsAI/NexusFIX.git
cd NexusFIX
./start.sh build
```

### Requirements

- **C++23 compiler**: GCC 13+ or Clang 17+
- **CMake**: 3.20+
- **OS**: Linux (io_uring optional), macOS, Windows

### Basic Usage

```cpp
#include <nexusfix/nexusfix.hpp>

using namespace nfx;
using namespace nfx::fix44;

// Connect to broker
TcpTransport transport;
transport.connect("fix.broker.com", 9876);

// Configure session
SessionConfig config{
    .sender_comp_id = "MY_CLIENT",
    .target_comp_id = "BROKER",
    .heartbeat_interval = 30
};
SessionManager session{transport, config};
session.initiate_logon();

// Send order (zero allocation)
MessageAssembler asm_;
NewOrderSingle::Builder order;
auto msg = order
    .cl_ord_id("ORD001")
    .symbol("AAPL")
    .side(Side::Buy)
    .order_qty(Qty::from_int(100))
    .ord_type(OrdType::Limit)
    .price(FixedPrice::from_double(150.00))
    .build(asm_);
transport.send(msg);
```

### Parse Incoming Messages

```cpp
// Zero-copy parsing
FixParser parser;
auto result = parser.parse(raw_buffer);

if (result) {
    auto& msg = *result;
    auto order_id = msg.get_string(Tag::OrderID);    // O(1) lookup
    auto exec_type = msg.get_char(Tag::ExecType);    // No allocation
    auto fill_qty = msg.get_qty(Tag::LastQty);       // Type-safe
}
```

---

## Build Options

| CMake Option | Default | Description |
|--------------|---------|-------------|
| `NFX_ENABLE_SIMD` | ON | AVX2/AVX-512 SIMD acceleration |
| `NFX_ENABLE_IO_URING` | OFF | Linux io_uring transport |
| `NFX_BUILD_BENCHMARKS` | ON | Build benchmark suite |
| `NFX_BUILD_TESTS` | ON | Build unit tests |
| `NFX_BUILD_EXAMPLES` | ON | Build examples |

```bash
# Build with all optimizations
cmake -B build -DCMAKE_BUILD_TYPE=Release -DNFX_ENABLE_SIMD=ON
cmake --build build -j

# Run benchmarks
./start.sh bench 100000

# Compare with QuickFIX
./start.sh compare 100000
```

---

## Benchmarking

Verify performance claims by running benchmarks yourself.

### NexusFIX Benchmark

```bash
# Run parser and session benchmarks
./start.sh bench 100000

# Example output:
# [BENCHMARK] ExecutionReport Parse
#   Iterations: 100000
#   Mean: 246 ns
#   P50:  245 ns
#   P99:  258 ns
```

### QuickFIX Comparison

Compare NexusFIX against QuickFIX (requires QuickFIX installed):

```bash
# Install QuickFIX first
# Ubuntu: sudo apt install libquickfix-dev
# Or build from source: https://github.com/quickfix/quickfix

# Run comparison
./start.sh compare 100000
```

### Benchmark Components

| Benchmark | File | What it measures |
|-----------|------|------------------|
| Parse | `parse_benchmark.cpp` | Message parsing latency |
| Session | `session_benchmark.cpp` | Session state machine |
| QuickFIX | `vs_quickfix/` | Side-by-side comparison |

### Interpreting Results

- **Mean**: Average latency (lower is better)
- **P50**: Median latency (50th percentile)
- **P99**: Tail latency (99th percentile) - critical for HFT
- **Throughput**: Messages per second (higher is better)

---

## Documentation

- [API Reference](docs/API_REFERENCE.md) - Complete API documentation
- [Implementation Guide](docs/design/IMPLEMENTATION_GUIDE.md) - Architecture overview
- [Benchmark Report](docs/compare/BENCHMARK_COMPARISON_REPORT.md) - Detailed performance analysis
- [Modern C++ Techniques](docs/modernc_quant.md) - Optimization techniques used

---

## Project Structure

```
nexusfix/
├── include/nexusfix/
│   ├── parser/           # Zero-copy FIX parser (SIMD)
│   ├── session/          # Session state machine
│   ├── transport/        # TCP / io_uring / Winsock transport
│   ├── platform/         # Cross-platform abstraction
│   ├── types/            # Strong types (Price, Qty, Side)
│   ├── memory/           # PMR buffer pools
│   ├── store/            # Message store (PMR-optimized)
│   ├── serializer/       # Message serialization
│   ├── util/             # Utilities (diagnostics, formatting)
│   ├── messages/fix44/   # FIX 4.4 message builders
│   └── interfaces/       # Concepts and interfaces
├── benchmarks/           # Performance benchmarks
├── tests/                # Unit tests
├── examples/             # Example programs
└── docs/                 # Documentation
```

---

## FAQ

### How does NexusFIX achieve zero-copy parsing?

NexusFIX uses `std::span<const char>` to create views into the original network buffer. Field values are never copied - the parser returns spans pointing to the exact byte range in the source buffer. This eliminates all `memcpy` and heap allocation overhead.

### Is NexusFIX compatible with QuickFIX?

NexusFIX implements the same FIX 4.4/5.0 protocol standards but with a different API optimized for performance. It is wire-compatible with any FIX counterparty, including systems using QuickFIX.

### What latency can I expect in production?

In our benchmarks: **~250 nanoseconds** for ExecutionReport parsing. Actual production latency depends on network, kernel configuration, and hardware. NexusFIX is designed to minimize the application-layer overhead.

### Does NexusFIX support FIX Repeating Groups?

Yes. Repeating groups are parsed with the same zero-copy approach. Group iteration is O(1) per entry.

---

## Use Cases

NexusFIX is designed for:

- **High-Frequency Trading (HFT)** - Sub-microsecond message processing
- **Algorithmic Trading Systems** - Low-latency order routing
- **Market Making** - High-throughput quote updates
- **Smart Order Routing (SOR)** - Multi-venue connectivity
- **Trading Infrastructure** - FIX gateways and bridges

---

## Contact

For advanced integration (kernel bypass, FPGA) or collaboration inquiries: contact@silverstream.tech

---

## Development

Built with **Modern C++23**. Optimized via hardware-aware high-performance patterns including cache-line alignment, SIMD vectorization, and zero-copy memory design. Verified through rigorous benchmarking and AI-assisted static analysis.

For technical deep-dives on our optimization journey, see [Optimization Diary](docs/optimization_diary.md).

---

## Contributing

This project is maintained by **SilverstreamsAI**.

- **Issues & Discussions**: Welcome for bug reports, performance questions, and feature discussions
- **Pull Requests**: Welcome

All contributions must follow:
- C++23 standards
- Zero allocation on hot paths
- Include benchmarks for performance changes

---

## License

MIT License - See [LICENSE](LICENSE) file.

---

<p align="center">
  <sub>Built with Modern C++23 for ultra-low latency quantitative trading</sub>
</p>
