# Reddit r/cpp Post

**Subreddit**: r/cpp

**Title**: NexusFIX: Zero-allocation FIX protocol engine in C++23 (3x faster than QuickFIX)

---

**Post Body**:

I've been building a FIX protocol parser that uses modern C++23 to eliminate all heap allocations on the hot path. The core techniques:

- `std::span<const char>` views instead of `std::string` copies (zero-copy parsing)
- O(1) field lookup via direct array indexing (vs QuickFIX's `std::map` O(log n))
- AVX2 SIMD for SOH delimiter scanning (32 bytes/cycle)
- `consteval` compile-time field offsets
- `std::expected` error handling (no exceptions on hot path)

**Benchmark** (Linux, GCC 13.3, 100K iterations):

- ExecutionReport parse: 246ns vs QuickFIX 730ns (3x)
- Throughput: 4.17M msg/s vs 1.19M msg/s (3.5x)
- Hot path allocations: 0 vs ~12

Header-only library, MIT licensed. FIX 4.4 and 5.0 supported.

GitHub: https://github.com/SilverstreamsAI/NexusFIX

Happy to answer questions about the SIMD parsing approach or the optimization journey (documented in the [optimization diary](https://github.com/SilverstreamsAI/NexusFIX/blob/main/docs/optimization_diary.md)).
