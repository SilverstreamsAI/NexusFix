# TICKET_023: Tag Metadata Benchmark

**Date**: 2026-02-01
**Ticket**: TICKET_023 (Compile-time Optimization Roadmap - Phase 4)
**Optimization**: Add compile-time tag metadata with sparse lookup table

---

## Summary

Added tag metadata (name, is_header, is_required) with compile-time template specializations and a sparse lookup table for O(1) runtime queries.

| Metric | Before | After |
|--------|--------|-------|
| Tag metadata functions | 0 | 3 (tag_name, is_header_tag, is_required_tag) |
| Lookup table entries | - | 200 (sparse, covers tags 0-199) |
| Average improvement | - | **62%** |

---

## Implementation

1. **TagInfo<TagNum>** - Template specializations for 39 common FIX tags
2. **Sparse lookup table** - 200 entries covering most common tag numbers
3. **Dual API**: Compile-time `tag_name<35>()` and runtime `tag_name(35)`

### Tag Coverage

| Category | Tags | Count |
|----------|------|-------|
| Header | 8, 9, 35, 49, 56, 34, 52, 43, 97, 122 | 10 |
| Trailer | 10 | 1 |
| Session | 98, 108, 141, 112, 45, 58 | 6 |
| Order | 11, 55, 54, 38, 40, 44, 99, 59, 60, 1, 21 | 11 |
| Execution | 37, 17, 150, 39, 151, 14, 6, 31, 32, 41 | 10 |
| **Total** | | **38** |

---

## Benchmark Results

Environment:
- CPU: Intel (VM environment)
- Compiler: GCC with -O3 -march=native
- Iterations: 10,000,000 per test

### Per Function

| Function | Cases | OLD (cycles) | NEW (cycles) | Improvement |
|----------|-------|--------------|--------------|-------------|
| tag_name() | 39 | 4.86 | 1.19 | **76%** |
| is_header_tag() | 10 | 1.76 | 1.02 | **42%** |
| is_required_tag() | 8 | 2.42 | 1.12 | **54%** |
| Hot path (7 tags) | - | 2.32 | 0.79 | **66%** |
| Random access | - | 4.81 | 1.28 | **73%** |

### Analysis

All functions show significant improvement:
- **tag_name() (39 cases): 76%** - Largest switch, biggest gain
- **Random access: 73%** - Lookup tables excel for unpredictable patterns
- **Hot path: 66%** - Even predictable header tag access benefits
- **is_header/is_required: 42-54%** - Smaller switches still improve

---

## Code Changes

**File**: `include/nexusfix/types/tag.hpp`

### New Template Specializations
```cpp
namespace detail {

template<int TagNum>
struct TagInfo {
    static constexpr std::string_view name = "";
    static constexpr bool is_header = false;
    static constexpr bool is_required = false;
};

template<> struct TagInfo<8> {
    static constexpr std::string_view name = "BeginString";
    static constexpr bool is_header = true;
    static constexpr bool is_required = true;
};
// ... 37 more specializations
```

### Sparse Lookup Table
```cpp
inline constexpr int MAX_COMMON_TAG = 200;

consteval std::array<TagEntry, MAX_COMMON_TAG> create_tag_table() {
    std::array<TagEntry, MAX_COMMON_TAG> table{};
    // Initialize all as unknown
    for (auto& entry : table) {
        entry = {"", false, false};
    }
    // Populate known tags
    table[8] = {TagInfo<8>::name, TagInfo<8>::is_header, TagInfo<8>::is_required};
    // ...
    return table;
}

inline constexpr auto TAG_TABLE = create_tag_table();
```

### Runtime Query
```cpp
[[nodiscard]] inline constexpr std::string_view tag_name(int tag_num) noexcept {
    if (tag_num >= 0 && tag_num < detail::MAX_COMMON_TAG) [[likely]] {
        return detail::TAG_TABLE[tag_num].name;
    }
    return "";
}
```

---

## Benefits

1. **Unified tag metadata**: Single source of truth for tag information
2. **Compile-time queries**: `tag_name<35>()` resolves at compile time
3. **O(1) runtime lookup**: Sparse table covers common tags efficiently
4. **Debugging/logging**: Easy access to human-readable tag names
5. **Validation**: `is_header_tag()` and `is_required_tag()` for message validation

---

## Memory Overhead

| Component | Size |
|-----------|------|
| Sparse table | 200 entries x 24 bytes = 4.8 KB |
| Template instantiations | Minimal (compile-time only) |

The 4.8 KB overhead is negligible and fits entirely in L1 cache.

---

## Conclusion

Phase 4 delivers **62% average improvement** across all tag metadata functions. The sparse lookup table approach provides excellent performance for the most common FIX tags while keeping memory overhead minimal.

### TICKET_023 Progress

| Phase | Target | Status | Key Improvement |
|-------|--------|--------|-----------------|
| 1 | Error Messages | **DONE** | 73-85% |
| 2 | Session State | **DONE** | 66-85% |
| 3 | FIX Version | **DONE** | 74-97% |
| 4 | Tag Metadata | **DONE** | 42-76% (avg 62%) |
| 5 | Field Types | Pending | - |
