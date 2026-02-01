# TICKET_023: Compile-time Optimization Roadmap

**Status**: Open
**Category**: Performance Optimization
**Priority**: Medium
**Technique**: Template Metaprogramming (C++23)

---

## Overview

Following the success of TICKET_022 (73% improvement in MsgType dispatch), this ticket tracks remaining opportunities to move runtime switch/if-chain logic to compile-time lookup tables.

---

## Completed

| Ticket | File | Optimization | Improvement |
|--------|------|--------------|-------------|
| TICKET_022 | `i_message.hpp` | MsgType dispatch | **73%** |

---

## Pending Optimizations

### 1. Error Message Mapping (HIGH PRIORITY)

**File**: `include/nexusfix/types/error.hpp`

| Error Type | Switch Cases | Lines |
|------------|--------------|-------|
| `ParseError::message()` | 12 cases | 67-83 |
| `SessionError::message()` | 9 cases | 120-133 |
| `TransportError::message()` | 20 cases | 190-219 |
| `ValidationError::message()` | 9 cases | 252-265 |

**Total**: 4 switch statements, 50 cases

**Approach**:
```cpp
namespace detail {
    template<ParseErrorCode Code>
    struct ParseErrorInfo;

    template<> struct ParseErrorInfo<ParseErrorCode::None> {
        static constexpr std::string_view message = "No error";
    };
    // ... single source of truth
}
```

**Expected Improvement**: 50-70% (similar to TICKET_022)

---

### 2. Session State Machine (MEDIUM PRIORITY)

**File**: `include/nexusfix/session/state.hpp`

| Function | Switch Cases | Lines |
|----------|--------------|-------|
| `state_name()` | 9 cases | 27-40 |
| `event_name()` | 13 cases | 78-95 |
| `next_state()` | ~30 branches | 102-160 |

**Total**: 3 switch statements, ~52 branches

**Approach**:
```cpp
namespace detail {
    template<SessionState State>
    struct StateInfo {
        static constexpr std::string_view name = "Unknown";
        static constexpr bool is_connected = false;
    };

    template<SessionState Current, SessionEvent Event>
    struct StateTransition {
        static constexpr SessionState next = Current;  // No transition
    };

    // Specializations for valid transitions
    template<> struct StateTransition<SessionState::Disconnected, SessionEvent::Connect> {
        static constexpr SessionState next = SessionState::SocketConnected;
    };
}
```

**Expected Improvement**: 40-60%

---

### 3. FIX Version Detection (MEDIUM PRIORITY)

**File**: `include/nexusfix/types/fix_version.hpp`

| Function | Cases | Lines |
|----------|-------|-------|
| `appl_ver_id::to_string()` | 10 cases | 49-63 |
| `detect_version()` | 6 if-chain | 85-93 |
| `version_string()` | 10 cases | 109-122 |

**Total**: 2 switch + 1 if-chain, 26 comparisons

**Approach**:
```cpp
namespace detail {
    template<FixVersion Ver>
    struct VersionInfo;

    template<> struct VersionInfo<FixVersion::FIX_4_4> {
        static constexpr std::string_view string = "FIX.4.4";
        static constexpr bool is_fixt = false;
        static constexpr bool is_fix4 = true;
    };

    // Hash-based detection for runtime
    inline constexpr auto VERSION_LOOKUP = create_version_table();
}
```

**Expected Improvement**: 30-50%

---

### 4. Tag Metadata Table (LOW PRIORITY)

**File**: `include/nexusfix/types/tag.hpp`

Currently only defines tag constants. Could add:

```cpp
namespace detail {
    template<int Tag>
    struct TagInfo {
        static constexpr std::string_view name = "Unknown";
        static constexpr bool is_header = false;
        static constexpr bool is_required = false;
    };

    template<> struct TagInfo<8> {
        static constexpr std::string_view name = "BeginString";
        static constexpr bool is_header = true;
        static constexpr bool is_required = true;
    };
}
```

**Benefit**: Unified tag metadata for logging, debugging, validation

---

### 5. Field Type Conversion (LOW PRIORITY)

**File**: `include/nexusfix/types/field_types.hpp`

Potential for compile-time enum-to-string mappings:

| Enum | Values |
|------|--------|
| `Side` | Buy, Sell, etc. |
| `OrdType` | Market, Limit, etc. |
| `TimeInForce` | Day, GTC, IOC, etc. |
| `ExecType` | New, Fill, Cancel, etc. |
| `OrdStatus` | New, PartialFill, Filled, etc. |

---

## Implementation Priority

| Phase | Ticket | Effort | Impact |
|-------|--------|--------|--------|
| 1 | Error Messages | Low | High (4 switches eliminated) |
| 2 | Session State | Medium | Medium (hot path in session) |
| 3 | FIX Version | Low | Medium (every message) |
| 4 | Tag Metadata | Medium | Low (debugging/logging) |
| 5 | Field Types | Low | Low (rarely called) |

---

## Estimated Total Improvement

| Metric | Current | After All Optimizations |
|--------|---------|------------------------|
| Switch statements | ~12 | 0 |
| Total case branches | ~130 | 0 (lookup tables) |
| Binary size | - | -500 bytes (less code) |
| Compile-time validation | Limited | Comprehensive |

---

## Pattern Template

Use TICKET_022 implementation as template:

1. Create `detail::XxxInfo<EnumValue>` template specializations
2. Generate `consteval` lookup table
3. Provide dual API: compile-time `name<Value>()` + runtime `name(value)`
4. Add `static_assert` validations
5. Create benchmark to verify improvement

---

## References

- TICKET_022: MsgType Dispatch (73% improvement)
- docs/modernc_quant.md: Techniques #1, #5, #82
- docs/compare/TICKET_022_MSGTYPE_DISPATCH_BENCHMARK.md
