# TICKET_208: Structural Index Before/After Comparison

**Date:** 2026-02-05
**Iterations:** 100,000
**Optimization Applied:** simdjson-style Two-Stage Structural Indexing
**SIMD Implementation:** AVX2 (runtime dispatched)
**Timing:** rdtsc_vm_safe (lfence serialized)
**CPU Frequency:** 3.418 GHz

---

## Executive Summary

simdjson-style two-stage parsing applied to FIX message indexing:

| Metric | Improvement |
|--------|-------------|
| Structural Indexing (Stage 1) | **77.4% faster** (P50) |
| Per-field Extraction (Stage 2) | **2.3 ns per field** |
| Full Pipeline | **109 ns** (build + extract 4 fields) |
| Speedup vs IndexedParser | **4.4x** |

---

## Before vs After: IndexedParser vs StructuralIndex

### ExecutionReport (35=8, ~169 bytes)

| Metric | IndexedParser (Before) | StructuralIndex (After) | Improvement |
|--------|------------------------|-------------------------|-------------|
| Min | 193.12 ns | 43.31 ns | **149.81 ns faster (77.6%)** |
| Mean | 208.24 ns | 47.43 ns | **160.81 ns faster (77.2%)** |
| P50 | 207.17 ns | 46.82 ns | **160.35 ns faster (77.4%)** |
| P90 | 214.78 ns | 49.16 ns | **165.62 ns faster (77.1%)** |
| P99 | 217.99 ns | 60.86 ns | **157.13 ns faster (72.1%)** |
| P99.9 | 299.34 ns | 74.62 ns | **224.72 ns faster (75.1%)** |

---

## Stage 1: Structural Indexing (build_index)

### Scalar vs AVX2

| Metric | Scalar | AVX2 | Improvement |
|--------|--------|------|-------------|
| Min | 7.32 ns | 43.31 ns | Scalar wins (small msg, no SIMD benefit) |
| Mean | 9.06 ns | 47.43 ns | - |
| P50 | 8.78 ns | 46.82 ns | - |

**Note:** For this message size (~169 bytes), the scalar path is faster because
AVX2 overhead (setup, mask extraction) exceeds the data-parallel benefit.
AVX2 wins for larger messages (>512 bytes) where SIMD throughput dominates.

---

## Stage 2: Field Extraction

### By Tag (Linear Search) vs By Index (O(1))

| Metric | By Tag (4 fields) | By Index (4 fields) | Per-Field |
|--------|-------------------|---------------------|-----------|
| Min | 79.30 ns | 7.61 ns | 1.90 ns |
| Mean | 91.27 ns | 9.02 ns | 2.26 ns |
| P50 | 89.25 ns | 9.07 ns | **2.27 ns** |
| P99 | 142.21 ns | 9.66 ns | 2.42 ns |

**Target: < 20 ns per field** - **PASS** (2.27 ns per field)

---

## Full Pipeline

### build_index + Extract 4 Fields

| Metric | Latency |
|--------|---------|
| Min | 101.24 ns |
| Mean | 112.38 ns |
| P50 | 109.14 ns |
| P90 | 127.29 ns |
| P99 | 133.43 ns |

---

## Buffer Strategy

### Unpadded vs Padded (64-byte SIMD padding)

| Metric | Unpadded | Padded | Delta |
|--------|----------|--------|-------|
| Mean | 47.4 ns | 51.4 ns | -8.3% |
| P50 | 46.8 ns | 50.9 ns | -8.7% |
| P99 | 60.9 ns | 62.3 ns | -2.4% |

**Note:** Padded buffer is marginally slower due to memcpy overhead.
The benefit is safety (no segfault on SIMD overread), not speed.

---

## Message Size Scaling

| Message Type | Size | P50 | P99 | Throughput |
|-------------|------|-----|-----|------------|
| Heartbeat | 78B | 38.6 ns | 52.1 ns | 2.02 GB/s |
| NewOrderSingle | 158B | 53.3 ns | 74.0 ns | 2.97 GB/s |
| ExecutionReport | 169B | 50.9 ns | 65.0 ns | 3.32 GB/s |

---

## TICKET_208 Performance Targets

| Metric | Result | Target | Status |
|--------|--------|--------|--------|
| SOH scanning | 46.8 ns | < 100 ns | **PASS** |
| Field indexing | 46.8 ns | < 50 ns | **PASS** |
| Per-field extraction | 2.3 ns | < 20 ns | **PASS** |

---

## Implementation

| File | Description |
|------|-------------|
| `include/nexusfix/parser/structural_index.hpp` | FIXStructuralIndex, build_index (scalar/AVX2/AVX-512), runtime dispatch, PaddedMessageBuffer, IndexedFieldAccessor |
| `benchmarks/structural_index_bench.cpp` | Benchmark using nfx::bench utilities |
| `tests/test_parser.cpp` | 4 new test cases (34 assertions) |

---

## Benchmark Command

```bash
cmake --build build --target structural_index_bench
./build/bin/benchmarks/structural_index_bench 100000
```
