# NexusFIX

High-performance FIX 4.4/5.0 protocol engine for quantitative trading.

## Performance

**3x faster than QuickFIX**

| Operation | QuickFIX | NexusFIX | Speedup |
|-----------|----------|----------|---------|
| ExecutionReport parse | 730 ns | 246 ns | **3.0x** |
| NewOrderSingle parse | 661 ns | 229 ns | **2.9x** |
| Throughput | 1.19M msg/sec | 4.17M msg/sec | **3.5x** |

## Features

- **Zero-copy parsing** - `std::span` views without memory allocation
- **SIMD acceleration** - AVX2 field boundary scanning
- **Compile-time optimization** - `consteval` field offset calculation
- **O(1) field lookup** - Pre-indexed field table
- **C++23** - Modern language features (`std::expected`, concepts)

## Supported Messages

### FIX 4.4 / FIX 5.0

| Type | Message | Description |
|------|---------|-------------|
| A | Logon | Session authentication |
| 5 | Logout | Session termination |
| 0 | Heartbeat | Session keepalive |
| D | NewOrderSingle | Order submission |
| F | OrderCancelRequest | Cancel order |
| 8 | ExecutionReport | Order fills/status |
| V | MarketDataRequest | Subscribe to data |
| W | MarketDataSnapshotFullRefresh | Full snapshot |
| X | MarketDataIncrementalRefresh | Incremental update |

## Quick Start

```cpp
#include <nexusfix/nexusfix.hpp>

using namespace nfx;
using namespace nfx::fix44;

// Connect
TcpTransport transport;
transport.connect("fix.broker.com", 9876);

// Session setup
SessionConfig config{
    .sender_comp_id = "MY_CLIENT",
    .target_comp_id = "BROKER",
    .heartbeat_interval = 30
};
SessionManager session{transport, config};
session.initiate_logon();

// Send order
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

## Build

```bash
# Build
./start.sh build

# Run benchmarks
./start.sh bench

# Run tests
./start.sh test
```

### Requirements

- C++23 compiler (GCC 13+ or Clang 17+)
- CMake 3.20+
- Linux (for io_uring support, optional)

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `NFX_ENABLE_SIMD` | ON | Enable AVX2 SIMD acceleration |
| `NFX_ENABLE_IO_URING` | OFF | Enable io_uring transport |
| `NFX_BUILD_BENCHMARKS` | ON | Build benchmark executables |
| `NFX_BUILD_TESTS` | ON | Build test executables |
| `NFX_BUILD_EXAMPLES` | ON | Build example programs |

## Documentation

- [API Reference](docs/API_REFERENCE.md) - Complete API documentation
- [Implementation Summary](docs/design/TICKET_005_NEXUSFIX_IMPLEMENTATION_SUMMARY.md) - Module overview
- [Benchmark Report](docs/compare/BENCHMARK_COMPARISON_REPORT.md) - Performance analysis

## Project Structure

```
nexusfix/
├── include/nexusfix/
│   ├── parser/           # Zero-copy FIX parser
│   ├── session/          # Session state machine
│   ├── transport/        # TCP/io_uring transport
│   ├── types/            # Strong types, tags, errors
│   ├── memory/           # Buffer pools
│   ├── messages/fix44/   # FIX 4.4 message types
│   └── interfaces/       # Message concepts
├── benchmarks/           # Performance benchmarks
├── tests/                # Unit tests
├── examples/             # Example programs
└── docs/                 # Documentation
```

## License

MIT License

## Contributing

Contributions are welcome. Please ensure:
- Code follows C++23 standards
- No allocations on hot paths
- Include benchmarks for performance-critical changes
