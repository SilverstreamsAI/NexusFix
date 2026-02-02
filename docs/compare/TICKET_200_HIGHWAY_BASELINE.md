# TICKET_200: Highway Integration - Baseline Performance

**Date**: 2025-01-29
**CPU**: 3.418 GHz (x86_64)
**SIMD**: AVX2 Available, AVX-512 Not Available

---

## Checksum Benchmark (Before Highway)

| Message Size | Method | Latency | Cycles | Throughput |
|--------------|--------|---------|--------|------------|
| 64 bytes (Heartbeat) | Scalar | 11.7 ns | 40 cyc | 5.5 GB/s |
| 64 bytes (Heartbeat) | AVX2 | 7.9 ns | 27 cyc | 8.1 GB/s |
| 64 bytes (Heartbeat) | Auto | 7.6 ns | 26 cyc | 8.4 GB/s |
| 256 bytes (NewOrderSingle) | Scalar | 21.9 ns | 75 cyc | 11.7 GB/s |
| 256 bytes (NewOrderSingle) | AVX2 | 10.5 ns | 36 cyc | 24.3 GB/s |
| 256 bytes (NewOrderSingle) | Auto | 9.9 ns | 34 cyc | 25.7 GB/s |
| 1024 bytes (ExecutionReport) | Scalar | 62.0 ns | 212 cyc | 16.5 GB/s |
| 1024 bytes (ExecutionReport) | AVX2 | 15.2 ns | 52 cyc | 67.3 GB/s |
| 1024 bytes (ExecutionReport) | Auto | 15.5 ns | 53 cyc | 66.0 GB/s |

---

## Parse Benchmark (Before Highway)

| Operation | Min | Mean | P50 | P99 | P99.9 |
|-----------|-----|------|-----|-----|-------|
| IndexedParser (ExecutionReport) | 193.13 ns | 198.18 ns | 197.23 ns | 203.67 ns | 217.71 ns |
| Field Access (4 fields) | 12.58 ns | 13.93 ns | 13.75 ns | 17.56 ns | 17.85 ns |
| Message Boundary Detection | 66.13 ns | 67.63 ns | 67.30 ns | 69.35 ns | 84.86 ns |
| Heartbeat Parse | 153.34 ns | 162.23 ns | 161.53 ns | 169.72 ns | 218.59 ns |
| NewOrderSingle Parse | 185.23 ns | 192.55 ns | 191.67 ns | 199.57 ns | 236.15 ns |
| Checksum Calculation | 10.24 ns | 11.84 ns | 11.71 ns | 14.92 ns | 15.51 ns |

---

## Current Implementation

- `simd_checksum.hpp`: SSE2/AVX2/AVX-512 intrinsics
- `simd_scanner.hpp`: AVX2/AVX-512 intrinsics
- Compile-time dispatch via `#if defined(__AVX2__)`
