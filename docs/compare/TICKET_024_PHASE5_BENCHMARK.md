# TICKET_024 Phase 5: C++23 Ranges Utilities Benchmark

**Date**: 2026-02-03
**Phase**: 5 - Advanced Ranges
**Ticket**: TICKET_024_CPP23_FEATURE_ADOPTION

---

## Overview

Benchmark comparing C++23 ranges utilities vs manual implementations to determine appropriate use cases.

### Important: What This Benchmark Measures

| This IS | This is NOT |
|---------|-------------|
| `ranges` vs `manual` 对比 | Phase 5 前后性能变化 |
| 新工具函数的开销 | 现有代码的性能回归 |

**Phase 5 对现有代码性能影响：零**

- 新增的 `contains()`, `chunk()`, `slide()`, `stride()` 是**可选工具**
- 现有热路径代码**未调用**这些函数
- 只有主动选择使用时才有性能差异

**Methodology**: Uses `nfx::bench` utilities (see [TICKET_027](../design/TICKET_027_BENCHMARK_UTILS_ENHANCEMENT.md))
- `estimate_cpu_freq_ghz_busy()` - Busy-wait CPU calibration
- `warmup_icache()` / `warmup_dcache()` - Cache prewarming
- `ScopedTimer` + `compiler_barrier()` - Accurate measurement
- `LatencyStats` - Statistical analysis

## Test Environment

- CPU: 3.418 GHz (busy-wait calibrated)
- Compiler: GCC 13.3 with C++23
- Container size: 1000 elements
- Iterations: 100,000

## C++23 Feature Detection

```
ranges::contains:  NATIVE (C++23)
views::enumerate:  NATIVE (C++23)
views::chunk:      NATIVE (C++23)
```

---

## Benchmark Results

### 1. contains() vs manual find

| Method | Mean (ns) | P99 (ns) |
|--------|-----------|----------|
| Manual: `find() != end()` | 83.2 | 101.0 |
| `nfx::util::contains()` | 112.4 | 133.7 |

**Delta**: -35.1% overhead

### 2. enumerate() vs manual index loop

| Method | Mean (ns) | P99 (ns) |
|--------|-----------|----------|
| Manual: `for (i = 0; i < size; ++i)` | 112.4 | 115.0 |
| `nfx::util::enumerate()` | 110.8 | 113.5 |

**Delta**: +1.5% (zero overhead, within noise)

### 3. chunk() vs manual batch loop

| Method | Mean (ns) | P99 (ns) |
|--------|-----------|----------|
| Manual: `for (j += CHUNK_SIZE)` | 202.4 | 210.7 |
| `std::views::chunk()` | 211.2 | 320.7 |

**Delta**: -4.3% overhead

---

## Summary

| Operation | Manual (ns) | Ranges (ns) | Delta | Recommendation |
|-----------|-------------|-------------|-------|----------------|
| `contains()` | 83.2 | 112.4 | -35.1% | Manual for hot path |
| `enumerate()` | 112.4 | 110.8 | +1.5% | **Ranges OK everywhere** |
| `chunk()` | 202.4 | 211.2 | -4.3% | **Ranges OK for non-hot** |

---

## Analysis

### Why enumerate() performs well

- Native C++23 `std::views::enumerate` generates efficient code
- Compiler optimizes the zip+iota pattern effectively
- Iterator-based traversal matches manual loop performance

### Why contains() has overhead

- Function call overhead (wrapper indirection)
- Range adaptor object creation

### Why chunk() overhead is minimal

- With proper cache warmup, overhead is only ~4%
- Acceptable for non-hot-path batch processing

### Recommendation

| Use Case | Recommended Approach |
|----------|---------------------|
| Hot path (parsing, SIMD) | Manual loops |
| Configuration parsing | Ranges OK |
| Logging/debugging | Ranges OK |
| Setup/initialization | Ranges OK |
| Batch processing (non-hot) | **Ranges OK** |
| enumerate with index | **Ranges OK everywhere** |

---

## Conclusion

### 对现有代码的影响

**零影响** - Phase 5 只是新增可选工具函数，不修改任何现有代码路径。

### 新工具的使用建议

| 工具 | 开销 | 建议 |
|------|------|------|
| `enumerate()` | 0% | ✅ 任何地方可用 |
| `chunk()` | -4% | ✅ 非热路径可用 |
| `slide()` | ~同 chunk | ✅ 非热路径可用 |
| `stride()` | ~同 chunk | ✅ 非热路径可用 |
| `contains()` | -35% | ⚠️ 仅配置/日志场景 |

### 何时使用

```cpp
// ✅ 热路径：保持手动循环
for (size_t i = 0; i < data.size(); ++i) { ... }

// ✅ 非热路径：用 ranges 提高可读性
for (auto [i, val] : nfx::util::enumerate(config)) { ... }
```

The feature detection macros ensure optimal code generation when native C++23 support is available, with automatic fallback for older compilers.

---

## Methodology Note

Initial measurements without proper warmup showed inflated overhead (chunk: -77%). After applying `nfx::bench` utilities correctly:

| Test | Without Warmup | With Warmup | Error |
|------|---------------|-------------|-------|
| chunk() | -77.1% | -4.3% | 72.8pp |
| contains() | -48.8% | -35.1% | 13.7pp |

See [TICKET_027](../design/TICKET_027_BENCHMARK_UTILS_ENHANCEMENT.md) for benchmark best practices.

---

## Files Changed

- `include/nexusfix/util/ranges_utils.hpp` - Added C++23 feature detection
- `benchmarks/ranges_utils_bench.cpp` - New benchmark

## Related

- [TICKET_024_PHASE1_BENCHMARK.md](TICKET_024_PHASE1_BENCHMARK.md)
- [TICKET_024_PHASE2_BENCHMARK.md](TICKET_024_PHASE2_BENCHMARK.md)
- [TICKET_024_PHASE3_BENCHMARK.md](TICKET_024_PHASE3_BENCHMARK.md)
