# TICKET_023: Field Types Benchmark

**Date**: 2026-02-01
**Ticket**: TICKET_023 (Compile-time Optimization Roadmap - Phase 5)
**Optimization**: Add compile-time enum-to-string mappings for FIX field types

---

## Summary

Added compile-time lookup tables for 5 FIX enum types with 63 total values.

| Metric | Before | After |
|--------|--------|-------|
| Enum types with to_string | 0 | 5 |
| Total enum values | 63 | 63 (with O(1) lookup) |
| Average improvement | - | **55%** |

---

## Implementation

For each enum type:
1. Template specializations with compile-time name
2. Lookup table indexed by char value
3. Dual API: compile-time `name<Value>()` and runtime `name(value)`

### Enums Covered

| Enum | Values | Lookup Size |
|------|--------|-------------|
| Side | 9 | 10 entries |
| OrdType | 12 | 128 entries (sparse) |
| OrdStatus | 15 | 128 entries (sparse) |
| ExecType | 19 | 128 entries (sparse) |
| TimeInForce | 8 | 8 entries |

---

## Benchmark Results

Environment:
- CPU: Intel (VM environment)
- Compiler: GCC with -O3 -march=native
- Iterations: 10,000,000 per test

### Per Function

| Function | Cases | OLD (cycles) | NEW (cycles) | Improvement |
|----------|-------|--------------|--------------|-------------|
| side_name() | 9 | 0.74 | 0.58 | **22%** |
| ord_type_name() | 12 | 5.69 | 0.76 | **87%** |
| ord_status_name() | 15 | 4.75 | 0.68 | **86%** |
| exec_type_name() | 19 | 4.73 | 1.74 | **63%** |
| time_in_force_name() | 8 | 0.79 | 0.76 | **3%** |
| Random access | - | 4.74 | 1.30 | **72%** |

### Analysis

**Major Wins** (larger switches with non-contiguous values):
- `ord_type_name()` (12 cases, sparse): **87%** - Values span '1'-'9' and 'D', 'E', 'P'
- `ord_status_name()` (15 cases): **86%** - Values span '0'-'9' and 'A'-'E'
- Random access: **72%** - Lookup tables excel for unpredictable patterns

**Moderate Wins**:
- `exec_type_name()` (19 cases): **63%** - Largest enum
- `side_name()` (9 cases): **22%** - Compact contiguous values

**Minimal Improvement**:
- `time_in_force_name()` (8 cases): **3%** - Small contiguous range, already optimal

---

## Code Changes

**File**: `include/nexusfix/types/field_types.hpp`

### Example: OrdType lookup
```cpp
namespace detail {
    template<OrdType T> struct OrdTypeInfo {
        static constexpr std::string_view name = "Unknown";
    };
    template<> struct OrdTypeInfo<OrdType::Market> {
        static constexpr std::string_view name = "Market";
    };
    // ... more specializations

    consteval std::array<std::string_view, 128> create_ord_type_table() {
        std::array<std::string_view, 128> table{};
        for (auto& e : table) e = "Unknown";
        table['1'] = OrdTypeInfo<OrdType::Market>::name;
        table['2'] = OrdTypeInfo<OrdType::Limit>::name;
        // ...
        return table;
    }
    inline constexpr auto ORD_TYPE_TABLE = create_ord_type_table();
}

[[nodiscard]] inline constexpr std::string_view ord_type_name(OrdType t) noexcept {
    const auto idx = static_cast<unsigned char>(static_cast<char>(t));
    if (idx < 128) [[likely]] {
        return detail::ORD_TYPE_TABLE[idx];
    }
    return "Unknown";
}
```

---

## Benefits

1. **Logging/debugging**: Easy conversion of enum values to readable strings
2. **Compile-time queries**: `exec_type_name<ExecType::Fill>()` resolves at compile time
3. **O(1) runtime lookup**: Constant time regardless of enum size
4. **Consistent API**: All enums follow the same pattern

---

## TICKET_023 Complete

All 5 phases are now complete:

| Phase | Target | Status | Key Improvement |
|-------|--------|--------|-----------------|
| 1 | Error Messages | **DONE** | 73-85% |
| 2 | Session State | **DONE** | 66-85% |
| 3 | FIX Version | **DONE** | 74-97% |
| 4 | Tag Metadata | **DONE** | 42-76% |
| 5 | Field Types | **DONE** | 22-87% |

### Total Impact

| Metric | Before | After |
|--------|--------|-------|
| Switch statements eliminated | - | ~15 |
| If-chain statements eliminated | - | ~5 |
| Total branches eliminated | - | ~200+ |
| Lookup tables added | 0 | 12 |
| Average improvement | - | **55-75%** |
