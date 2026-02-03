# TICKET_024: C++23 Feature Adoption Roadmap

**Status**: In Progress
**Priority**: High
**Category**: Optimization / Modernization
**Created**: 2026-02-02

---

## Overview

Systematic adoption of C++23 (ISO/IEC 14882:2024) features to maximize performance, type safety, and code clarity in NexusFix. This ticket tracks all C++23 features and their adoption status.

**Reference**: [cppreference C++23](https://en.cppreference.com/w/cpp/23) | [C++ Stories C++23](https://www.cppstories.com/2024/cpp23_lang/)

---

## Current Adoption Status Summary

| Category | Total Features | Adopted | Blocked | Remaining | Adoption Rate |
|----------|---------------|---------|---------|-----------|---------------|
| Language Features | 17 | 9 | 1 | 7 | 53% |
| Library Features | 45+ | 19 | 8 | 18+ | ~42% |
| **Total** | **62+** | **28** | **9** | **25+** | **~45%** |

**Note**: 9 features (flat_map, flat_set, mdspan, ranges::to, ranges::starts_with, ranges::ends_with, deducing this, constexpr bitset, start_lifetime_as) are blocked pending GCC 14+ / libstdc++ 14+ support. Current environment: GCC 13.3.

---

## Part 1: Language Features

### 1.1 ADOPTED Features

| # | Feature | Paper | Location | Notes |
|---|---------|-------|----------|-------|
| L1 | `consteval` functions | P1938 | `parser/consteval_parser.hpp` | Field offset calculation |
| L2 | `constexpr` enhancements | P2242/P2647 | Throughout | Static vars in constexpr |
| L3 | `[[nodiscard]]` | - | 300+ uses | All API return values |
| L4 | `[[likely]]`/`[[unlikely]]` | - | Hot paths | Branch hints |
| L5 | Spaceship operator `<=>` | P1185 | `types/field_types.hpp` | Strong types |
| L6 | Concepts | P0734 | `interfaces/`, `memory/` | `Message`, `IsTag`, etc. |
| L8 | `[[assume]]` Attribute | P1774R8 | `simd_scanner.hpp`, `field_view.hpp` | Compiler optimization (Phase 1) |
| L12 | `if consteval` | P1938R3 | `sbe/types/sbe_types.hpp` | Cleaner dual-path code (Phase 6) |
| L13 | Size_t literal `uz` | P0330R8 | `util/ranges_utils.hpp` | Type safety (Phase 6) |

### 1.2 NOT YET ADOPTED Features

| # | Feature | Paper | Priority | Target | Benefit |
|---|---------|-------|----------|--------|---------|
| L7 | **Deducing This** | P0847R7 | **HIGH** | CRTP simplification | Eliminates boilerplate (BLOCKED: GCC 14+) |
| L9 | **Multidimensional `[]`** | P2128R6 | MEDIUM | `mdspan` usage | Cleaner matrix access |
| L10 | **Static `operator()`** | P1169R4 | MEDIUM | Functors | Zero-overhead callables (N/A: no existing functors) |
| L11 | **`auto(x)` decay-copy** | P0849R8 | LOW | Generic code | Explicit decay |
| L14 | **`#elifdef`/`#elifndef`** | P2334R1 | LOW | Platform code | Cleaner preprocessor |
| L15 | **`#warning`** | P2437R1 | LOW | Build warnings | Deprecation notices |
| L16 | **Lambda attributes** | P2173R1 | LOW | Callbacks | `[[nodiscard]]` on lambdas |
| L17 | **Extended floats** | P1467R9 | LOW | Future | `float16_t`, `bfloat16_t` |

---

## Part 2: Library Features - New Headers

### 2.1 ADOPTED Headers

| Header | Status | Location | Notes |
|--------|--------|----------|-------|
| `<expected>` | **ADOPTED** | `types/error.hpp` | `ParseResult<T>`, `SessionResult<T>` |
| `<bit>` | **ADOPTED** | `memory/buffer_pool.hpp` | `has_single_bit`, `bit_cast` |
| `<concepts>` | **ADOPTED** | `interfaces/`, `types/` | Message concepts |
| `<print>` | **ADOPTED** | `util/format_utils.hpp` | `std::print`, `std::println` with fallback (Phase 3) |

### 2.2 NOT YET ADOPTED Headers

| # | Header | Priority | Target | Benefit |
|---|--------|----------|--------|---------|
| H2 | **`<mdspan>`** | **HIGH** | Market data matrices | Multi-dimensional spans |
| H3 | **`<generator>`** | MEDIUM | Coroutine ranges | `std::generator<T>` |
| H4 | **`<flat_map>`** | MEDIUM | Lookup tables | Cache-friendly maps |
| H5 | **`<flat_set>`** | MEDIUM | Symbol sets | Cache-friendly sets |
| H6 | **`<spanstream>`** | LOW | Buffer parsing | `std::spanstream` |
| H7 | **`<stacktrace>`** | LOW | Debug builds | Stack traces |
| H8 | **`<stdfloat>`** | LOW | Future | Extended floats |

---

## Part 3: Library Features - Utilities

### 3.1 ADOPTED Utilities

| Utility | Location | Notes |
|---------|----------|-------|
| `std::expected` | `types/error.hpp` | Zero-cost error handling |
| `std::optional` | Throughout | Nullable fields |
| `std::span` | `parser/field_view.hpp` | Zero-copy views |
| `std::format` | `util/format_utils.hpp` | Type-safe formatting |
| `std::unreachable` | `memory/memory_lock.hpp` | Optimization hint (Phase 1) |
| `std::to_underlying` | `memory/memory_lock.hpp`, `market_data.hpp` | Type-safe enum cast (Phase 1) |
| Monadic `std::expected` | `messages/*.hpp`, `session/` | `and_then`, `or_else`, `transform` (Phase 2) |
| Monadic `std::optional` | `types/error.hpp`, `messages/*.hpp` | `and_then`, `transform` (Phase 2) |
| `std::byteswap` | `util/bit_utils.hpp` | Endian conversion with fallback (Phase 7) |

### 3.2 NOT YET ADOPTED Utilities

| # | Utility | Paper | Priority | Target | Benefit |
|---|---------|-------|----------|--------|---------|
| U6 | **`std::move_only_function`** | P0288R9 | MEDIUM | Callbacks | Non-copyable funcs |
| U7 | **`std::forward_like`** | P2445R1 | MEDIUM | Generic code | Forwarding helpers |
| U8 | **`std::invoke_r`** | P2136R3 | LOW | Callbacks | Return type cast |
| U9 | **`std::bind_back`** | P2387R3 | LOW | Partial application | Bind trailing args |
| U10 | **`std::out_ptr`** | P1132R8 | LOW | C interop | Smart ptr adaptor |
| U11 | **`std::start_lifetime_as`** | P2590R2 | MEDIUM | Buffer reuse | Explicit lifetime |

---

## Part 4: Library Features - Ranges & Algorithms

### 4.1 ADOPTED Ranges

| Feature | Location | Notes |
|---------|----------|-------|
| `std::ranges` basics | Various | Range-based algorithms |
| `std::views::filter` | Parsing | Conditional filtering |

### 4.2 NOT YET ADOPTED Ranges

| # | Feature | Paper | Priority | Target | Benefit |
|---|---------|-------|----------|--------|---------|
| R1 | **`ranges::to`** | P1206R7 | **HIGH** | Container conversion | `range \| to<vector>()` |
| R2 | **`views::enumerate`** | P2164R9 | **HIGH** | Indexed iteration | `for (auto [i, v] : enumerate(vec))` |
| R3 | **`views::zip`** | P2321R2 | MEDIUM | Parallel iteration | Zip multiple ranges |
| R4 | **`views::chunk`** | P2442R1 | MEDIUM | Batch processing | Fixed-size chunks |
| R5 | **`views::slide`** | P2442R1 | MEDIUM | Window functions | Sliding windows |
| R6 | **`views::stride`** | P1899R3 | MEDIUM | Sampling | Every Nth element |
| R7 | **`views::as_const`** | P2278R4 | LOW | Const iteration | Const view |
| R8 | **`views::cartesian_product`** | P2374R4 | LOW | Combinations | Cross product |
| R9 | **`views::repeat`** | P2474R2 | LOW | Test data | Repeat value N times |
| R10 | **`views::adjacent`** | P2321R2 | LOW | Pairs | Adjacent elements |

### 4.3 NOT YET ADOPTED Algorithms

| # | Algorithm | Paper | Priority | Target | Benefit |
|---|-----------|-------|----------|--------|---------|
| A1 | **`ranges::contains`** | P2302R4 | **HIGH** | Lookups | Cleaner than `find != end` |
| A2 | **`ranges::starts_with`** | P1659R3 | **HIGH** | String matching | Protocol headers |
| A3 | **`ranges::ends_with`** | P1659R3 | MEDIUM | String matching | Suffix checks |
| A4 | **`ranges::find_last`** | P1223R5 | MEDIUM | Reverse search | Last occurrence |
| A5 | **`ranges::fold_left`** | P2322R6 | MEDIUM | Reductions | Left fold |
| A6 | **`ranges::fold_right`** | P2322R6 | LOW | Reductions | Right fold |
| A7 | **`ranges::iota`** | P2440R1 | LOW | Sequences | Generate sequences |

---

## Part 5: Library Features - Containers

### 5.1 ADOPTED Container Features

| Feature | Status | Location | Notes |
|---------|--------|----------|-------|
| `string::contains` | **ADOPTED** | `test_market_data.cpp` | Cleaner string checks (Phase 1) |
| `string::resize_and_overwrite` | **ADOPTED** | `util/format_utils.hpp` | `format_to_string`, `build_string` (Phase 3) |

### 5.2 NOT YET ADOPTED Container Features

| # | Feature | Paper | Priority | Target | Benefit |
|---|---------|-------|----------|--------|---------|
| C1 | **`std::flat_map`** | P0429R9 | **HIGH** | Symbol lookup | Cache-friendly |
| C2 | **`std::flat_set`** | P1222R4 | **HIGH** | Tag sets | Cache-friendly |
| C3 | **`std::mdspan`** | P0009R18 | **HIGH** | Market data | Multi-dim views |

---

## Part 6: Library Features - Compile-Time

### 6.1 NOT YET ADOPTED Constexpr Enhancements

| # | Feature | Paper | Priority | Target | Benefit |
|---|---------|-------|----------|--------|---------|
| CE1 | **`constexpr std::unique_ptr`** | P2273R3 | MEDIUM | RAII in constexpr | Compile-time allocation |
| CE2 | **`constexpr std::bitset`** | P2417R2 | MEDIUM | Flag sets | Compile-time bitsets |
| CE3 | **`constexpr <cmath>`** | P0533R9 | LOW | Math operations | `sqrt`, `pow` constexpr |
| CE4 | **`constexpr to_chars/from_chars`** | P2291R3 | **HIGH** | Number parsing | Compile-time int conversion |

### 6.2 NOT YET ADOPTED Type Traits

| # | Trait | Paper | Priority | Target | Benefit |
|---|-------|-------|----------|--------|---------|
| T1 | **`std::is_scoped_enum`** | P1048R1 | MEDIUM | Enum validation | Type safety |
| T2 | **`std::is_implicit_lifetime`** | P2674R1 | LOW | Buffer types | Lifetime checks |

---

## Part 7: Library Features - I/O & Formatting

### 7.1 NOT YET ADOPTED I/O Features

| # | Feature | Paper | Priority | Target | Benefit |
|---|---------|-------|----------|--------|---------|
| IO1 | **`std::print`** | P2093R14 | **HIGH** | Logging | Formatted output |
| IO2 | **`std::println`** | P2093R14 | **HIGH** | Logging | With newline |
| IO3 | **`std::spanstream`** | P0448R4 | MEDIUM | Buffer I/O | Span-based streams |
| IO4 | **Format ranges** | P2286R8 | MEDIUM | Debug output | Print containers |
| IO5 | **Format tuples** | P2286R8 | LOW | Debug output | Print tuples |

---

## Implementation Phases

### Phase 1: Quick Wins (Week 1)
**HIGH priority, minimal code changes**

| Item | Feature | Files | Effort |
|------|---------|-------|--------|
| 1.1 | `std::unreachable()` | All switch statements | 1 day |
| 1.2 | `std::to_underlying()` | Enum conversions | 1 day |
| 1.3 | `[[assume]]` attribute | Hot path loops | 1 day |
| 1.4 | `ranges::contains` | Lookup code | 1 day |
| 1.5 | `string::contains` | String checks | 0.5 day |

### Phase 2: Error Handling Enhancement (Week 2)
**Monadic operations for std::expected/optional**

| Item | Feature | Files | Effort |
|------|---------|-------|--------|
| 2.1 | `expected::and_then` | Error chains | 2 days |
| 2.2 | `expected::or_else` | Fallback logic | 1 day |
| 2.3 | `expected::transform` | Value mapping | 1 day |
| 2.4 | `optional::and_then` | Field parsing | 1 day |

### Phase 3: I/O Modernization (Week 3)
**std::print and formatting**

| Item | Feature | Files | Effort |
|------|---------|-------|--------|
| 3.1 | `std::print` adoption | `util/format_utils.hpp` | 2 days |
| 3.2 | Format ranges | Debug output | 1 day |
| 3.3 | `resize_and_overwrite` | Buffer building | 2 days |

### Phase 4: Container Optimization (Week 4)
**Cache-friendly containers**

| Item | Feature | Files | Effort |
|------|---------|-------|--------|
| 4.1 | `std::flat_map` | Symbol lookups | 2 days |
| 4.2 | `std::flat_set` | Tag sets | 1 day |
| 4.3 | `std::mdspan` | Market data | 2 days |

### Phase 5: Advanced Ranges (Week 5)
**Modern range operations**

| Item | Feature | Files | Effort |
|------|---------|-------|--------|
| 5.1 | `ranges::to` | Container conversions | 1 day |
| 5.2 | `views::enumerate` | Indexed loops | 1 day |
| 5.3 | `views::zip` | Parallel iteration | 1 day |
| 5.4 | `views::chunk` | Batch processing | 1 day |
| 5.5 | `ranges::starts_with` | Protocol matching | 1 day |

### Phase 6: Language Features (Week 6)
**Deducing this and more**

| Item | Feature | Files | Effort |
|------|---------|-------|--------|
| 6.1 | Deducing `this` | CRTP classes | 2 days |
| 6.2 | Static `operator()` | Functors | 1 day |
| 6.3 | `if consteval` | Dual-path code | 1 day |
| 6.4 | Size_t literal `uz` | Throughout | 1 day |

### Phase 7: Compile-Time (Week 7)
**constexpr enhancements**

| Item | Feature | Files | Effort |
|------|---------|-------|--------|
| 7.1 | `constexpr to_chars` | Number serialization | 2 days |
| 7.2 | `constexpr bitset` | Flag operations | 1 day |
| 7.3 | `std::byteswap` | Endian conversion | 1 day |
| 7.4 | `std::start_lifetime_as` | Buffer reuse | 1 day |

---

## Detailed Implementation Notes

### L7: Deducing This (P0847R7)

**Current CRTP pattern:**
```cpp
template <typename Derived, typename T>
struct StrongType {
    T value;
    constexpr Derived& operator++() noexcept {
        ++value;
        return static_cast<Derived&>(*this);
    }
};
```

**With deducing this:**
```cpp
template <typename T>
struct StrongType {
    T value;
    constexpr auto& operator++(this auto&& self) noexcept {
        ++self.value;
        return self;
    }
};
```

**Files to update:**
- `types/field_types.hpp` - `StrongType` base class

---

### L8: [[assume]] Attribute (P1774R8)

**Current code:**
```cpp
if (size > 0) {
    // process
}
```

**With [[assume]]:**
```cpp
[[assume(size > 0)]];
for (size_t i = 0; i < size; ++i) {
    // compiler can optimize knowing size > 0
}
```

**Target locations:**
- `parser/runtime_parser.hpp` - Field parsing loops
- `memory/mpsc_queue.hpp` - Queue operations
- `serializer/` - Serialization loops

---

### U1: std::unreachable (P0627R6)

**Current code:**
```cpp
switch (type) {
    case Type::A: return handleA();
    case Type::B: return handleB();
    default:
        assert(false && "unreachable");
        return {};  // Dead code to satisfy compiler
}
```

**With std::unreachable:**
```cpp
switch (type) {
    case Type::A: return handleA();
    case Type::B: return handleB();
}
std::unreachable();  // Optimizer knows this is never reached
```

---

### U3: std::byteswap (P1272R4)

**Current code (if any manual byte swapping):**
```cpp
uint32_t swap_bytes(uint32_t val) {
    return ((val >> 24) & 0xFF) |
           ((val >> 8) & 0xFF00) |
           ((val << 8) & 0xFF0000) |
           ((val << 24) & 0xFF000000);
}
```

**With std::byteswap:**
```cpp
auto swapped = std::byteswap(val);
```

---

### U4: Monadic std::expected (P2505R5)

**Current code:**
```cpp
auto result1 = parse_header(data);
if (!result1) return std::unexpected(result1.error());
auto result2 = parse_body(result1.value());
if (!result2) return std::unexpected(result2.error());
return result2.value();
```

**With monadic operations:**
```cpp
return parse_header(data)
    .and_then(parse_body)
    .transform(finalize);
```

---

### R1: ranges::to (P1206R7)

**Current code:**
```cpp
std::vector<int> result;
for (auto x : view | std::views::filter(pred)) {
    result.push_back(x);
}
```

**With ranges::to:**
```cpp
auto result = view
    | std::views::filter(pred)
    | std::ranges::to<std::vector>();
```

---

### C1: std::flat_map (P0429R9)

**Current code:**
```cpp
std::map<Symbol, Price> prices;  // Red-black tree, cache-unfriendly
```

**With flat_map:**
```cpp
std::flat_map<Symbol, Price> prices;  // Sorted vector, cache-friendly
```

**Benefits:**
- Better cache locality for iteration
- Lower memory overhead
- Faster lookups for small-medium maps

---

### C5: string::resize_and_overwrite (P1072R10)

**Current code:**
```cpp
std::string buffer;
buffer.resize(max_size);  // Zero-initializes
auto written = fill_buffer(buffer.data(), max_size);
buffer.resize(written);
```

**With resize_and_overwrite:**
```cpp
std::string buffer;
buffer.resize_and_overwrite(max_size, [](char* p, size_t n) {
    return fill_buffer(p, n);  // Returns actual size written
});
```

**Benefits:**
- Avoids double initialization
- Single resize operation

---

### IO1/IO2: std::print/println (P2093R14)

**Current code:**
```cpp
std::cout << std::format("Price: {}, Qty: {}\n", price, qty);
```

**With std::print:**
```cpp
std::println("Price: {}, Qty: {}", price, qty);
```

**Benefits:**
- Cleaner syntax
- Automatic newline with `println`
- Better performance (no iostream overhead)

---

## Success Metrics

| Metric | Before | Target |
|--------|--------|--------|
| C++23 adoption rate | ~23% | >80% |
| Lines using `[[assume]]` | 0 | 50+ |
| Monadic error chains | 0 | 20+ |
| `std::flat_map` usage | 0 | 5+ |
| `ranges::to` usage | 0 | 10+ |

---

## Compiler Support Requirements

| Feature | GCC | Clang | MSVC |
|---------|-----|-------|------|
| Deducing this | 14+ | 18+ | 19.32+ |
| `[[assume]]` | 13+ | 19+ | 19.35+ |
| `std::expected` | 12+ | 16+ | 19.33+ |
| `std::flat_map` | 14+ | 18+ | 19.34+ |
| `std::print` | 14+ | 17+ | 19.37+ |
| `std::mdspan` | 14+ | 18+ | 19.33+ |
| `ranges::to` | 14+ | 17+ | 19.34+ |

**Minimum Required**: GCC 14+ or Clang 18+

---

## References

- [C++23 - cppreference](https://en.cppreference.com/w/cpp/23)
- [C++23 Language Features - C++ Stories](https://www.cppstories.com/2024/cpp23_lang/)
- [C++23 Library Features - C++ Stories](https://www.cppstories.com/2024/cpp23_lib/)
- [C++23 - Wikipedia](https://en.wikipedia.org/wiki/C++23)

---

## Progress Log

| Date | Phase | Items Completed | Notes |
|------|-------|-----------------|-------|
| 2026-02-02 | Setup | Ticket created | Initial assessment |
| 2026-02-02 | Phase 1 | L8, U1, U2, C4 | Quick wins completed |
| 2026-02-02 | Phase 2 | U4, U5 | Monadic error handling - and_then, transform, or_else |
| 2026-02-02 | Phase 3 | IO1, IO2, C5 | I/O modernization - std::print, resize_and_overwrite |
| 2026-02-02 | Phase 4 | BLOCKED | flat_map/flat_set/mdspan require GCC 14+ (libstdc++ 14+) |
| 2026-02-02 | Phase 5 | R2, A1 partial | Added C++23 feature detection to ranges_utils.hpp |
| 2026-02-03 | Phase 6 | L12, L13 | if consteval, uz literal; L7/L10 BLOCKED (GCC 14+) |
| 2026-02-03 | Phase 7 | U3 | std::byteswap already adopted in bit_utils.hpp; CE2/U11 BLOCKED |

## Benchmark Reports

| Phase | Report | Summary |
|-------|--------|---------|
| Phase 1 | [TICKET_024_PHASE1_BENCHMARK.md](/docs/compare/TICKET_024_PHASE1_BENCHMARK.md) | P99 -8% to -30%, StdDev -27% to -80% |
| Phase 2 | [TICKET_024_PHASE2_BENCHMARK.md](/docs/compare/TICKET_024_PHASE2_BENCHMARK.md) | Zero overhead (+0.7% to +1.6% within noise) |
| Phase 3 | [TICKET_024_PHASE3_BENCHMARK.md](/docs/compare/TICKET_024_PHASE3_BENCHMARK.md) | Zero hot-path impact (I/O utilities only) |
| Phase 5 | [TICKET_024_PHASE5_BENCHMARK.md](/docs/compare/TICKET_024_PHASE5_BENCHMARK.md) | 新增可选工具，现有代码零影响；enumerate() 零开销，chunk() -4%，contains() -35% |
| Phase 5+6+7 | [TICKET_024_PHASE567_BENCHMARK.md](/docs/compare/TICKET_024_PHASE567_BENCHMARK.md) | 综合测试：零性能影响，Parse -1.5%~-3.5% (噪声范围) |

---

## Checklist

### Phase 1: Quick Wins
- [x] L8: Add `[[assume]]` to hot paths (simd_scanner.hpp, field_view.hpp, mpsc_queue.hpp)
- [x] U1: Replace `assert(false)` with `std::unreachable()` (memory_lock.hpp)
- [x] U2: Replace `static_cast<underlying>` with `std::to_underlying()` (memory_lock.hpp, market_data.hpp, prefetch.hpp)
- [ ] A1: Replace `find != end` with `ranges::contains` (skipped - need position values)
- [x] C4: Use `string::contains` where applicable (test_market_data.cpp)

### Phase 2: Error Handling
- [x] U4: Add monadic chains to `ParseResult` (and_then in message from_buffer methods)
- [x] U4: Add monadic chains to `SessionResult` (session_manager.hpp)
- [x] U5: Add monadic operations to optional field parsing (if_has_value, to_uint32, to_int)

### Phase 3: I/O Modernization
- [x] IO1/IO2: Adopt `std::print`/`std::println` (format_utils.hpp, logdump.hpp)
- [x] IO4: Add range formatting for debug output (format_hex in format_utils.hpp)
- [x] C5: Use `resize_and_overwrite` in buffer builders (format_to_string, build_string in format_utils.hpp)

### Phase 4: Container Optimization (BLOCKED - needs GCC 14+/libstdc++ 14+)
- [ ] C1: Evaluate `std::flat_map` for symbol lookup (not available in GCC 13.3)
- [ ] C2: Evaluate `std::flat_set` for tag sets (not available in GCC 13.3)
- [ ] C3: Implement `std::mdspan` for market data (not available in GCC 13.3)

### Phase 5: Advanced Ranges
- [ ] R1: Adopt `ranges::to` for container conversions (not available in GCC 13.3)
- [x] R2: Use `views::enumerate` for indexed iteration (added to ranges_utils.hpp with feature detection)
- [ ] R3: Use `views::zip` for parallel iteration (available, added to ranges_utils.hpp)
- [x] R4: Use `views::chunk` for batch processing (added to ranges_utils.hpp with feature detection)
- [x] R5: Use `views::slide` for sliding windows (added to ranges_utils.hpp with feature detection)
- [x] R6: Use `views::stride` for sampling (added to ranges_utils.hpp with feature detection)
- [ ] A2: Use `ranges::starts_with` for protocol matching (not available in GCC 13.3)
- [x] A1: Add `ranges::contains` wrapper (added to ranges_utils.hpp with feature detection)

### Phase 6: Language Features
- [ ] L7: Refactor CRTP with deducing this (BLOCKED - needs GCC 14+)
- [ ] L10: Add static `operator()` to functors (N/A - no existing functors with operator())
- [x] L12: Replace `is_constant_evaluated` with `if consteval` (sbe_types.hpp read_le/write_le)
- [x] L13: Use `uz` size_t literal for type safety (ranges_utils.hpp indices/enumerate/FixFieldView)

### Phase 7: Compile-Time
- [ ] CE4: Use `constexpr to_chars` where possible (N/A - uses custom IntToString for compile-time)
- [x] U3: Use `std::byteswap` for endian conversion (bit_utils.hpp byteswap16/32/64 with fallback)
- [ ] CE2: Use `constexpr bitset` (BLOCKED - needs GCC 14+)
- [ ] U11: Use `std::start_lifetime_as` for buffer reuse (BLOCKED - needs GCC 14+)
