# NexusFix: Zero-Allocation FIX Protocol Engine in C++23

**Excerpt**: NexusFix is a zero-allocation C++23 FIX engine for HFT. 3x faster than QuickFIX with SIMD-accelerated parsing and zero heap allocations on the hot path.

**Category**: Open Source | **Tags**: nexusfix, fix-protocol, c++23, quant-trading, open-source, hft

---

The FIX protocol has been the backbone of electronic trading since 1992. Over three decades later, most FIX engines still parse messages the same way: allocate strings, copy bytes, traverse tree structures. In a world where nanoseconds determine profitability, that approach is a liability.

We built [NexusFIX](https://github.com/SilverstreamsAI/NexusFIX) to fix that.

## The Problem with Legacy FIX Engines

QuickFIX is the industry workhorse. It's battle-tested, widely deployed, and... slow. Here's what happens when QuickFIX parses an ExecutionReport:

- **~12 heap allocations** per message (`std::string` copies, `std::map` node insertions)
- **O(log n) field lookup** via `std::map` tree traversal (5-7 pointer chases per access)
- **Byte-by-byte scanning** for delimiters (1 byte per CPU cycle)
- **730 nanoseconds** average parse latency

For a market maker processing millions of messages per day, those allocations add up. Each `malloc` is a potential cache miss. Each tree traversal is a branch misprediction waiting to happen. And the garbage collector... well, QuickFIX is C++, so at least there's no GC pause. But ~12 destructors per message is its own kind of tax.

## NexusFIX: Designed for Zero

NexusFIX takes a different approach: **zero heap allocations on the hot path**. Not "few." Zero.

| Technique | QuickFIX | NexusFIX |
|-----------|----------|----------|
| Memory | Heap allocation per message | Zero-copy `std::span` views |
| Field Lookup | O(log n) `std::map` | O(1) direct array indexing |
| Parsing | Byte-by-byte scanning | AVX2 SIMD (32 bytes/cycle) |
| Field Offsets | Runtime calculation | `consteval` compile-time |
| Error Handling | Exceptions | `std::expected` (no throw) |

The result:

| Metric | QuickFIX | NexusFIX | Improvement |
|--------|----------|----------|-------------|
| ExecutionReport Parse | 730 ns | 246 ns | **3.0x faster** |
| NewOrderSingle Parse | 661 ns | 229 ns | **2.9x faster** |
| Field Access (4 fields) | 31 ns | 11 ns | **2.9x faster** |
| Throughput | 1.19M msg/s | 4.17M msg/s | **3.5x higher** |
| P99 Latency | 784 ns | 258 ns | **3.0x lower** |

*Tested on Linux, GCC 13.3, 100,000 iterations.*

## How We Got to 246ns

The optimization wasn't a single breakthrough. It was five compounding phases:

### Phase 1: Zero-Copy Parsing (730ns -> 520ns)

Replace `std::string` copies with `std::span<const char>` views into the original buffer:

```cpp
// QuickFIX: heap allocation per field
std::string orderID = message.getField(37);  // malloc + memcpy

// NexusFIX: zero allocation
std::span<const char> orderID = message.get_view(Tag::OrderID);  // pointer + length
```

A `std::span` is 16 bytes on the stack. No heap, no copy, no destructor.

### Phase 2: O(1) Field Lookup (520ns -> 380ns)

Replace `std::map<int, std::string>` with a pre-indexed array:

```cpp
struct FieldEntry {
    uint16_t offset;  // position in buffer
    uint16_t length;  // field length
};
std::array<FieldEntry, 1024> fields_;  // O(1) by tag number

auto& entry = fields_[tag];  // single mov instruction
```

### Phase 3: SIMD Delimiter Scanning (380ns -> 290ns)

FIX uses SOH (0x01) as field delimiter. AVX2 processes 32 bytes per cycle instead of 1:

```cpp
__m256i soh = _mm256_set1_epi8('\x01');
__m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr));
uint32_t mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, soh));
if (mask) return ptr + __builtin_ctz(mask);
```

### Phase 4: Compile-Time Offsets (290ns -> 260ns)

`consteval` computes FIX header offsets at compile time. Zero runtime cost.

### Phase 5: Cache-Line Alignment (260ns -> 246ns)

`alignas(64)` keeps hot data in a single cache line. `[[gnu::hot]]` hints the compiler to optimize critical functions for speed.

## Quick Start

```cpp
#include <nexusfix/nexusfix.hpp>
using namespace nfx;
using namespace nfx::fix44;

// Send an order - zero allocation
MessageAssembler asm_;
NewOrderSingle::Builder order;
auto msg = order
    .cl_ord_id("ORD001")
    .symbol("AAPL")
    .side(Side::Buy)
    .order_qty(Qty::from_int(100))
    .ord_type(OrdType::Limit)
    .price(FixedPrice::from_double(150.50))
    .build(asm_);
transport.send(msg);

// Parse incoming - zero copy
FixParser parser;
auto result = parser.parse(raw_buffer);
if (result) {
    auto order_id = result->get_string(Tag::OrderID);  // O(1)
    auto fill_qty = result->get_qty(Tag::LastQty);      // type-safe
}
```

Header-only library. C++23 compiler required (GCC 13+ or Clang 17+).

```bash
git clone https://github.com/SilverstreamsAI/NexusFIX.git
cd NexusFIX && ./start.sh build
```

## Architecture

NexusFIX was built by studying 11 high-performance C++ libraries and extracting their core techniques:

| Source | What We Took | Result |
|--------|-------------|--------|
| hffix | `consteval` offsets + O(1) indexing | 14ns field access |
| Abseil | `flat_hash_map` for sessions | 31% faster lookup |
| Quill | Lock-free SPSC logging | 8ns median, zero blocking |
| liburing | DEFER_TASKRUN + registered buffers | 7-27% faster I/O |
| Rigtorp | Cache-line padded SPSC queue | 88M ops/sec |

FIX 4.4 and FIX 5.0 (FIXT 1.1) are both supported, with only 2% overhead for version detection.

## What's Next

NexusFIX is MIT licensed and actively developed. The roadmap includes kernel bypass transport (DPDK/AF_XDP), FPGA acceleration paths, and lock-free session management.

If you're building low-latency trading infrastructure and QuickFIX is your bottleneck, give NexusFIX a look.

GitHub: [github.com/SilverstreamsAI/NexusFIX](https://github.com/SilverstreamsAI/NexusFIX)

---

*Built by [SilverstreamsAI](https://github.com/SilverstreamsAI). For questions or collaboration: contact@silverstream.tech*
