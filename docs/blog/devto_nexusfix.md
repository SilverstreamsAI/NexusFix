---
title: "NexusFix: Zero-Allocation FIX Protocol Engine in C++23"
published: true
tags: cpp, performance, trading, opensource
canonical_url: https://silverstream.tech/blog/nexusfix-zero-allocation-fix-engine
---

# NexusFix: Zero-Allocation FIX Protocol Engine in C++23

I've been working on a FIX protocol engine that takes a fundamentally different approach to message parsing. Instead of the allocate-copy-traverse pattern that QuickFIX has used for 20+ years, NexusFIX does **zero heap allocations** on the hot path.

Here's what that looks like in practice.

## The Allocation Problem

Every time QuickFIX parses an ExecutionReport, it makes roughly 12 heap allocations. Each field value gets copied into a `std::string`. Fields are stored in a `std::map<int, std::string>`, which means tree node allocations and O(log n) lookups with pointer chasing.

For a typical ExecutionReport parse, that adds up to **730 nanoseconds**.

In HFT, 730ns is an eternity. At the speed of light, a signal travels 219 meters in that time. If your counterparty's engine is faster, you're seeing stale prices.

## What We Did Differently

NexusFIX uses five techniques to get that down to **246 nanoseconds** (3x improvement):

**1. `std::span` instead of `std::string`**

```cpp
// QuickFIX: allocates
std::string orderID = message.getField(37);

// NexusFIX: just a view (pointer + length)
std::span<const char> orderID = message.get_view(Tag::OrderID);
```

**2. Array indexing instead of tree traversal**

```cpp
// O(1) lookup by tag number
std::array<FieldEntry, 1024> fields_;
auto& entry = fields_[tag];  // compiles to single mov
```

**3. AVX2 SIMD for delimiter scanning**

FIX uses SOH (0x01) as delimiter. Instead of checking 1 byte at a time, AVX2 checks 32 bytes simultaneously:

```cpp
__m256i soh = _mm256_set1_epi8('\x01');
__m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr));
uint32_t mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, soh));
```

**4. `consteval` for compile-time field offsets**

FIX headers have known structure. `consteval` computes offsets during compilation - zero runtime cost.

**5. Cache-line alignment**

`alignas(64)` keeps hot parser state in a single 64-byte cache line. No cache line splits, no false sharing.

## The Numbers

| Metric | QuickFIX | NexusFIX | Improvement |
|--------|----------|----------|-------------|
| ExecutionReport Parse | 730 ns | 246 ns | 3.0x |
| Field Access (4 fields) | 31 ns | 11 ns | 2.9x |
| Throughput | 1.19M msg/s | 4.17M msg/s | 3.5x |
| Hot path allocations | ~12 | 0 | - |

Tested on Linux, GCC 13.3, 100K iterations.

## Try It

Header-only, C++23. Three commands:

```bash
git clone https://github.com/SilverstreamsAI/NexusFIX.git
cd NexusFIX && ./start.sh build

# Run benchmarks yourself
./start.sh bench 100000
```

Send an order:

```cpp
#include <nexusfix/nexusfix.hpp>
using namespace nfx::fix44;

MessageAssembler asm_;
auto msg = NewOrderSingle::Builder{}
    .cl_ord_id("ORD001")
    .symbol("AAPL")
    .side(Side::Buy)
    .order_qty(Qty::from_int(100))
    .ord_type(OrdType::Limit)
    .price(FixedPrice::from_double(150.50))
    .build(asm_);
```

MIT licensed. FIX 4.4 and 5.0 supported.

GitHub: [github.com/SilverstreamsAI/NexusFIX](https://github.com/SilverstreamsAI/NexusFIX)

---

## Discussion

I'm curious what the community thinks:

- **What FIX engine are you using in production?** QuickFIX, hffix, something proprietary?
- **Has anyone else applied SIMD to protocol parsing?** We took inspiration from simdjson's structural indexing approach.
- **What's your hot path allocation budget?** We targeted zero, but is that overkill for most use cases?

Would love to hear about your FIX infrastructure challenges.
