# TICKET_023: Market Data Types Benchmark

**Date**: 2026-02-01
**Ticket**: TICKET_023 (Compile-time Optimization Roadmap - Phase 6)
**Optimization**: Add compile-time lookup tables for market data enum types

---

## Summary

Added compile-time lookup tables for 4 market data enum types with 35 total cases.

| Metric | Before | After |
|--------|--------|-------|
| Switch statements | 4 | 0 |
| Total cases | 35 | 35 (O(1) lookup) |
| Major improvement | - | **92%** |

---

## Implementation

For each enum type:
1. Template specializations with compile-time name
2. Lookup table indexed by char value
3. Dual API: compile-time `name<Value>()` and runtime `name(value)`

### Enums Covered

| Enum | Values | Lookup Strategy |
|------|--------|-----------------|
| MDEntryType | 13 | 128-entry sparse table |
| MDUpdateAction | 5 | 5-entry compact table |
| SubscriptionRequestType | 3 | 3-entry compact table |
| MDReqRejReason | 14 | 128-entry sparse table |

---

## Benchmark Results

Environment:
- CPU: Intel (VM environment)
- Compiler: GCC with -O3 -march=native
- Iterations: 10,000,000 per test

### Per Function

| Function | Cases | OLD (cycles) | NEW (cycles) | Improvement |
|----------|-------|--------------|--------------|-------------|
| md_entry_type_name() | 13 | 10.90 | 0.86 | **92%** |
| md_update_action_name() | 5 | 0.84 | 0.85 | 0% |
| subscription_type_name() | 3 | 0.88 | 0.88 | 0% |
| md_rej_reason_name() | 14 | 10.73 | 0.86 | **92%** |

### Random Access Pattern

| Function | OLD (cycles) | NEW (cycles) | Improvement |
|----------|--------------|--------------|-------------|
| md_entry_type_name() | 25.64 | 2.58 | **90%** |
| md_rej_reason_name() | 25.44 | 2.68 | **89%** |

### Analysis

**Major Wins** (larger switches with sparse values):
- `md_entry_type_name()` (13 cases): **92%** - Values span '0'-'9' and 'A'-'C'
- `md_rej_reason_name()` (14 cases): **92%** - Values span '0'-'9' and 'A'-'D'
- Random access: **90%** - Lookup tables eliminate branch misprediction

**No Change** (small contiguous enums):
- `md_update_action_name()` (5 cases): 0% - Compiler already generates optimal jump table
- `subscription_type_name()` (3 cases): 0% - Too small for optimization benefit

---

## Code Changes

**File**: `include/nexusfix/types/market_data_types.hpp`

### Example: MDEntryType lookup
```cpp
namespace detail {
    template<MDEntryType T> struct MDEntryTypeInfo {
        static constexpr std::string_view name = "Unknown";
    };
    template<> struct MDEntryTypeInfo<MDEntryType::Bid> {
        static constexpr std::string_view name = "Bid";
    };
    // ... 12 more specializations

    consteval std::array<std::string_view, 128> create_md_entry_type_table() {
        std::array<std::string_view, 128> table{};
        for (auto& e : table) e = "Unknown";
        table['0'] = MDEntryTypeInfo<MDEntryType::Bid>::name;
        table['1'] = MDEntryTypeInfo<MDEntryType::Offer>::name;
        // ...
        return table;
    }
    inline constexpr auto MD_ENTRY_TYPE_TABLE = create_md_entry_type_table();
}

[[nodiscard]] inline constexpr std::string_view md_entry_type_name(MDEntryType t) noexcept {
    const auto idx = static_cast<unsigned char>(static_cast<char>(t));
    if (idx < 128) [[likely]] {
        return detail::MD_ENTRY_TYPE_TABLE[idx];
    }
    return "Unknown";
}
```

---

## Additional: ConnectionState (socket.hpp)

Also optimized `connection_state_name()` in `include/nexusfix/transport/socket.hpp`:

| Function | Cases | Notes |
|----------|-------|-------|
| connection_state_name() | 5 | Compact 5-entry lookup table |

---

## TICKET_023 Extended Summary

| Phase | Target | Status | Key Improvement |
|-------|--------|--------|-----------------|
| 1 | Error Messages | **DONE** | 73-85% |
| 2 | Session State | **DONE** | 66-85% |
| 3 | FIX Version | **DONE** | 74-97% |
| 4 | Tag Metadata | **DONE** | 42-76% |
| 5 | Field Types | **DONE** | 22-87% |
| 6 | Market Data Types | **DONE** | 0-92% |
| 6 | Connection State | **DONE** | (small enum) |

### Total Impact

| Metric | Before | After |
|--------|--------|-------|
| Switch statements eliminated | - | ~20 |
| If-chain statements eliminated | - | ~5 |
| Total branches eliminated | - | ~250+ |
| Lookup tables added | 0 | 16+ |
