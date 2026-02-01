# TICKET_022: Compile-time MsgType Dispatch Table

**Status**: Completed
**Category**: Performance Optimization
**Priority**: High
**Technique**: Template Metaprogramming (C++23)

---

## Problem

Current implementation in `i_message.hpp` has redundant switch statements:

```cpp
// Switch #1: name() - 13 cases
constexpr std::string_view name(char type) noexcept {
    switch (type) {
        case Heartbeat: return "Heartbeat";
        case Logon: return "Logon";
        // ... 11 more cases
    }
}

// Switch #2: is_admin() - repeated logic
constexpr bool is_admin(char type) noexcept {
    return type == Heartbeat || type == Logon ||
           type == Logout || type == TestRequest || ...;
}
```

**Issues**:
1. Two switch/if-chain with duplicated message type definitions
2. Adding new message type requires updates in 2+ places
3. Branch prediction pressure (13+ branches)
4. No compile-time validation of completeness

---

## Solution

Replace runtime switch with compile-time template specialization:

```cpp
namespace detail {
    template<char MsgType>
    struct MsgTypeInfo;

    template<> struct MsgTypeInfo<'0'> {
        static constexpr std::string_view name = "Heartbeat";
        static constexpr bool is_admin = true;
    };

    template<> struct MsgTypeInfo<'A'> {
        static constexpr std::string_view name = "Logon";
        static constexpr bool is_admin = true;
    };

    // ... single source of truth for all message types
}
```

---

## Expected Benefits

| Metric | Before | After |
|--------|--------|-------|
| Code duplication | 2 switch statements | Single template table |
| Branch prediction | 13+ branches | Compiler-optimized jump table |
| Maintenance | Update 2 places | Update 1 place |
| Compile-time validation | None | static_assert coverage |

## Benchmark Results

| Scenario | OLD (cycles/op) | NEW (cycles/op) | Improvement |
|----------|-----------------|-----------------|-------------|
| `name()` all 17 types | 4.63 | 1.23 | **73.4%** |
| `is_admin()` all types | 1.36 | 0.68 | **49.8%** |
| Hot path (5 common) | 1.73 | 1.01 | **41.6%** |
| Random access pattern | 5.72 | 1.23 | **78.6%** |

Benchmark: `benchmarks/msgtype_dispatch_bench.cpp`

---

## Implementation Plan

1. Create `MsgTypeInfo` template specializations
2. Replace `name()` to use template lookup
3. Replace `is_admin()` to use template lookup
4. Add static_assert for compile-time validation
5. Benchmark before/after

---

## Files Modified

- `include/nexusfix/interfaces/i_message.hpp`

---

## Implementation Result

### Before vs After

| Aspect | Before | After |
|--------|--------|-------|
| Data source | 2 switch/if-chain | Single `MsgTypeInfo<>` template |
| Runtime lookup | switch (17 cases) | O(1) array lookup |
| Compile-time query | Not available | `name<'A'>()`, `is_admin<'D'>()` |
| Validation | None | 12 static_assert checks |
| New message type | Update 2 places | Update 1 template specialization |

### Key Changes

1. **Template specializations**: `MsgTypeInfo<char>` for each message type
2. **Compile-time lookup table**: `consteval create_lookup_table()` generates 128-entry array
3. **Dual API**:
   - Compile-time: `name<'A'>()` (consteval)
   - Runtime: `name('A')` (O(1) array access)
4. **Static assertions**: Verify correctness at compile time

### Test Results

```
Runtime lookup:
  name('0') = Heartbeat
  name('A') = Logon
  is_admin('0') = 1 (Heartbeat)
  is_admin('D') = 0 (NewOrderSingle)

Compile-time query:
  name<'0'>() = Heartbeat
  is_admin<'A'>() = 1
```

---

## References

- TICKET_INTERNAL_020: Compile-time Sort Evaluation
- docs/modernc_quant.md: Technique #29 (std::variant de-virtualization)
