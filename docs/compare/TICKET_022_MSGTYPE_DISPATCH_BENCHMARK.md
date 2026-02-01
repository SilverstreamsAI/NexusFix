# TICKET_022: MsgType Dispatch Benchmark

**Date:** 2026-02-01
**Iterations:** 10,000,000
**Optimization Applied:** Compile-time lookup table (Template Metaprogramming)

---

## Executive Summary

MsgType dispatch performance improvement after replacing switch statements with compile-time lookup table:

| Metric | Improvement |
|--------|-------------|
| `name()` all types | **73.4% faster** |
| `is_admin()` all types | **49.8% faster** |
| Hot path (5 common) | **41.6% faster** |
| Random access pattern | **78.6% faster** |

---

## Benchmark Results

### name() Function - All 17 Message Types

| Metric | Before (switch) | After (lookup) | Improvement |
|--------|-----------------|----------------|-------------|
| Cycles/op | 4.63 | 1.23 | **73.4% faster** |

### is_admin() Function - All 17 Message Types

| Metric | Before (if-chain) | After (lookup) | Improvement |
|--------|-------------------|----------------|-------------|
| Cycles/op | 1.36 | 0.68 | **49.8% faster** |

### Hot Path - 5 Common Types

ExecutionReport ('8'), NewOrderSingle ('D'), Heartbeat ('0'), Logon ('A'), MarketDataSnapshot ('W')

| Metric | Before (switch) | After (lookup) | Improvement |
|--------|-----------------|----------------|-------------|
| Cycles/op | 1.73 | 1.01 | **41.6% faster** |

### Random Access Pattern (1024 random types)

Simulates real-world cache pressure with unpredictable message type distribution.

| Metric | Before (switch) | After (lookup) | Improvement |
|--------|-----------------|----------------|-------------|
| Cycles/op | 5.72 | 1.23 | **78.6% faster** |

---

## Implementation Details

### Before: Switch-based (17 cases)

```cpp
[[nodiscard]] constexpr std::string_view name(char type) noexcept {
    switch (type) {
        case '0': return "Heartbeat";
        case '1': return "TestRequest";
        // ... 15 more cases
        default:  return "Unknown";
    }
}

[[nodiscard]] constexpr bool is_admin(char type) noexcept {
    return type == '0' || type == '1' || type == '2' ||
           type == '3' || type == '4' || type == '5' || type == 'A';
}
```

**Issues:**
- 17 branches for `name()`, 7 comparisons for `is_admin()`
- Branch prediction pressure
- Redundant definitions (admin list in 2 places)

### After: Compile-time Lookup Table

```cpp
namespace detail {
    template<char MsgType>
    struct MsgTypeInfo;

    template<> struct MsgTypeInfo<'0'> {
        static constexpr std::string_view name = "Heartbeat";
        static constexpr bool is_admin = true;
    };
    // ... single source of truth

    inline constexpr auto LOOKUP_TABLE = create_lookup_table();
}

[[nodiscard]] inline constexpr std::string_view name(char type) noexcept {
    return detail::LOOKUP_TABLE[static_cast<unsigned char>(type)].name;
}
```

**Benefits:**
- O(1) array access (single memory load)
- No branches
- Single source of truth
- Compile-time validation via static_assert

---

## Why Such Large Improvement?

| Factor | Switch | Lookup Table |
|--------|--------|--------------|
| Branches | 17 comparisons | 0 |
| Memory access | Conditional jumps | 1 indexed load |
| Cache behavior | Jump table + strings | Contiguous array |
| Branch prediction | Pressure on random | Perfect (no branches) |

The **78.6% improvement on random access** is particularly significant because:
1. Switch-based dispatch suffers when branch predictor can't anticipate pattern
2. Lookup table performance is constant regardless of access pattern

---

## Real-world Impact

In NexusFIX, `msg_type::name()` and `is_admin()` are called:

| Location | Frequency | Affected Path |
|----------|-----------|---------------|
| Message parsing | Every message | Hot path |
| Session management | `is_admin()` check | Hot path |
| Logging | `name()` for display | Medium |
| Error handling | Message identification | Medium |

At **1M messages/sec**, saving 3.4 cycles per `name()` call = **~1.1 million cycles/sec** saved.

---

## Files Modified

- `include/nexusfix/interfaces/i_message.hpp` (+268/-50 lines)

## Benchmark Source

- `benchmarks/msgtype_dispatch_bench.cpp`

---

## Technique Reference

| Technique | modernc_quant.md Reference |
|-----------|---------------------------|
| Template specialization | #5 NTTP String Passing |
| consteval lookup table | #1 consteval Protocol Hardening |
| O(1) dispatch | #82 Compile-time lookup tables |
| Static assertions | #43 Static Assertions for Layout |
