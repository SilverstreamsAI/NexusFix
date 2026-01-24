# NexusFIX Optimization Diary

How we achieved **3x performance improvement** over QuickFIX: from 730ns to 246ns.

---

## Executive Summary

| Phase | Technique | Before | After | Improvement |
|-------|-----------|--------|-------|-------------|
| 1 | Zero-Copy Parsing | 730 ns | 520 ns | 1.4x |
| 2 | O(1) Field Lookup | 520 ns | 380 ns | 1.4x |
| 3 | SIMD Delimiter Scan | 380 ns | 290 ns | 1.3x |
| 4 | Compile-Time Offsets | 290 ns | 260 ns | 1.1x |
| 5 | Cache-Line Alignment | 260 ns | 246 ns | 1.05x |
| **Total** | | **730 ns** | **246 ns** | **3.0x** |

---

## Phase 1: Zero-Copy Parsing

### Problem

QuickFIX copies every field value into `std::string`:

```cpp
// QuickFIX approach - allocates memory for each field
std::string orderID = message.getField(37);  // heap allocation
std::string symbol = message.getField(55);   // heap allocation
```

Each `getField()` call triggers:
- Heap allocation (~50-100ns)
- Memory copy
- Potential cache miss

### Solution

Use `std::span<const char>` to create views into the original buffer:

```cpp
// NexusFIX approach - zero allocation
std::span<const char> orderID = message.get_view(Tag::OrderID);  // just pointer + length
std::span<const char> symbol = message.get_view(Tag::Symbol);    // no copy
```

### Why It Works

- `std::span` is just 16 bytes (pointer + size) on stack
- No heap allocation, no `malloc()` syscall
- CPU cache stays hot - data never moves

### Result

**730ns → 520ns** (1.4x improvement)

---

## Phase 2: O(1) Field Lookup

### Problem

QuickFIX uses `std::map<int, std::string>` for field storage:

```cpp
// QuickFIX internal structure
std::map<int, std::string> fields_;  // O(log n) lookup, ~5-7 comparisons
```

Tree traversal causes:
- Multiple pointer dereferences
- Cache misses on each node visit
- Branch mispredictions

### Solution

Pre-indexed array by tag number:

```cpp
// NexusFIX internal structure
struct FieldEntry {
    uint16_t offset;  // position in buffer
    uint16_t length;  // field length
};
std::array<FieldEntry, 1024> fields_;  // O(1) direct indexing
```

Lookup is single array access:

```cpp
auto& entry = fields_[tag];  // One memory access
return std::span{buffer + entry.offset, entry.length};
```

### Why It Works

- Direct indexing: `fields_[37]` compiles to single `mov` instruction
- Array is cache-friendly - sequential memory layout
- No branch mispredictions

### Result

**520ns → 380ns** (1.4x improvement)

---

## Phase 3: SIMD Delimiter Scanning

### Problem

FIX messages use SOH (`\x01`) as field delimiter. Sequential scanning:

```cpp
// Traditional approach - 1 byte per cycle
for (size_t i = 0; i < len; ++i) {
    if (buffer[i] == '\x01') {
        // found delimiter
    }
}
```

Processing 1 byte per iteration on modern CPU is wasteful.

### Solution

AVX2 SIMD processes 32 bytes simultaneously:

```cpp
// NexusFIX approach - 32 bytes per cycle
// Performance: AVX2 scans 32 bytes/cycle vs 1 byte/cycle sequential
// This reduces delimiter detection from O(n) to O(n/32)
__m256i soh = _mm256_set1_epi8('\x01');
__m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr));
__m256i cmp = _mm256_cmpeq_epi8(chunk, soh);
uint32_t mask = _mm256_movemask_epi8(cmp);
if (mask) {
    return ptr + __builtin_ctz(mask);  // position of first SOH
}
```

### Why It Works

- Single instruction compares 32 bytes
- `_mm256_movemask_epi8` extracts comparison results to 32-bit integer
- `__builtin_ctz` finds first set bit in ~1 cycle

### Result

**380ns → 290ns** (1.3x improvement)

---

## Phase 4: Compile-Time Field Offsets

### Problem

Runtime offset calculation for well-known fields:

```cpp
// Runtime calculation
int offset = calculate_header_size(version);  // branches, function call
```

### Solution

`consteval` computes offsets at compile time:

```cpp
// Compile-time calculation
consteval size_t header_offset() {
    // 8=FIX.4.4 | 9=xxx | 35=x | ...
    return 8 + 1 + 4 + 1 + 3 + 1;  // computed during compilation
}

static constexpr size_t HEADER_OFFSET = header_offset();
```

### Why It Works

- Zero runtime cost - offset is embedded as immediate value
- Compiler can optimize subsequent code knowing exact value
- No branch for version checking in hot path

### Result

**290ns → 260ns** (1.1x improvement)

---

## Phase 5: Cache-Line Alignment

### Problem

Hot data structures crossing cache line boundaries:

```cpp
struct ParseState {
    char* buffer;        // 8 bytes
    size_t position;     // 8 bytes
    size_t length;       // 8 bytes
    FieldTable fields;   // 4096 bytes - crosses cache lines
};
```

### Solution

Align hot data to 64-byte cache lines:

```cpp
struct alignas(64) ParseState {
    // Hot data - first cache line
    char* buffer;
    size_t position;
    size_t length;
    uint32_t field_count;

    // Cold data - separate cache line
    alignas(64) FieldTable fields;
};
```

Also applied `[[gnu::hot]]` to critical functions:

```cpp
[[gnu::hot]] [[nodiscard]]
auto parse(std::span<const char> buffer) noexcept -> ParseResult;
```

### Why It Works

- Hot data fits in single cache line (64 bytes)
- `[[gnu::hot]]` hints compiler to optimize for speed over size
- Prevents false sharing in multi-threaded scenarios

### Result

**260ns → 246ns** (1.05x improvement)

---

## Additional Optimizations

### Branch Hints

```cpp
if (tag == Tag::MsgType) [[likely]] {
    // Most messages have MsgType early
    return fast_path();
} else [[unlikely]] {
    return slow_path();
}
```

### Restrict Pointers

```cpp
void parse(const char* __restrict input,
           FieldEntry* __restrict output) {
    // Compiler knows input and output don't alias
    // Enables better vectorization
}
```

### Link-Time Optimization

```cmake
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)  # -flto
```

Enables cross-file inlining and dead code elimination.

---

## Lessons Learned

1. **Measure First** - RDTSC + perf counters before any optimization
2. **Memory is King** - Most gains came from eliminating allocations
3. **Cache Matters** - Data layout impacts performance more than algorithms
4. **Compiler is Smart** - `consteval` and LTO let compiler do heavy lifting
5. **SIMD Carefully** - Only where data is naturally parallel (delimiter scanning)

---

## Benchmark Methodology

```bash
# CPU isolation
taskset -c 0 ./benchmark

# Warm-up
for (int i = 0; i < 10000; ++i) parse(msg);  // I-Cache warming

# Measurement
auto start = rdtsc_fenced();
for (int i = 0; i < 100000; ++i) parse(msg);
auto end = rdtsc_fenced();

# Statistics
# Report P50, P99, P999, min, max
```

All measurements on isolated CPU core with governor set to `performance`.

---

## References

- [Modern C++ for Quantitative Trading](modernc_quant.md) - Full technique catalog
- [Benchmark Report](compare/BENCHMARK_COMPARISON_REPORT.md) - Detailed comparison data
- Intel Intrinsics Guide - AVX2 instruction reference
