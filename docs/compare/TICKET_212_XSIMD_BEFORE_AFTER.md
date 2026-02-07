# TICKET_212: xsimd Portable SIMD Before/After Comparison

**Date:** 2026-02-06
**Iterations:** 100,000
**Change Applied:** Replace raw x86 intrinsics with xsimd portable abstractions
**SIMD Implementation:** AVX2 (runtime dispatched via xsimd)
**Timing:** rdtsc_vm_safe (lfence serialized)
**CPU Frequency:** 3.418 GHz
**xsimd Version:** 13.2.0

---

## Executive Summary

TICKET_212 replaces raw `<immintrin.h>` intrinsics with xsimd arch-templated functions across all SIMD code paths. The goal is ARM NEON portability with zero performance regression on x86.

| Component | Before (raw intrinsics) | After (xsimd) | Delta |
|-----------|------------------------|----------------|-------|
| Structural Index AVX2 (P50) | 46.82 ns | 45.65 ns | **-2.5% (within noise)** |
| Structural Index AVX2 (P99) | 60.86 ns | 59.69 ns | **-1.9% (within noise)** |
| Checksum AVX2 64B (P50) | 7.6 ns | 7.6 ns | **0% (identical)** |
| Checksum AVX2 256B (P50) | 10.2 ns | 10.5 ns | **+2.9% (within noise)** |
| Checksum AVX2 1024B (P50) | 14.9 ns | 15.2 ns | **+2.0% (within noise)** |
| Correctness | 471/471 tests | 471/471 tests | **100% pass** |

**Conclusion:** Zero performance regression. All deltas within measurement noise (+/- 3%).

---

## Structural Index: build_index AVX2

### Before (TICKET_208, raw intrinsics) vs After (TICKET_212, xsimd)

ExecutionReport (~169 bytes):

| Metric | Before (raw) | After (xsimd) | Delta |
|--------|-------------|----------------|-------|
| Min | 43.31 ns | 42.72 ns | -1.4% |
| Mean | 47.43 ns | 46.39 ns | -2.2% |
| P50 | 46.82 ns | 45.65 ns | -2.5% |
| P90 | 49.16 ns | 47.99 ns | -2.4% |
| P99 | 60.86 ns | 59.69 ns | -1.9% |
| P99.9 | 74.62 ns | 62.62 ns | -16.1% |

Scalar baseline (unchanged, expected):

| Metric | Before | After | Delta |
|--------|--------|-------|-------|
| P50 | 8.78 ns | 8.78 ns | 0% |

### Full Pipeline (build + extract 4 fields)

| Metric | Before | After | Delta |
|--------|--------|-------|-------|
| Mean | 112.38 ns | 110.49 ns | -1.7% |
| P50 | 109.14 ns | 106.51 ns | -2.4% |
| P99 | 149.82 ns | 147.48 ns | -1.6% |

### Message Size Scaling (After, xsimd)

| Message | Size | P50 | P99 | Throughput |
|---------|------|-----|-----|------------|
| Heartbeat | 78B | 36.0 ns | 52.1 ns | 2.17 GB/s |
| NewOrderSingle | 158B | 51.5 ns | 80.5 ns | 3.07 GB/s |
| ExecutionReport | 169B | 46.8 ns | 60.3 ns | 3.61 GB/s |

---

## Checksum: AVX2

### Before (raw intrinsics SAD) vs After (xsimd uint8_t accumulation)

**Implementation change:** Raw intrinsics used `_mm256_sad_epu8` (SAD instruction) for widened 64-bit accumulation. xsimd uses `uint8_t` lane accumulation with `reduce_add`, relying on modular arithmetic (FIX checksum = sum mod 256).

#### Small Message (64 bytes - Heartbeat)

| Method | Before | After | Delta |
|--------|--------|-------|-------|
| Scalar | 11.4 ns / 5.6 GB/s | 11.4 ns / 5.6 GB/s | 0% |
| AVX2 | 7.6 ns / 8.4 GB/s | 7.6 ns / 8.4 GB/s | 0% |

#### Medium Message (256 bytes - NewOrderSingle)

| Method | Before | After | Delta |
|--------|--------|-------|-------|
| Scalar | 21.7 ns / 11.8 GB/s | 21.9 ns / 11.7 GB/s | +0.9% |
| AVX2 | 10.2 ns / 25.0 GB/s | 10.5 ns / 24.3 GB/s | +2.9% |

#### Large Message (1024 bytes - ExecutionReport)

| Method | Before | After | Delta |
|--------|--------|-------|-------|
| Scalar | 61.2 ns / 16.7 GB/s | 62.0 ns / 16.5 GB/s | +1.3% |
| AVX2 | 14.9 ns / 68.6 GB/s | 15.2 ns / 67.3 GB/s | +2.0% |

### Correctness Verification

| Check | Result |
|-------|--------|
| Scalar == xsimd AVX2 | YES (checksum = 65) |
| All 471 test assertions | PASS |
| Scalar fallback (NFX_SIMD_IMPL=scalar) | PASS |

---

## SIMD Scanner: AVX2

Scanner functions (`scan_soh`, `find_soh`, `find_equals`, `count_soh`) are exercised indirectly through the structural index and parser tests. All 44 SIMD-specific assertions and 34 structural index assertions pass identically.

---

## What Changed

### Files Modified

| File | Change |
|------|--------|
| `CMakeLists.txt` | Added `NFX_ENABLE_XSIMD` option, FetchContent xsimd 13.2.0, `NFX_HAS_XSIMD=1` |
| `simd_scanner.hpp` | 4 arch-templated functions in `detail` namespace, named wrappers |
| `simd_checksum.hpp` | 1 arch-templated function in `detail` namespace, named wrappers |
| `structural_index.hpp` | 1 arch-templated function + unified `extract_positions` in `detail_idx` namespace |
| `simd_scanner_bench.cpp` | Added `NFX_HAS_XSIMD` define |

### Architecture

```
Before:  scan_soh_avx2()     -> raw __m256i intrinsics
         scan_soh_avx512()   -> raw __m512i intrinsics

After:   scan_soh_avx2()     -> detail::scan_soh_xsimd<xsimd::avx2>()
         scan_soh_avx512()   -> detail::scan_soh_xsimd<xsimd::avx512bw>()
         (future) scan_soh_neon() -> detail::scan_soh_xsimd<xsimd::neon64>()
```

### Key API Mapping

| Raw Intrinsic | xsimd Equivalent |
|---------------|-----------------|
| `_mm256_set1_epi8(x)` | `batch_t(static_cast<uint8_t>(x))` |
| `_mm256_loadu_si256(ptr)` | `xsimd::load_unaligned<batch_t>(ptr)` |
| `_mm256_load_si256(ptr)` | `xsimd::load_aligned<batch_t>(ptr)` |
| `_mm256_cmpeq_epi8(a, b)` | `a == b` |
| `_mm256_movemask_epi8(cmp)` | `cmp.mask()` |
| `__builtin_ctz(mask)` | `std::countr_zero(mask)` |
| `__builtin_popcount(mask)` | `std::popcount(mask)` |

### Fallback

When `NFX_HAS_XSIMD` is not defined (e.g., xsimd disabled in CMake), all files fall back to the original raw intrinsics code path. No functionality is lost.

---

## Performance Targets

| Target | Result | Status |
|--------|--------|--------|
| Zero regression vs raw intrinsics | All deltas within +/- 3% | **PASS** |
| All existing tests pass | 471/471 assertions | **PASS** |
| Scalar fallback works | Tested via NFX_SIMD_IMPL=scalar | **PASS** |
| ARM NEON portable (compile) | xsimd arch templates ready | **READY** |
