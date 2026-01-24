# NexusFIX vs QuickFIX Performance Comparison Report

**Date:** 2026-01-22
**Iterations:** 100,000
**System:** Linux, GCC 13.3.0
**Optimization:** modernc_quant.md techniques applied (2026-01-22)

---

## Executive Summary

NexusFIX demonstrates significant performance improvements over QuickFIX:

| Metric | Improvement |
|--------|-------------|
| Parse Latency | **2.9-3.0x faster** |
| Field Access | **2.9x faster** |
| Throughput | **3.5x higher** |

---

## Parse Latency Comparison

### ExecutionReport (35=8) - Most Common Trading Message

| Metric | QuickFIX | NexusFIX | Speedup |
|--------|----------|----------|---------|
| Min | 676 ns | 226 ns | **3.0x** |
| Mean | 730 ns | 246 ns | **3.0x** |
| P50 | 723 ns | 246 ns | **2.9x** |
| P90 | 761 ns | 254 ns | **3.0x** |
| P99 | 784 ns | 258 ns | **3.0x** |

### NewOrderSingle (35=D) - Order Entry Message

| Metric | QuickFIX | NexusFIX | Speedup |
|--------|----------|----------|---------|
| Min | 586 ns | 208 ns | **2.8x** |
| Mean | 661 ns | 229 ns | **2.9x** |
| P50 | 651 ns | 227 ns | **2.9x** |
| P99 | 1028 ns | 244 ns | **4.2x** |

### Heartbeat (35=0) - Session Admin Message

| Metric | QuickFIX | NexusFIX | Speedup |
|--------|----------|----------|---------|
| Min | 281 ns | 185 ns | **1.5x** |
| Mean | 312 ns | 203 ns | **1.5x** |
| P50 | 308 ns | 203 ns | **1.5x** |
| P99 | 344 ns | 213 ns | **1.6x** |

---

## Field Access Comparison

Accessing 4 fields (OrderID, ExecID, Side, MsgType):

| Metric | QuickFIX | NexusFIX | Speedup |
|--------|----------|----------|---------|
| Min | 28 ns | 10 ns | **2.8x** |
| Mean | 31 ns | 11 ns | **2.9x** |
| P50 | 31 ns | 11 ns | **2.8x** |
| P99 | 46 ns | 14 ns | **3.3x** |

**Note:** NexusFIX uses O(1) hash table lookup vs QuickFIX's O(log n) tree-based lookup.

---

## Throughput Comparison

| Engine | Throughput | Bandwidth |
|--------|------------|-----------|
| QuickFIX | 1.19M msg/sec | 192 MB/sec |
| NexusFIX | 4.17M msg/sec | 687 MB/sec |
| **Speedup** | **3.5x** | **3.6x** |

---

## Why NexusFIX is Faster

### 1. Zero-Copy Parsing
- NexusFIX: `std::span<const char>` views into original buffer
- QuickFIX: Copies data into `std::string` objects

### 2. O(1) Field Lookup
- NexusFIX: Direct array indexing by tag number
- QuickFIX: `std::map` tree traversal (O(log n))

### 3. SIMD-Accelerated Scanning
- NexusFIX: AVX2 instructions for SOH delimiter detection
- QuickFIX: Byte-by-byte scanning

### 4. Compile-Time Optimization
- NexusFIX: `consteval` field offsets, no runtime calculations
- QuickFIX: Runtime field parsing

### 5. No Dynamic Memory Allocation
- NexusFIX: Stack-allocated field tables, PMR pools
- QuickFIX: Heap allocations for each message

### 6. Modern C++ Optimizations (modernc_quant.md)
- Cache-line aligned hot data structures (`alignas(64)`)
- Branch prediction hints (`[[likely]]`/`[[unlikely]]`)
- `[[gnu::hot]]` attributes on critical functions
- `__restrict` pointer hints for aliasing optimization
- Compile-time lookup tables for decimal parsing
- Link-Time Optimization (LTO) with `-flto=auto`

---

## Test Environment

```
CPU: ~3.4 GHz (calibrated via RDTSC)
OS: Linux
Compiler: GCC 13.3.0
NexusFIX: C++23, -O3 -march=native -flto=auto
QuickFIX: C++14, -O3 -march=native
```

---

## How to Reproduce

```bash
# Build
./start.sh build

# Run QuickFIX benchmark
./start.sh compare 100000

# Run NexusFIX benchmark
./start.sh bench 100000
```

---

## FIX 4.4 vs FIX 5.0 / FIXT 1.1 Comparison

NexusFIX supports both FIX 4.4 and FIX 5.0 (with FIXT 1.1 transport) with minimal performance overhead.

### ExecutionReport Parse Latency

| Metric | FIX 4.4 | FIX 5.0 | Diff |
|--------|---------|---------|------|
| Mean | 232 ns | 237 ns | +5 ns (+2%) |
| P50 | 232 ns | 236 ns | +4 ns (+2%) |
| P99 | 244 ns | 248 ns | +4 ns (+2%) |

### FIXT 1.1 Session Messages

| Message Type | Mean Latency |
|--------------|-------------|
| Logon (A) | 211 ns |
| Heartbeat (0) | 197 ns |

### FIX 5.0 Application Messages

| Message Type | Mean Latency |
|--------------|-------------|
| ExecutionReport (8) | 255 ns |
| NewOrderSingle (D) | 235 ns |

### Version Detection Performance

| Operation | Latency |
|-----------|---------|
| 3 version checks | 9 ns |

**Key Findings:**
- FIX 5.0 has only ~2% overhead vs FIX 4.4 (due to extra ApplVerID field)
- Version detection is essentially free (~9 ns for 3 checks)
- FIXT 1.1 session messages parse faster than FIX 4.4 (simpler structure)

---

## Conclusion

NexusFIX with Modern C++ optimizations achieves its design goal of being significantly faster than QuickFIX:

- **3.0x faster** ExecutionReport parsing (730ns -> 246ns)
- **2.9x faster** field access (31ns -> 11ns)
- **3.5x higher** throughput (1.2M -> 4.2M msg/sec)
- **FIX 5.0 support** with only 2% overhead vs FIX 4.4

### Optimizations Applied (modernc_quant.md)

| Technique | Reference | Impact |
|-----------|-----------|--------|
| Cache-line alignment | #9 | Prevents false sharing |
| Branch hints | #35 | Better branch prediction |
| `[[gnu::hot]]` | #73 | Compiler optimization hints |
| `__restrict` | #75 | Better aliasing analysis |
| Lookup tables | #82 | Branch-free decimal parsing |
| LTO | #71 | Cross-module optimization |
| Static assertions | #43, #95 | Compile-time validation |

These improvements translate directly to lower trading latency and higher system capacity for quantitative trading applications.

---

## Related Documents

- `OPTIMIZED_BENCHMARK_COMPARISON.md` - Detailed optimization report
- `FIX44_VS_FIX50_COMPARISON.md` - FIX version comparison
- `optimized_nexusfix_benchmark_output.txt` - Raw NexusFIX output
- `quickfix_benchmark_output.txt` - Raw QuickFIX output
