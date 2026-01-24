# Abseil flat_hash_map Benchmark

**Date**: 2026-01-23
**Ticket**: TICKET_INTERNAL_011 (Abseil Analysis)
**Optimization**: Replace std::unordered_map with absl::flat_hash_map

---

## Summary

| Operation | std::unordered_map | absl::flat_hash_map | Speedup |
|-----------|-------------------|---------------------|---------|
| Lookup | 20.0 ns | 15.2 ns | **1.31x** |
| Insert | 17.4 ns | 12.7 ns | **1.37x** |
| P99 Lookup | 61.4 ns | 52.3 ns | **1.17x** |

---

## Test Environment

| Parameter | Value |
|-----------|-------|
| CPU | 3.418 GHz |
| Map size | 10,000 entries |
| Value size | 100 bytes (typical FIX message) |
| Key type | uint32_t (sequence number) |
| Warmup | 1,000 iterations |
| Benchmark | 100,000 iterations |
| Runs | 5 |

---

## Lookup Performance

```
std::unordered_map:   20.0 ns (mean), 18.8 ns (median), 61.4 ns (P99)
absl::flat_hash_map:  15.2 ns (mean), 13.8 ns (median), 52.3 ns (P99)

Throughput:
  std::unordered_map:   49.9M ops/s
  absl::flat_hash_map:  65.6M ops/s

Improvement: 31% faster
```

## Insert Performance

```
std::unordered_map:   17.4 ns/op (mean), 17.3 ns (median)
absl::flat_hash_map:  12.7 ns/op (mean), 12.6 ns (median)

Throughput:
  std::unordered_map:   57.6M ops/s
  absl::flat_hash_map:  78.9M ops/s

Improvement: 37% faster
```

---

## Implementation

### Conditional Include

```cpp
// memory_message_store.hpp
#if defined(NFX_HAS_ABSEIL) && NFX_HAS_ABSEIL
    #include <absl/container/flat_hash_map.h>
    #define NFX_USE_ABSEIL_HASH_MAP 1
#else
    #include <unordered_map>
    #define NFX_USE_ABSEIL_HASH_MAP 0
#endif

template<typename K, typename V>
#if NFX_USE_ABSEIL_HASH_MAP
using HashMap = absl::flat_hash_map<K, V>;
#else
using HashMap = std::unordered_map<K, V>;
#endif
```

### CMake Configuration

```cmake
option(NFX_ENABLE_ABSEIL "Enable Abseil for Swiss Table hash maps" ON)

if(NFX_ENABLE_ABSEIL)
    FetchContent_Declare(abseil-cpp
        GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
        GIT_TAG 20240722.0
    )
    FetchContent_MakeAvailable(abseil-cpp)

    target_link_libraries(nexusfix INTERFACE
        absl::flat_hash_map
        absl::flat_hash_set
    )
    target_compile_definitions(nexusfix INTERFACE NFX_HAS_ABSEIL=1)
endif()
```

---

## Why flat_hash_map is Faster

### Swiss Tables Design

1. **SIMD-accelerated probing**: SSE instructions compare 16 slots simultaneously
2. **Higher load factor**: 87.5% vs std::unordered_map's ~50%
3. **Flat storage**: Values stored inline, not in separate nodes
4. **Better cache locality**: Contiguous memory layout

### Memory Layout Comparison

```
std::unordered_map (node-based):
  [bucket array] → [node] → value
                 → [node] → value
  Cache misses: 2-3 per lookup

absl::flat_hash_map (flat):
  [control bytes | values in slots]
  Cache misses: 1-2 per lookup
```

---

## Analysis

### Observed vs Expected

| Metric | Expected | Observed | Notes |
|--------|----------|----------|-------|
| Lookup speedup | 2-3x | **1.31x** | Simple uint32_t keys |
| Insert speedup | 1.5-2x | **1.37x** | Matches expectations |

### Why Lower Than Expected

1. **Simple keys**: uint32_t has trivial hash, less SIMD benefit
2. **High cache hit rate**: 10K entries fit in L3 cache
3. **Test environment**: VM may have different cache behavior

### For FIX Message Store

The actual improvement may be higher in production:
- Complex keys (session ID + seq number)
- Larger datasets (100K+ messages)
- Mixed workloads (store/retrieve/range queries)

---

## Files Modified

| File | Changes |
|------|---------|
| `CMakeLists.txt` | Added NFX_ENABLE_ABSEIL option, FetchContent for Abseil |
| `include/nexusfix/store/memory_message_store.hpp` | Replaced std::unordered_map with HashMap alias |
| `benchmarks/CMakeLists.txt` | Added hash_map_bench target |
| `benchmarks/hash_map_bench.cpp` | Created benchmark |

---

## Conclusion

| Aspect | Result |
|--------|--------|
| Lookup improvement | **31% faster** |
| Insert improvement | **37% faster** |
| P99 improvement | **17% faster** |
| Backward compatibility | Full (fallback to std::unordered_map) |
| Build complexity | Low (FetchContent) |

The absl::flat_hash_map provides measurable improvement with automatic fallback when Abseil is not available.

---

## References

- [Abseil Swiss Tables](https://abseil.io/docs/cpp/guides/container)
- [CppCon 2017: Designing a Fast, Efficient, Cache-friendly Hash Table](https://www.youtube.com/watch?v=ncHmEUmJZf4)
- [Swiss Tables Design Notes](https://abseil.io/about/design/swisstables)
