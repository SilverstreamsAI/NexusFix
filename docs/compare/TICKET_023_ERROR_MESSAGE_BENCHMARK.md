# TICKET_023: Error Message Mapping Benchmark

**Date**: 2026-02-01
**Ticket**: TICKET_023 (Compile-time Optimization Roadmap - Phase 1)
**Optimization**: Replace switch-based error message dispatch with compile-time lookup tables

---

## Summary

Eliminated 4 switch statements (50 total cases) by replacing runtime switch dispatch with compile-time generated lookup tables.

| Metric | Before | After |
|--------|--------|-------|
| Switch statements | 4 | 0 |
| Total case branches | 50 | 0 (lookup tables) |
| Average improvement | - | **47%** |

---

## Implementation

Following the TICKET_022 pattern:

1. Created `detail::XxxErrorInfo<Code>` template specializations for each error type
2. Generated `consteval` lookup tables at compile time
3. Provided dual API: compile-time `message<Code>()` + runtime `message(code)`
4. Added `static_assert` validations

---

## Benchmark Results

Environment:
- CPU: Intel (VM environment)
- Compiler: GCC with -O3 -march=native
- Iterations: 10,000,000 per test

### Per Error Type

| Error Type | Cases | OLD (cycles) | NEW (cycles) | Improvement |
|------------|-------|--------------|--------------|-------------|
| ParseError | 12 | 4.32 | 0.63 | **85%** |
| SessionError | 9 | 0.64 | 0.63 | 1% |
| TransportError | 20 | 4.56 | 1.24 | **73%** |
| ValidationError | 9 | 0.65 | 0.63 | 4% |
| Random Access | - | 3.83 | 0.98 | **74%** |

### Analysis

**Large Switches (12+ cases)**:
- ParseError (12 cases): **85% improvement**
- TransportError (20 cases): **73% improvement**

These show dramatic improvement because the switch generates multiple conditional branches that the lookup table eliminates entirely.

**Small Switches (9 cases)**:
- SessionError (9 cases): ~1% improvement
- ValidationError (9 cases): ~4% improvement

Small switches show minimal improvement because GCC already optimizes them to jump tables or well-predicted branch sequences. Both old and new execute in <1 cycle/op.

**Random Access Pattern**:
- **74% improvement** under cache pressure

This is the most realistic scenario for error handling in production, where error codes are not accessed in predictable order.

---

## Code Changes

**File**: `include/nexusfix/types/error.hpp`

Before (switch-based):
```cpp
[[nodiscard]] constexpr std::string_view message() const noexcept {
    switch (code) {
        case ParseErrorCode::None: return "No error";
        case ParseErrorCode::BufferTooShort: return "Buffer too short";
        // ... 10 more cases
    }
    return "Unknown error";
}
```

After (lookup table):
```cpp
namespace detail {
    template<ParseErrorCode Code>
    struct ParseErrorInfo {
        static constexpr std::string_view message = "Unknown error";
    };

    template<> struct ParseErrorInfo<ParseErrorCode::None> {
        static constexpr std::string_view message = "No error";
    };
    // ... specializations for all codes

    consteval std::array<std::string_view, 12> create_parse_error_table() {
        // Populate from template specializations
    }

    inline constexpr auto PARSE_ERROR_TABLE = create_parse_error_table();
}

[[nodiscard]] inline constexpr std::string_view parse_error_message(ParseErrorCode code) noexcept {
    const auto idx = static_cast<uint8_t>(code);
    if (idx < detail::PARSE_ERROR_TABLE.size()) [[likely]] {
        return detail::PARSE_ERROR_TABLE[idx];
    }
    return "Unknown error";
}
```

---

## Compile-time Benefits

1. **Single source of truth**: Error metadata defined once in template specializations
2. **Static validation**: `static_assert` ensures table correctness at compile time
3. **Dual API**: Both compile-time `message<Code>()` and runtime `message(code)` available
4. **Zero runtime overhead**: Lookup tables baked into binary

---

## Conclusion

The optimization delivers significant improvements for larger switch statements (73-85%) and maintains performance parity for smaller ones. The pattern is now established for future optimizations in TICKET_023:

- Phase 2: Session State Machine
- Phase 3: FIX Version Detection
- Phase 4: Tag Metadata Table
- Phase 5: Field Type Conversion
