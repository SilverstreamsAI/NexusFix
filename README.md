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
  <img src="https://img.shields.io/badge/License-MIT-green.svg" alt="MIT License">
  <img src="https://img.shields.io/badge/Platform-Linux-lightgrey.svg" alt="Linux">
  <img src="https://img.shields.io/badge/Speed-3x%20QuickFIX-orange.svg" alt="3x Faster">
</p>

<p align="center">
  <a href="#performance">Performance</a> |
  <a href="#features">Features</a> |
  <a href="#quick-start">Quick Start</a> |
  <a href="#documentation">Docs</a> |
  <a href="#development">Development</a>
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
| Error Handling | Exceptions | `std::expected` (no throw) |


---

## Features

### Core Capabilities

- **Zero-Copy Parsing** - `std::span<const char>` views into original buffer, no `memcpy`
- **SIMD Acceleration** - AVX2/AVX-512 instructions for delimiter scanning
- **Compile-Time Optimization** - `consteval` field offsets, `constexpr` validation
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

## Documentation

- [API Reference](docs/API_REFERENCE.md) - Complete API documentation
- [Implementation Guide](docs/design/TICKET_005_NEXUSFIX_IMPLEMENTATION_SUMMARY.md) - Architecture overview
- [Benchmark Report](docs/compare/BENCHMARK_COMPARISON_REPORT.md) - Detailed performance analysis
- [Modern C++ Techniques](docs/modernc_quant.md) - Optimization techniques used

---

## Project Structure

```
nexusfix/
├── include/nexusfix/
│   ├── parser/           # Zero-copy FIX parser (SIMD)
│   ├── session/          # Session state machine
│   ├── transport/        # TCP / io_uring transport
│   ├── types/            # Strong types (Price, Qty, Side)
│   ├── memory/           # PMR buffer pools
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

## Development

Built with **Modern C++23**. Optimized via hardware-aware high-performance patterns including cache-line alignment, SIMD vectorization, and zero-copy memory design. Verified through rigorous benchmarking and AI-assisted static analysis.

For technical deep-dives on our optimization journey, see [Optimization Diary](docs/optimization_diary.md).

---

## Contributing

This project is maintained by **SilverstreamsAI**.

- **Issues & Discussions**: Welcome for bug reports, performance questions, and feature discussions
- **Pull Requests**: Currently limited to **bug fixes only**

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
