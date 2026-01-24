# NexusFIX Optimized Benchmark Comparison Report

**Date:** 2026-01-22
**Iterations:** 100,000
**System:** Linux, GCC 13.3.0
**Optimization Applied:** modernc_quant.md techniques

---

## Executive Summary

After applying Modern C++ optimizations from modernc_quant.md, NexusFIX maintains significant performance improvements over QuickFIX:

| Metric | Improvement |
|--------|-------------|
| Parse Latency | **3.0x faster** |
| Field Access | **2.9x faster** |
| Throughput | **3.5x higher** |

---

## Parse Latency Comparison (Post-Optimization)

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
| QuickFIX | 1.19M msg/sec | 191 MB/sec |
| NexusFIX | 4.17M msg/sec | 687 MB/sec |
| **Speedup** | **3.5x** | **3.6x** |

---

## FIX 5.0 / FIXT 1.1 Performance

NexusFIX supports both FIX 4.4 and FIX 5.0 with minimal overhead:

### ExecutionReport Parse Latency

| Metric | FIX 4.4 | FIX 5.0 | Overhead |
|--------|---------|---------|----------|
| Mean | 232 ns | 237 ns | +5 ns (+2%) |
| P50 | 232 ns | 236 ns | +4 ns (+2%) |
| P99 | 244 ns | 248 ns | +4 ns (+2%) |

### Version Detection Performance

| Operation | Latency |
|-----------|---------|
| 3 version checks | 9 ns |

---

## Optimizations Applied

The following modernc_quant.md techniques were applied:

### 1. Cache-Line Alignment (#9)
```cpp
struct alignas(CACHE_LINE_SIZE) SohPositions { ... };
class alignas(CACHE_LINE_SIZE) FieldTable { ... };
class alignas(CACHE_LINE_SIZE) IndexedParser { ... };
```

### 2. Branch Hints (#35)
```cpp
if (data[i] == fix::SOH) [[unlikely]] { ... }
for (size_t i = 0; i < n; ++i) [[likely]] { ... }
```

### 3. Hot/Cold Attributes (#73)
```cpp
[[nodiscard]] [[gnu::hot]]
inline SohPositions scan_soh_avx2(std::span<const char> data) noexcept;
```

### 4. __restrict Pointer Hints (#75)
```cpp
const char* __restrict ptr = data.data();
```

### 5. Compile-Time Lookup Tables (#82)
```cpp
static constexpr int64_t POW10[] = {100000000LL, 10000000LL, ...};
fractional_part *= POW10[fractional_digits];  // Branch-free
```

### 6. Static Assertions (#43, #95)
```cpp
static_assert(alignof(SohPositions) >= CACHE_LINE_SIZE);
static_assert(sizeof(FixedPrice) == 8);
```

### 7. LTO Optimization (#71)
```cmake
add_compile_options(-flto=auto -fdevirtualize-at-ltrans -fipa-pta)
add_link_options(-flto=auto -fuse-linker-plugin)
```

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
- NexusFIX: `consteval` field offsets, lookup tables
- QuickFIX: Runtime field parsing

### 5. No Dynamic Memory Allocation
- NexusFIX: Stack-allocated field tables, PMR pools
- QuickFIX: Heap allocations for each message

### 6. Modern C++ Optimizations
- Cache-line aligned hot data structures
- Branch prediction hints on all critical paths
- `[[gnu::hot]]` attributes on frequently called functions
- `__restrict` pointer hints for better aliasing analysis

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

## Benchmark Summary

| Message Type | QuickFIX | NexusFIX | Speedup |
|--------------|----------|----------|---------|
| ExecutionReport | 730 ns | 246 ns | **3.0x** |
| NewOrderSingle | 661 ns | 229 ns | **2.9x** |
| Heartbeat | 312 ns | 203 ns | **1.5x** |
| Field Access | 31 ns | 11 ns | **2.9x** |
| **Throughput** | 1.19M/s | 4.17M/s | **3.5x** |

---

## Conclusion

NexusFIX with Modern C++ optimizations achieves:

- **3.0x faster** ExecutionReport parsing (730ns -> 246ns)
- **2.9x faster** field access (31ns -> 11ns)
- **3.5x higher** throughput (1.2M -> 4.2M msg/sec)
- **FIX 5.0 support** with only 2% overhead vs FIX 4.4

These improvements translate directly to lower trading latency and higher system capacity for quantitative trading applications.

---

## Raw Benchmark Output

See:
- `optimized_nexusfix_benchmark_output.txt` - NexusFIX benchmark results
- `quickfix_benchmark_output.txt` - QuickFIX benchmark results
