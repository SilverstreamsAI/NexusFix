# NexusFIX Implementation Guide

## Overview

Summary of NexusFIX implementation status, including supported FIX 4.4 message types, core modules, and benchmark results.

---

## FIX 4.4 Message Types (Implemented)

### Session Management Messages

| Message Type | MsgType | Status | Description |
|--------------|---------|--------|-------------|
| Logon | A | Supported | Session authentication, heartbeat negotiation |
| Logout | 5 | Supported | Session termination |
| Heartbeat | 0 | Supported | Session keepalive |
| TestRequest | 1 | Supported | Heartbeat solicitation |
| ResendRequest | 2 | Supported | Gap fill request |
| SequenceReset | 4 | Supported | Sequence number reset/gap fill |
| Reject | 3 | Supported | Session-level rejection |

### Order Flow Messages

| Message Type | MsgType | Status | Description |
|--------------|---------|--------|-------------|
| NewOrderSingle | D | Supported | Order submission |
| ExecutionReport | 8 | Supported | Order fills, rejections, status |
| OrderCancelRequest | F | Supported | Cancel existing order |
| OrderCancelReplaceRequest | G | Supported | Modify existing order |
| OrderStatusRequest | H | Supported | Query order status |
| OrderCancelReject | 9 | Supported | Cancel rejection response |

### Market Data Messages

| Message Type | MsgType | Status | Description |
|--------------|---------|--------|-------------|
| MarketDataRequest | V | Supported | Subscribe/unsubscribe to market data |
| MarketDataSnapshotFullRefresh | W | Supported | Full market data snapshot |
| MarketDataIncrementalRefresh | X | Supported | Incremental market data update |
| MarketDataRequestReject | Y | Supported | Subscription rejection |

**Total: 17 message types implemented**

---

## Core Modules

### Parser Module (`include/nexusfix/parser/`)

| File | Features |
|------|----------|
| consteval_parser.hpp | Compile-time field specs, schema validation |
| runtime_parser.hpp | Zero-copy IndexedParser, O(1) field lookup |
| simd_scanner.hpp | AVX2 SOH detection, field boundary scanning |
| field_view.hpp | Zero-copy field reference, type conversion |
| repeating_group.hpp | Repeating group iterator, MDEntry parser |

### Session Module (`include/nexusfix/session/`)

| File | Features |
|------|----------|
| session_manager.hpp | Session lifecycle, admin message routing |
| state.hpp | 9-state machine, event-driven transitions |
| sequence.hpp | Sequence number tracking, gap detection |
| coroutine.hpp | C++20 async patterns |

### Transport Module (`include/nexusfix/transport/`)

| File | Features |
|------|----------|
| tcp_transport.hpp | POSIX socket, non-blocking I/O |
| socket.hpp | Abstract socket interface |
| io_uring_transport.hpp | Linux io_uring async I/O (optional) |

### Type System (`include/nexusfix/types/`)

| File | Features |
|------|----------|
| field_types.hpp | Strong types: FixedPrice, Qty, Side, OrdType |
| tag.hpp | Compile-time FIX tag definitions |
| error.hpp | std::expected error handling |
| market_data_types.hpp | MDEntryType, MDUpdateAction, MDReqRejReason |

### Memory (`include/nexusfix/memory/`)

| File | Features |
|------|----------|
| buffer_pool.hpp | Pre-allocated buffer pool, zero-allocation hot path |

---

## Benchmark Results (vs QuickFIX)

**Test Configuration:**
- Iterations: 100,000
- Messages: FIX 4.4 (ExecutionReport, NewOrderSingle, Heartbeat)
- Measurement: RDTSC cycle-accurate timing

### Parse Latency Comparison

| Message Type | QuickFIX | NexusFIX | Speedup |
|--------------|----------|----------|---------|
| ExecutionReport (35=8) | 730 ns | 246 ns | **3.0x** |
| NewOrderSingle (35=D) | 661 ns | 229 ns | **2.9x** |
| Heartbeat (35=0) | 312 ns | 203 ns | **1.5x** |

### Field Access Comparison

| Metric | QuickFIX | NexusFIX | Speedup |
|--------|----------|----------|---------|
| 4-field access | 31 ns | 11 ns | **2.9x** |

### Throughput Comparison

| Engine | Throughput | Bandwidth |
|--------|------------|-----------|
| QuickFIX | 1.19M msg/sec | 191 MB/sec |
| NexusFIX | 4.17M msg/sec | 687 MB/sec |
| **Speedup** | **3.5x** | **3.6x** |

### FIX 5.0 / FIXT 1.1 Support

| Metric | FIX 4.4 | FIX 5.0 | Overhead |
|--------|---------|---------|----------|
| ExecutionReport | 232 ns | 237 ns | +2% |
| Version detection | - | 9 ns | Negligible |

---

## C++ Features Used

| Feature | Usage |
|---------|-------|
| C++23 | std::expected, concepts |
| Zero-copy | std::span<const char> views |
| SIMD | AVX2 intrinsics for field scanning |
| Compile-time | consteval field offsets |
| Strong types | FixedPrice, Qty, SeqNum |
| Coroutines | C++20 async session management |
| PMR | Pre-allocated memory pools |
| Cache-line alignment | `alignas(64)` for hot structures |
| Branch hints | `[[likely]]`/`[[unlikely]]` |
| Hot attributes | `[[gnu::hot]]` for critical functions |
| LTO | `-flto=auto` link-time optimization |

---

## FIX 5.0 / FIXT 1.1 Support

| Component | Status | Description |
|-----------|--------|-------------|
| FIXT 1.1 Session Messages | Supported | Logon, Logout, Heartbeat, TestRequest, ResendRequest, SequenceReset, Reject |
| FIX 5.0 Application Messages | Supported | NewOrderSingle, ExecutionReport, OrderCancelRequest, OrderCancelReject |
| ApplVerID (tag 1128) | Supported | Per-message application version |
| DefaultApplVerID (tag 1137) | Supported | In FIXT 1.1 Logon message |
| Version detection | Supported | `is_fixt11()`, `is_fix44()`, `is_fix4()` methods |

---

## Directory Structure

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
│   └── vs_quickfix/      # QuickFIX comparison
├── tests/                # Unit tests (Catch2)
├── examples/             # Simple client example
└── docs/
    ├── design/           # Design documents
    └── compare/          # Benchmark comparison reports
```
