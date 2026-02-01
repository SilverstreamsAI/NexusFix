# TICKET_023: Compile-time Optimization Roadmap

**Status**: Complete
**Category**: Performance Optimization
**Priority**: Medium
**Technique**: Template Metaprogramming (C++23)

---

## Overview

Following the success of TICKET_022 (73% improvement in MsgType dispatch), this ticket tracked opportunities to move runtime switch/if-chain logic to compile-time lookup tables.

**All phases are now complete.**

---

## Completed Optimizations

| Phase | File | Optimization | Improvement |
|-------|------|--------------|-------------|
| TICKET_022 | `i_message.hpp` | MsgType dispatch | **73%** |
| TICKET_023.1 | `error.hpp` | Error message mapping | **73-85%** |
| TICKET_023.2 | `state.hpp` | Session state machine | **66-85%** |
| TICKET_023.3 | `fix_version.hpp` | FIX version detection | **74-97%** |
| TICKET_023.4 | `tag.hpp` | Tag metadata | **42-76%** |
| TICKET_023.5 | `field_types.hpp` | Field type enums | **22-87%** |
| TICKET_023.6 | `market_data_types.hpp` | Market data enums | **0-92%** |
| TICKET_023.7 | `socket.hpp` | Connection state | (small enum) |

---

## Final Summary

### Lookup Tables Added

| File | Tables | Purpose |
|------|--------|---------|
| `i_message.hpp` | 1 | MsgType dispatch |
| `state.hpp` | 3 | State names, is_connected, transitions |
| `error.hpp` | 4 | Parse, session, transport, validation errors |
| `tag.hpp` | 1 | Tag metadata (name, header, required) |
| `field_types.hpp` | 5 | Side, OrdType, OrdStatus, ExecType, TimeInForce |
| `fix_version.hpp` | 2 | ApplVer, Version strings |
| `market_data_types.hpp` | 4 | MDEntryType, MDUpdateAction, SubscriptionType, MDReqRejReason |
| `socket.hpp` | 1 | Connection state |
| **Total** | **21** | |

### Impact Metrics

| Metric | Before | After |
|--------|--------|-------|
| Switch statements | ~25 | 0 |
| If-chain statements | ~5 | 0 |
| Total branches eliminated | ~300 | 0 (O(1) lookup) |
| Lookup tables | 0 | 21 |
| Average improvement | - | **55-75%** |

---

## Pattern Used

All optimizations followed this template:

```cpp
namespace detail {
    // 1. Template specializations (single source of truth)
    template<EnumType Value>
    struct EnumInfo {
        static constexpr std::string_view name = "Unknown";
    };

    template<> struct EnumInfo<EnumType::Value1> {
        static constexpr std::string_view name = "Value1";
    };
    // ... more specializations

    // 2. Compile-time lookup table generation
    consteval std::array<std::string_view, SIZE> create_table() {
        std::array<std::string_view, SIZE> table{};
        // Fill from template specializations
        return table;
    }
    inline constexpr auto TABLE = create_table();
}

// 3. Compile-time API
template<EnumType V>
[[nodiscard]] consteval std::string_view enum_name() noexcept {
    return detail::EnumInfo<V>::name;
}

// 4. Runtime O(1) API
[[nodiscard]] inline constexpr std::string_view enum_name(EnumType v) noexcept {
    const auto idx = static_cast<size_t>(v);
    if (idx < detail::TABLE.size()) [[likely]] {
        return detail::TABLE[idx];
    }
    return "Unknown";
}
```

---

## Benchmark Reports

- `docs/compare/TICKET_022_MSGTYPE_DISPATCH_BENCHMARK.md`
- `docs/compare/TICKET_023_ERROR_MESSAGE_BENCHMARK.md`
- `docs/compare/TICKET_023_SESSION_STATE_BENCHMARK.md`
- `docs/compare/TICKET_023_FIX_VERSION_BENCHMARK.md`
- `docs/compare/TICKET_023_TAG_METADATA_BENCHMARK.md`
- `docs/compare/TICKET_023_FIELD_TYPES_BENCHMARK.md`
- `docs/compare/TICKET_023_MARKET_DATA_TYPES_BENCHMARK.md`

---

## Remaining Switch Statements (Not Optimized)

These switches were analyzed but not optimized due to low benefit:

| File | Switch | Reason |
|------|--------|--------|
| `error_mapping.hpp` | OS error mapping | Values are OS-dependent macros, not contiguous |
| `logger.hpp` | LogLevel | Only called during initialization |
| `transport_factory.hpp` | TransportPreference | Returns polymorphic objects, not hot path |
| `sbe.hpp` | templateId dispatch | Only 2 cases, returns different types |

---

## References

- TICKET_022: MsgType Dispatch (73% improvement)
- docs/modernc_quant.md: Techniques #1, #5, #82
