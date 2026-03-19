# NexusFIX Technical Deep-Dive: From 730ns to 246ns

**Category**: Show and Tell

This post walks through the engineering decisions behind NexusFIX - a zero-allocation FIX protocol engine written in C++23. We'll cover the optimization journey in detail, with code and assembly-level reasoning.

---

## Background

The FIX protocol (Financial Information eXchange) is the standard wire protocol for electronic trading. A typical ExecutionReport message looks like:

```
8=FIX.4.4\x019=176\x0135=8\x0149=BROKER\x0156=CLIENT\x0134=42\x0152=20260314-10:30:00\x0137=ORD001\x0117=EXEC001\x01150=F\x0139=2\x0155=AAPL\x0154=1\x0138=100\x0132=100\x0131=150.50\x0114=100\x016=150.50\x0110=128\x01
```

Fields are delimited by SOH (0x01), with tag=value pairs. Parsing this efficiently is the core challenge.

## The Optimization Stack

### Stage 1: Zero-Copy Architecture

**Decision**: Never copy field data. Use `std::span<const char>` views into the original network buffer.

```cpp
class ParsedMessage {
    std::span<const char> raw_;  // view into original buffer

    struct FieldEntry {
        uint16_t offset;  // position in raw_
        uint16_t length;  // value length
    };
    std::array<FieldEntry, 1024> fields_{};  // O(1) lookup by tag
    uint16_t field_count_ = 0;

public:
    [[nodiscard]] std::span<const char> get_view(uint16_t tag) const noexcept {
        auto& e = fields_[tag];
        return raw_.subspan(e.offset, e.length);
    }
};
```

**Why `std::span` over `std::string_view`**: Both are non-owning views, but `std::span` makes the zero-copy intent explicit. It signals "this is raw memory, not text" - important when dealing with binary-adjacent protocols.

**Impact**: 730ns -> 520ns (1.4x). The majority of QuickFIX's overhead is memory allocation.

### Stage 2: Direct Array Indexing

**Decision**: FIX tags are integers (1-1024 range covers 99% of production usage). Use them as direct array indices.

QuickFIX uses `std::map<int, std::string>`:
- Red-black tree: O(log n) with ~5-7 comparisons
- Each node is a separate heap allocation
- Pointer chasing causes cache misses

NexusFIX uses `std::array<FieldEntry, 1024>`:
- Direct indexing: `fields_[tag]` compiles to a single `mov` instruction
- Contiguous memory: cache-friendly
- 4 bytes per entry (2x uint16_t), total array is 4KB

```asm
; QuickFIX field lookup (simplified)
; ~5-7 comparisons, pointer chasing
mov rax, [rbx+node_ptr]    ; load tree node
cmp [rax+key], edi         ; compare key
jl .left_child             ; branch
; ... more comparisons

; NexusFIX field lookup
; Single indexed load
movzx eax, word ptr [rdi + rcx*4]      ; offset
movzx edx, word ptr [rdi + rcx*4 + 2]  ; length
```

**Impact**: 520ns -> 380ns (1.4x).

### Stage 3: SIMD Delimiter Scanning

**Decision**: Use AVX2 to scan for SOH delimiters 32 bytes at a time.

FIX messages are typically 100-300 bytes. Sequential scanning at 1 byte/cycle wastes the CPU's vector units.

```cpp
namespace simd {

[[gnu::hot]] inline const char*
find_soh_avx2(const char* ptr, const char* end) noexcept {
    const __m256i soh = _mm256_set1_epi8('\x01');

    while (ptr + 32 <= end) {
        __m256i chunk = _mm256_loadu_si256(
            reinterpret_cast<const __m256i*>(ptr));
        __m256i cmp = _mm256_cmpeq_epi8(chunk, soh);
        uint32_t mask = _mm256_movemask_epi8(cmp);
        if (mask) {
            return ptr + __builtin_ctz(mask);
        }
        ptr += 32;
    }

    // Scalar fallback for remaining bytes
    while (ptr < end) {
        if (*ptr == '\x01') return ptr;
        ++ptr;
    }
    return end;
}

} // namespace simd
```

For AVX-512 capable machines, we process 64 bytes per cycle:

```cpp
[[gnu::hot]] inline const char*
find_soh_avx512(const char* ptr, const char* end) noexcept {
    const __m512i soh = _mm512_set1_epi8('\x01');

    while (ptr + 64 <= end) {
        __m512i chunk = _mm512_loadu_si512(ptr);
        __mmask64 mask = _mm512_cmpeq_epi8_mask(chunk, soh);
        if (mask) {
            return ptr + __builtin_ctzll(mask);
        }
        ptr += 64;
    }
    // Fall through to AVX2 or scalar
}
```

**Runtime dispatch** via function pointer, initialized once at startup:

```cpp
using ScanFn = const char*(*)(const char*, const char*) noexcept;

ScanFn select_scanner() {
    if (__builtin_cpu_supports("avx512bw")) return find_soh_avx512;
    if (__builtin_cpu_supports("avx2"))     return find_soh_avx2;
    return find_soh_scalar;
}

static const ScanFn scan_soh = select_scanner();
```

**Impact**: 380ns -> 290ns (1.3x). SIMD scanner alone is 13x faster than scalar for isolated delimiter scanning.

### Stage 4: Compile-Time Field Offsets

**Decision**: FIX message headers have a known structure. Compute offsets at compile time with `consteval`.

```cpp
consteval size_t fix44_header_size() {
    // 8=FIX.4.4\x01  -> 10 bytes
    // 9=xxx\x01       -> 4-6 bytes (BodyLength)
    // 35=x\x01        -> 4-5 bytes (MsgType)
    return 10;  // BeginString is always exactly "8=FIX.4.4\x01"
}

static constexpr size_t HEADER_OFFSET = fix44_header_size();
```

The compiler embeds this as an immediate value. Zero runtime cost.

**Impact**: 290ns -> 260ns (1.1x).

### Stage 5: Cache-Line Optimization

**Decision**: Align hot parser state to 64-byte cache lines. Separate hot and cold data.

```cpp
struct alignas(64) ParseState {
    // Hot data - first cache line (64 bytes)
    const char* buffer;         // 8B
    size_t position;            // 8B
    size_t length;              // 8B
    uint16_t field_count;       // 2B
    uint16_t msg_type_offset;   // 2B
    uint16_t msg_type_length;   // 2B
    // 30 bytes used, 34 bytes padding

    // Cold data - separate cache line
    alignas(64) std::array<FieldEntry, 1024> fields;
};
```

Combined with `[[gnu::hot]]` on critical parsing functions:

```cpp
[[gnu::hot]] [[nodiscard]]
auto parse(std::span<const char> buffer) noexcept
    -> std::expected<ParsedMessage, ParseError>;
```

**Impact**: 260ns -> 246ns (1.05x).

## Zero Allocation Verification

We instrumented the global allocator to count allocations during parsing:

| Operation | QuickFIX | NexusFIX |
|-----------|----------|----------|
| Heap allocations | ~12 | **0** |
| `std::string` copies | ~12 | **0** |
| `std::map` node allocs | ~12 | **0** |
| Destructor calls | ~12 | **0** |

NexusFIX uses PMR pools (`std::pmr::monotonic_buffer_resource`) for any allocations outside the hot path (session management, logging).

## Benchmark Results

| Metric | QuickFIX | NexusFIX | Improvement |
|--------|----------|----------|-------------|
| ExecutionReport Parse | 730 ns | 246 ns | 3.0x |
| NewOrderSingle Parse | 661 ns | 229 ns | 2.9x |
| Field Access (4 fields) | 31 ns | 11 ns | 2.9x |
| Throughput | 1.19M msg/s | 4.17M msg/s | 3.5x |
| P99 Latency | 784 ns | 258 ns | 3.0x |

*Linux, GCC 13.3, -O3 -march=native -flto, 100K iterations, CPU isolated with `taskset`.*

Reproduce:

```bash
git clone https://github.com/SilverstreamsAI/NexusFIX.git
cd NexusFIX && ./start.sh build
./start.sh bench 100000      # NexusFIX benchmark
./start.sh compare 100000    # vs QuickFIX comparison
```

## Architecture Decisions

| Decision | Rationale |
|----------|-----------|
| Header-only library | Zero link-time overhead, full inlining |
| `std::expected` over exceptions | Deterministic control flow on hot path |
| Hand-tuned SIMD over Highway/xsimd | FIX-specific patterns need byte-level control |
| `absl::flat_hash_map` for sessions | Swiss Table SIMD probing, 31% faster than `std::unordered_map` |
| Quill for logging | Lock-free SPSC queue, 8ns median, zero blocking |

## What's Next

- Kernel bypass transport (DPDK/AF_XDP)
- Lock-free session state machine (coroutine-based)
- FPGA acceleration paths

---

MIT licensed. Questions and contributions welcome.

GitHub: [github.com/SilverstreamsAI/NexusFIX](https://github.com/SilverstreamsAI/NexusFIX)
