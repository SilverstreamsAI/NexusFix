# TICKET_210: mimalloc Memory Allocator Before/After Comparison

**Date:** 2026-02-06
**Iterations:** 100,000 (single alloc), 1,000 (burst/cleanup/isolation)
**Optimization Applied:** mimalloc PMR Integration + SessionHeap Combined Architecture
**Timing:** rdtsc_vm_safe (lfence serialized)
**CPU Frequency:** 3.418 GHz
**Message Size:** 256 bytes (typical FIX ExecutionReport)

---

## Executive Summary

TICKET_210 implements a combined memory allocation architecture where monotonic bump allocation sits on top of a mimalloc per-session heap:

| Metric | Improvement |
|--------|-------------|
| Burst allocation vs monotonic-only | **64% faster** (P50, 1000 msgs) |
| Burst allocation vs mimalloc-only | **5% faster** (P50, 1000 msgs) |
| Overflow graceful degradation | **9.2 ns/msg** (no bad_alloc) |
| Per-session isolation | **Yes** (mi_heap_t per session) |
| O(1) full cleanup | **Yes** (mi_heap_destroy) |
| P99 tail latency | **4.2 ns** (vs 14.3 ns monotonic, vs 4.4 ns mimalloc) |

---

## Isolation Benchmark: Controlled Strategy Comparison

Four strategies tested with **identical loop structure**, **isolated warmup**, and **identical compiler optimization**. No cross-test cache warming effects.

**Strategies:**
- **A: Monotonic-only** -- `std::vector<char>` 64MB + `monotonic_buffer_resource` + `null_memory_resource` (existing baseline)
- **B: mimalloc-only** -- `MimallocMemoryResource` per-alloc `mi_heap_malloc_aligned`
- **C: SessionHeap** -- monotonic bump over mimalloc-allocated buffer (PMR wrapper class)
- **D: mimalloc+mono direct** -- mimalloc buffer + `monotonic_buffer_resource` as separate local variables

### Per-Message Latency (P50 / P99)

| Burst Size | A: Monotonic-only | B: mimalloc-only | **C: SessionHeap** | D: mimalloc+mono direct |
|-----------|-------------------|-------------------|--------------------|-------------------------|
| 100 msgs | 12.2 / 16.2 ns | 4.6 / 5.8 ns | **4.0 / 4.2 ns** | 6.9 / 7.0 ns |
| 500 msgs | 11.4 / 16.2 ns | 4.2 / 5.2 ns | **4.1 / 4.2 ns** | 6.9 / 7.1 ns |
| 1,000 msgs | 11.4 / 14.3 ns | 4.3 / 4.4 ns | **4.1 / 4.2 ns** | 6.9 / 7.2 ns |
| 5,000 msgs | 10.7 / 13.8 ns | 4.5 / 4.8 ns | **4.3 / 4.7 ns** | 7.0 / 7.3 ns |
| 10,000 msgs | 6.5 / 11.3 ns | 5.6 / 6.9 ns | **5.1 / 5.5 ns** | 7.0 / 7.4 ns |

**Winner: SessionHeap (C) at every burst size.**

### Improvement vs Baseline (P50, 1000 msgs)

| Comparison | Before | After | Improvement |
|------------|--------|-------|-------------|
| SessionHeap vs Monotonic-only | 11.4 ns | 4.1 ns | **64.0% faster** |
| SessionHeap vs mimalloc-only | 4.3 ns | 4.1 ns | **4.7% faster** |
| SessionHeap vs mimalloc+mono direct | 6.9 ns | 4.1 ns | **40.6% faster** |

### Why Each Strategy Loses

| Strategy | Why It Loses to SessionHeap |
|----------|----------------------------|
| A: Monotonic-only | `std::vector<char>` 64MB allocation via glibc malloc; pages not cache-optimized by mimalloc |
| B: mimalloc-only | Per-allocation free-list lookup + page management overhead; no bump pointer |
| D: mimalloc+mono direct | Separate local variables prevent compiler from optimizing member layout as a single object |

---

## Single Allocation Latency (100,000 iterations)

| Metric | PMR Monotonic | mimalloc Heap | Delta |
|--------|--------------|---------------|-------|
| Min | 9.4 ns | 12.0 ns | +2.6 ns |
| Mean | 10.7 ns | 13.3 ns | +2.6 ns |
| P50 | 10.5 ns | 12.9 ns | +2.4 ns |
| P99 | 13.2 ns | 20.8 ns | +7.6 ns |
| P99.9 | 13.8 ns | 21.4 ns | +7.6 ns |
| Max | 2427.5 ns | 1823.5 ns | **-604 ns** |

**Analysis:** Single-allocation latency favors monotonic bump (~2.4 ns faster P50). However, burst allocation (the real-world pattern) reverses this due to cache locality. mimalloc has lower max latency (fewer extreme outliers).

---

## Deallocation Latency (mimalloc only)

| Metric | Individual Dealloc | Cross-Thread Dealloc |
|--------|-------------------|---------------------|
| Min | 11.4 ns | 21.1 ns |
| P50 | 13.2 ns | 22.5 ns |
| P99 | 16.4 ns | 42.4 ns |

**Analysis:** Capabilities that monotonic-only cannot provide. SessionHeap inherits these via its mimalloc upstream when individual deallocation or cross-thread free is needed.

---

## Cleanup Latency

| Metric | pool.release() | mi_heap_destroy() | Delta |
|--------|---------------|-------------------|-------|
| Min | 30.1 ns | 254.6 ns | +224.5 ns |
| P50 | 103.0 ns | 277.7 ns | +174.7 ns |
| P99 | 297.0 ns | 353.5 ns | +56.5 ns |

**Analysis:** `pool.release()` is a pointer reset (~100 ns). `mi_heap_destroy()` traverses pages (~280 ns) but provides true OS memory return and per-session isolation. Both are O(1) relative to allocation count.

---

## Overflow Graceful Degradation

| Metric | SessionHeap (4KB buffer, 100 allocs) |
|--------|-------------------------------------|
| P50 total | 917.3 ns |
| P99 total | 939.3 ns |
| **Per-msg P50** | **9.2 ns** (mixed bump + mimalloc) |

**Analysis:** When the initial buffer is exhausted, SessionHeap falls back to mimalloc upstream at 9.2 ns/msg instead of `std::bad_alloc`. This is the critical advantage over standalone monotonic with `null_memory_resource` upstream.

---

## Architecture

```
BEFORE (TICKET_209 and earlier):

  std::vector<char> pool_storage(64MB)     ← glibc malloc, no isolation
  monotonic_buffer_resource pool            ← bump alloc, overflow = bad_alloc
  └── upstream: null_memory_resource

  Hot path:  bump ~11 ns/msg (burst)
  Overflow:  std::bad_alloc (fatal)
  Cleanup:   pool.release() ~100 ns
  Isolation: none

AFTER (TICKET_210):

  SessionHeap (per-session, owns everything)
  ├── MimallocMemoryResource (mi_heap_t*)   ← per-session isolation
  │   └── allocates initial 64MB buffer      ← cache-aligned by mimalloc
  └── monotonic_buffer_resource              ← hot-path bump allocation
      ├── initial_buffer: 64MB from mimalloc
      └── upstream: MimallocMemoryResource   ← overflow to mimalloc

  Hot path:  bump ~4.1 ns/msg (burst)
  Overflow:  mimalloc fallback ~9.2 ns/msg
  Cleanup:   mi_heap_destroy() ~280 ns (releases everything)
  Isolation: per-session mi_heap_t
```

Destruction order guarantees safety:
1. `pool_` destroyed first -- releases overflow chunks to still-alive `heap_`
2. `heap_` destroyed last -- `mi_heap_destroy()` releases initial buffer + everything

---

## Capability Matrix

| Capability | A: Monotonic-only | B: mimalloc-only | **C: SessionHeap** |
|------------|:-----------------:|:----------------:|:------------------:|
| Burst alloc (P50, 1K msgs) | 11.4 ns | 4.3 ns | **4.1 ns** |
| Burst alloc (P99, 1K msgs) | 14.3 ns | 4.4 ns | **4.2 ns** |
| Individual deallocation | N/A | 13.2 ns | N/A (monotonic) |
| Cross-thread free | N/A | 22.5 ns | N/A (monotonic) |
| Per-session isolation | No | **Yes** | **Yes** |
| Overflow handling | bad_alloc | N/A | **9.2 ns fallback** |
| O(1) full cleanup | 103 ns | 278 ns | **278 ns** |
| MemoryMessageStore upstream | No | Yes | **Yes** |

---

## Files Changed

| File | Change |
|------|--------|
| `include/nexusfix/memory/mimalloc_resource.hpp` | Added `SessionHeap` class |
| `include/nexusfix/store/memory_message_store.hpp` | Added `upstream_resource` to Config |
| `tests/test_mimalloc.cpp` | Added 6 SessionHeap test cases |
| `benchmarks/mimalloc_bench.cpp` | Added Test 6 combined benchmark |
| `benchmarks/session_heap_isolation_bench.cpp` | Controlled isolation benchmark |
| `tests/CMakeLists.txt` | Abseil warning suppression for mimalloc_tests |
| `benchmarks/CMakeLists.txt` | Added isolation benchmark target |

### Backward Compatibility

- `MemoryMessageStore` default behavior unchanged (`upstream_resource = nullptr`)
- All 83 existing tests pass without modification
- New code guarded by `#if NFX_HAS_MIMALLOC`

---

## Test Results

| Test Suite | Result |
|------------|--------|
| nexusfix_tests (main) | **83/83 passed** (500,803 assertions) |
| mimalloc_tests | **13/13 passed** (10,501 assertions) |

---

## Conclusion

SessionHeap is the optimal allocation strategy for NexusFix hot-path message processing:

1. **Fastest burst allocation**: 4.1 ns/msg (64% faster than monotonic-only, 5% faster than mimalloc-only)
2. **Flat P99 tail**: 4.2 ns (vs 14.3 ns monotonic-only)
3. **Overflow safety**: Graceful fallback to mimalloc at 9.2 ns/msg instead of `bad_alloc`
4. **Session isolation**: Each session owns its own `mi_heap_t`, preventing cross-session interference
5. **O(1) cleanup**: `mi_heap_destroy()` releases all memory in a single operation
6. **Zero breaking changes**: `MemoryMessageStore` defaults are unchanged
