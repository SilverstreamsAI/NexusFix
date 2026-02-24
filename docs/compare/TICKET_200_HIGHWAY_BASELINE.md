# TICKET_200: Highway SIMD Analysis - Current xsimd Baseline

**Date**: 2026-02-12 (updated from 2025-01-29)
**CPU**: 3.418 GHz (x86_64)
**SIMD**: AVX2 Available, AVX-512 Not Available
**Implementation**: xsimd 13.2.0 arch-templated (TICKET_212)
**Iterations**: 100,000
**Timing**: rdtsc_vm_safe (lfence serialized)

---

## Context

TICKET_200 evaluated Google Highway v1.3.0 as a potential replacement for our SIMD abstraction layer. After adopting xsimd in TICKET_212 (2026-02-06), the Highway evaluation concluded that xsimd meets all current requirements. This baseline documents the current xsimd performance for reference.

---

## 1. Checksum Benchmark (xsimd AVX2)

| Message Size | Method | Latency | Cycles | Throughput |
|--------------|--------|---------|--------|------------|
| 64 bytes (Heartbeat) | Scalar | 11.4 ns | 39 cyc | 5.6 GB/s |
| 64 bytes (Heartbeat) | AVX2 | 7.6 ns | 26 cyc | 8.4 GB/s |
| 64 bytes (Heartbeat) | Auto | 7.3 ns | 25 cyc | 8.7 GB/s |
| 256 bytes (NewOrderSingle) | Scalar | 21.4 ns | 73 cyc | 12.0 GB/s |
| 256 bytes (NewOrderSingle) | AVX2 | 10.2 ns | 35 cyc | 25.0 GB/s |
| 256 bytes (NewOrderSingle) | Auto | 9.9 ns | 34 cyc | 25.7 GB/s |
| 1024 bytes (ExecutionReport) | Scalar | 60.9 ns | 208 cyc | 16.8 GB/s |
| 1024 bytes (ExecutionReport) | AVX2 | 14.9 ns | 51 cyc | 68.6 GB/s |
| 1024 bytes (ExecutionReport) | Auto | 15.2 ns | 52 cyc | 67.3 GB/s |

---

## 2. SOH Scanner Benchmark (xsimd AVX2)

| Buffer Size | Scalar (P50) | AVX2 (P50) | AVX2 Throughput | Speedup |
|-------------|-------------|------------|-----------------|---------|
| 64 bytes | 20.1 ns | 7.3 ns | 69.81 Gbps | 2.82x |
| 128 bytes | 61.2 ns | 7.7 ns | 129.86 Gbps | 7.64x |
| 256 bytes | 81.9 ns | 10.6 ns | 192.19 Gbps | 7.81x |
| 512 bytes | 137.6 ns | 17.3 ns | 236.55 Gbps | 8.40x |
| 1024 bytes | 275.8 ns | 25.5 ns | 319.41 Gbps | 12.29x |
| 2048 bytes | 600.7 ns | 61.2 ns | 273.23 Gbps | 10.86x |
| 4096 bytes | 1502.0 ns | 123.2 ns | 267.46 Gbps | 12.82x |
| 8192 bytes | 2063.1 ns | 141.0 ns | 472.54 Gbps | 15.37x |

---

## 3. Structural Index Benchmark (xsimd AVX2)

### Stage 1: build_index (ExecutionReport ~169 bytes)

| Metric | Scalar | AVX2 |
|--------|--------|------|
| Min | 7.32 ns | 42.72 ns |
| Mean | 8.63 ns | 46.72 ns |
| P50 | 8.49 ns | 46.23 ns |
| P90 | 9.07 ns | 48.57 ns |
| P99 | 10.53 ns | 59.98 ns |
| P99.9 | 11.41 ns | 62.62 ns |

Note: Scalar build_index is faster for small messages because it avoids SIMD setup overhead. The AVX2 path performs full structural indexing (SOH + equals scanning + position extraction) vs scalar which uses a simpler loop.

### Stage 2: Field Extraction

| Operation | P50 | P99 |
|-----------|-----|-----|
| Field by tag (4 fields) | 84.56 ns | 86.32 ns |
| Field by index (4 fields) | 8.78 ns | 9.66 ns |

### Full Pipeline (build_index + extract 4 fields)

| Metric | Value |
|--------|-------|
| Mean | 118.90 ns |
| P50 | 116.46 ns |
| P99 | 137.53 ns |

### Message Size Scaling

| Message | Size | P50 | P99 | Throughput |
|---------|------|-----|-----|------------|
| Heartbeat | 78B | 36.6 ns | 51.5 ns | 2.13 GB/s |
| NewOrderSingle | 158B | 50.9 ns | 71.1 ns | 3.10 GB/s |
| ExecutionReport | 169B | 47.4 ns | 60.9 ns | 3.57 GB/s |

### Performance Targets (TICKET_208)

| Target | Result | Status |
|--------|--------|--------|
| SOH scanning < 100 ns | 46.2 ns | **PASS** |
| Field indexing < 50 ns | 46.2 ns | **PASS** |
| Per-field extract < 20 ns | 2.2 ns | **PASS** |
| Speedup vs IndexedParser | 4.2x | **PASS** |

---

## 4. Current Implementation

- `simd_checksum.hpp`: xsimd arch-templated (`detail::checksum_xsimd<Arch>`)
- `simd_scanner.hpp`: xsimd arch-templated (`detail::scan_soh_xsimd<Arch>`, etc.)
- `structural_index.hpp`: xsimd arch-templated with runtime dispatch
- Compile-time dispatch: `#if defined(NFX_AVX512_CHECKSUM)` > `NFX_AVX2_CHECKSUM` > `NFX_SSE2_CHECKSUM`
- Fallback: raw intrinsics when `NFX_HAS_XSIMD=0`
