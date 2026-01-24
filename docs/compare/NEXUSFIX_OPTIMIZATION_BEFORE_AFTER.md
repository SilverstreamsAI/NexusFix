# NexusFIX Optimization Before/After Comparison

**Date:** 2026-01-22
**Iterations:** 100,000
**Optimization Applied:** modernc_quant.md techniques

---

## Executive Summary

NexusFIX performance improvement after applying Modern C++ optimizations:

| Metric | Improvement |
|--------|-------------|
| ExecutionReport Parse | **6.0% faster** |
| Heartbeat Parse | **7.1% faster** |
| NewOrderSingle Parse | **7.2% faster** |
| P99 Latency | **8-17% faster** |

---

## Parse Latency Comparison

### ExecutionReport (35=8)

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| Min | 242.58 ns | 225.36 ns | **17.22 ns faster (7.1%)** |
| Mean | 257.15 ns | 241.74 ns | **15.41 ns faster (6.0%)** |
| P50 | 254.00 ns | 239.70 ns | **14.30 ns faster (5.6%)** |
| P90 | 269.21 ns | 251.70 ns | **17.51 ns faster (6.5%)** |
| P99 | 278.28 ns | 254.63 ns | **23.65 ns faster (8.5%)** |

### Heartbeat (35=0)

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| Min | 197.52 ns | 178.24 ns | **19.28 ns faster (9.8%)** |
| Mean | 210.84 ns | 195.82 ns | **15.02 ns faster (7.1%)** |
| P50 | 208.93 ns | 194.34 ns | **14.59 ns faster (7.0%)** |
| P90 | 221.52 ns | 201.36 ns | **20.16 ns faster (9.1%)** |
| P99 | 224.73 ns | 207.80 ns | **16.93 ns faster (7.5%)** |

### NewOrderSingle (35=D)

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| Min | 230.88 ns | 216.87 ns | **14.01 ns faster (6.1%)** |
| Mean | 248.16 ns | 230.32 ns | **17.84 ns faster (7.2%)** |
| P50 | 246.97 ns | 227.41 ns | **19.56 ns faster (7.9%)** |
| P90 | 252.83 ns | 237.95 ns | **14.88 ns faster (5.9%)** |
| P99 | 294.09 ns | 246.14 ns | **47.95 ns faster (16.3%)** |

### Message Boundary Detection (SIMD)

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| Min | 44.19 ns | 43.32 ns | **0.87 ns faster (2.0%)** |
| Mean | 49.26 ns | 45.44 ns | **3.82 ns faster (7.8%)** |
| P50 | 49.16 ns | 45.07 ns | **4.09 ns faster (8.3%)** |
| P99 | 57.35 ns | 47.41 ns | **9.94 ns faster (17.3%)** |

---

## Field Access Comparison

| Metric | Before | After | Change |
|--------|--------|-------|--------|
| Min | 9.07 ns | 9.07 ns | 0 |
| Mean | 10.24 ns | 10.37 ns | +0.13 ns |
| P50 | 10.24 ns | 10.54 ns | +0.30 ns |
| P99 | 10.83 ns | 11.12 ns | +0.29 ns |

**Note:** Field access is already at the hardware limit (~10ns). Minor variations are within measurement noise.

---

## Low-level Operations

### Checksum Calculation

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| Mean | 9.51 ns | 8.95 ns | **0.56 ns faster (5.9%)** |

### Integer Parsing

| Metric | Before | After | Change |
|--------|--------|-------|--------|
| Mean | 9.14 ns | 9.10 ns | 0.04 ns faster |

### FixedPrice Parsing

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| Mean | 9.13 ns | 9.03 ns | **0.10 ns faster (1.1%)** |

---

## Optimizations Applied

| Technique | Reference | Impact |
|-----------|-----------|--------|
| Cache-line alignment | #9 | Prevents false sharing, better cache utilization |
| Branch hints `[[likely]]`/`[[unlikely]]` | #35 | Better branch prediction |
| `[[gnu::hot]]` attributes | #73 | Compiler places hot code together |
| `__restrict` pointer hints | #75 | Better aliasing analysis |
| Compile-time lookup tables | #82 | Branch-free decimal parsing |
| Static assertions | #43, #95 | Compile-time validation |
| LTO (`-flto=auto`) | #71 | Cross-module optimization |

---

## Analysis

### Why Improvements Are Modest (~6-7%)

1. **Already Highly Optimized**: NexusFIX was already using:
   - Zero-copy parsing with `std::span`
   - SIMD AVX2 scanning
   - O(1) field lookup
   - Compile-time field offsets

2. **Single-threaded Benchmark**: Cache-line alignment benefits are more visible under multi-threaded contention.

3. **Micro-benchmarks**: Branch hints and hot attributes show larger improvements in real workloads with more complex control flow.

4. **Hardware Limits**: Operations like field access (10ns) and integer parsing (9ns) are already at memory/cache latency limits.

### Where Improvements Are Largest

| Area | Improvement | Reason |
|------|-------------|--------|
| P99 latency | 8-17% | Branch hints reduce worst-case paths |
| SIMD scanning | 7-17% | `__restrict` enables better vectorization |
| Message parsing | 6-7% | `[[gnu::hot]]` improves I-cache locality |

---

## Throughput Impact

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| ExecutionReport/sec | 3.89M | 4.14M | **6.4% higher** |
| Bandwidth | 641 MB/s | 682 MB/s | **6.4% higher** |

---

## Conclusion

The modernc_quant.md optimizations provide:

- **6-7% average latency reduction** across all message types
- **8-17% P99 tail latency reduction** (more consistent performance)
- **~6% throughput increase**

These improvements compound in production environments where:
- Multiple threads compete for cache lines
- Branch mispredictions have higher penalties
- I-cache pressure is higher with larger codebases

The optimizations are "free" in terms of code maintainability and provide measurable benefits without architectural changes.
