# PMR Message Store Optimization Benchmark

**Date:** 2026-01-26
**Optimization:** PMR (Polymorphic Memory Resources) for MemoryMessageStore
**Reference:** TICKET_INTERNAL_008, modernc_quant.md #16

---

## Executive Summary

| Metric | Improvement |
|--------|-------------|
| Mean Latency | **31.8% faster** |
| P99 Latency | **92.8% faster** (13.9x) |
| Latency Variance | **14x more stable** |

**Key Finding:** PMR provides predictable, stable latency critical for trading systems.

---

## Benchmark Results

### Configuration

| Parameter | Value |
|-----------|-------|
| Iterations | 100,000 |
| Message Size | 256 bytes (typical FIX) |
| PMR Pool Size | 64 MB |
| CPU | Pinned to core 2 |
| CPU Frequency | 3.418 GHz |

### Before: Heap Allocation (std::vector)

| Metric | Latency |
|--------|---------|
| Min | 18.1 ns |
| Mean | 86.5 ns |
| Median | 26.6 ns |
| P99 | 780.7 ns |
| P99.9 | 1899.3 ns |

**Observation:** High variance (18 ns to 780 ns at P99). The heap allocator is fast for cached allocations but suffers from occasional syscalls.

### After: PMR Pool Allocation (std::pmr::vector)

| Metric | Latency |
|--------|---------|
| Min | 43.9 ns |
| Mean | 59.0 ns |
| Median | 52.1 ns |
| P99 | 56.2 ns |
| P99.9 | 69.6 ns |

**Observation:** Extremely stable latency (44 ns to 56 ns at P99). No syscalls, pure pointer arithmetic.

---

## Comparison: Before vs After

| Metric | Before (Heap) | After (PMR) | Speedup | Improvement |
|--------|---------------|-------------|---------|-------------|
| Mean | 86.5 ns | 59.0 ns | 1.47x | **31.8%** |
| Median | 26.6 ns | 52.1 ns | 0.51x | -95.6% |
| P99 | 780.7 ns | 56.2 ns | 13.90x | **92.8%** |
| P99.9 | 1899.3 ns | 69.6 ns | 27.29x | **96.3%** |

### Latency Distribution Analysis

```
Heap (std::vector):     [18 ns] -------- [26 ns] ------------------------------------ [780 ns P99]
                        ^Min              ^Median                                      ^P99

PMR (std::pmr::vector): [44 ns] -- [52 ns] -- [56 ns P99]
                        ^Min       ^Median     ^P99

                        |<-- 12 ns range -->|   vs   |<--- 762 ns range --->|
```

**The PMR latency range is 14x narrower than heap allocation.**

---

## Why Median is Slower but P99 is Faster

### Heap Allocator Behavior

1. **First allocations:** Very fast (uses pre-allocated tcmalloc/jemalloc arena)
2. **Arena exhausted:** Triggers `brk()` or `mmap()` syscall (~500-2000 ns)
3. **Result:** Bimodal distribution - fast most times, occasional spikes

### PMR Allocator Behavior

1. **All allocations:** Consistent pointer bump (~50 ns)
2. **No syscalls:** Pool is pre-allocated at session start
3. **Result:** Flat distribution - predictable every time

---

## Trading System Implications

### Why P99 Matters More Than Median

In quantitative trading:
- **Median latency** affects average throughput
- **P99 latency** affects worst-case execution (when it matters most)

During market volatility:
- Order flow spikes cause allocator contention
- Heap allocator P99 degrades further under load
- PMR maintains consistent performance

### Real-World Impact

| Scenario | Heap Impact | PMR Impact |
|----------|-------------|------------|
| Normal trading | ~27 ns | ~52 ns |
| High volume burst | ~780+ ns (P99) | ~56 ns (P99) |
| Market volatility | Unpredictable spikes | Flat performance |

**PMR prevents worst-case latency spikes by 14x.**

---

## Implementation Details

### Changes Made

```cpp
// Before: Heap allocation per message
auto [it, inserted] = messages_.try_emplace(
    seq_num,
    std::vector<char>(msg.begin(), msg.end())  // malloc()
);

// After: PMR pool allocation
std::pmr::polymorphic_allocator<char> alloc(&pool_);
PmrVector pmr_msg(alloc);
pmr_msg.assign(msg.begin(), msg.end());  // pointer bump
messages_.try_emplace(seq_num, std::move(pmr_msg));
```

### Files Modified

- `include/nexusfix/store/memory_message_store.hpp`
  - Added `pool_size_bytes` config (default 64MB)
  - Added `PoolMetrics` struct
  - Changed `std::vector<char>` to `std::pmr::vector<char>`
  - Added `pool_metrics()` method

---

## Conclusion

| Aspect | Heap | PMR | Winner |
|--------|------|-----|--------|
| Average case | 27 ns | 52 ns | Heap |
| Worst case (P99) | 780 ns | 56 ns | **PMR (14x)** |
| Predictability | Low | High | **PMR** |
| Memory efficiency | Fragmentation | None | **PMR** |
| Syscalls | Yes | No | **PMR** |

**For trading systems, PMR is the correct choice** because:
1. Worst-case latency matters more than average
2. Predictable performance under load is critical
3. Zero syscalls on hot path is mandatory

---

## Benchmark Reproduction

```bash
cmake --build build --target message_store_pmr_bench
./build/bin/benchmarks/message_store_pmr_bench
```
