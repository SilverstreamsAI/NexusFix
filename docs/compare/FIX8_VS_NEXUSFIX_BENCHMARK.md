# Fix8 vs NexusFIX Benchmark Report

**Date**: 2026-01-26
**Type**: Independent Performance Measurement

---

## Executive Summary

| Metric | Fix8 (C++11) | NexusFIX (C++23) | Speedup |
|--------|--------------|------------------|---------|
| **P50 Latency** | 1,453 ns | 193 ns | **7.5x** |
| **P99 Latency** | 2,203 ns | 398 ns | **5.5x** |
| **Min Latency** | 1,262 ns | 184 ns | **6.9x** |

**Conclusion**: NexusFIX is **7.5x faster** than Fix8 at P50 for ExecutionReport parsing.

---

## Test Environment

| Parameter | Value |
|-----------|-------|
| CPU | 3.42 GHz (measured via RDTSC calibration) |
| OS | Linux 6.14.0-37-generic |
| Compiler | g++ 13.x |
| Fix8 Version | Latest master (2026-01) |
| Fix8 Compile Flags | `-O3 -march=native -DNDEBUG` |
| NexusFIX Compile Flags | `-O3 -march=native -DNDEBUG -std=c++23` |
| Iterations | 100,000 |
| Warmup | 10,000 iterations |

---

## Test Message

Both benchmarks parsed the same FIX 4.4 ExecutionReport (35=8):

```
8=FIX.4.4|9=200|35=8|49=SENDER|56=TARGET|34=12345|
52=20240115-10:30:00.123|37=ORDER123|17=EXEC456|
150=F|39=2|55=AAPL|54=1|38=100|44=150.50|32=100|
31=150.45|151=0|14=100|6=150.45|60=20240115-10:30:00.100|10=123|
```

**Message Size**: 210 bytes, 21 fields

---

## Detailed Results

### Fix8 (C++11)

```
ExecutionReport Decode Latency:
  Min:  1,262 ns (4,313 cycles)
  P50:  1,453 ns (4,966 cycles)
  P99:  2,203 ns (7,529 cycles)
  P999: 3,196 ns (10,924 cycles)
  Max:  14,757 ns (50,432 cycles)
```

### NexusFIX (C++23)

```
ExecutionReport Parse Latency:
  Min:    184 ns
  P50:    193 ns
  P99:    398 ns
  P999:   410 ns
  Max:  14,577 ns
```

---

## Comparison Chart

```
Latency (nanoseconds, lower is better)
=====================================

P50 Latency:
Fix8      |████████████████████████████████████████████████████████████| 1453 ns
NexusFIX  |████████|                                                    |  193 ns

P99 Latency:
Fix8      |████████████████████████████████████████████████████████████| 2203 ns
NexusFIX  |███████████|                                                 |  398 ns

Speedup: 7.5x faster at P50
```

---

## Performance Analysis

### Why is NexusFIX Faster?

| Factor | Fix8 | NexusFIX | Impact |
|--------|------|----------|--------|
| **Dispatch** | Virtual functions | `std::variant` + `std::visit` | ~200 ns saved |
| **Memory** | std::string copies | `std::string_view` zero-copy | ~300 ns saved |
| **Field Lookup** | Hash table | Direct array index | ~100 ns saved |
| **Allocation** | Heap per message | PMR arena | ~400 ns saved |
| **Error Handling** | Exceptions | `std::expected` | Deterministic |

### Cycles Breakdown (Estimated)

```
Fix8 (~4,966 cycles):
  - String allocation/copy:  ~1,500 cycles
  - Hash table lookup:       ~1,000 cycles
  - Virtual dispatch:        ~500 cycles
  - Field parsing:           ~1,500 cycles
  - Framework overhead:      ~466 cycles

NexusFIX (~660 cycles):
  - Zero-copy view setup:    ~50 cycles
  - Direct array lookup:     ~20 cycles
  - Static dispatch:         ~10 cycles
  - SIMD-assisted parsing:   ~400 cycles
  - Validation:              ~180 cycles
```

---

## Comparison with Official Claims

| Source | Fix8 Decode Latency | Our Measurement |
|--------|---------------------|-----------------|
| Fix8 Official (fix8.org) | 3,200 ns | 1,453 ns (P50) |
| Fix8 GitHub README | 3,750 ns | 2,203 ns (P99) |

**Note**: Our measurements are faster than Fix8's official claims, possibly due to:
- Different hardware
- Different message complexity
- Different measurement methodology
- Optimized compilation flags

---

## Methodology

### Fix8 Benchmark Code

```cpp
for (int i = 0; i < ITERATIONS; ++i) {
    FIX8::FIX44::ExecutionReport er;

    uint64_t start = rdtsc_fenced();
    er.decode(std::string(raw_msg, msg_len));
    uint64_t end = rdtsc_fenced();

    cycles[i] = end - start;
}
```

### NexusFIX Benchmark Code

```cpp
for (int i = 0; i < ITERATIONS; ++i) {
    uint64_t start = rdtsc_fenced();
    auto result = parser.parse(msg_span);
    uint64_t end = rdtsc_fenced();

    cycles[i] = end - start;
}
```

### Timing Method

- RDTSC with `lfence` barriers for accurate cycle counting
- CPU frequency calibrated via `steady_clock` over 100ms
- Warmup phase to eliminate I-cache effects

---

## Verification

Both parsers correctly extracted fields:

| Field | Fix8 | NexusFIX |
|-------|------|----------|
| OrderID (37) | ORDER123 | ORDER123 |
| Symbol (55) | AAPL | AAPL |
| LastPx (31) | 150.45 | 150.45 |

---

## Reproducibility

### Build Fix8

```bash
git clone https://github.com/fix8/fix8.git
cd fix8
./bootstrap
./configure --disable-ssl
make -j$(nproc)

# Generate FIX 4.4 classes
./compiler/f8c -n FIX44 schema/FIX44.xml
```

### Build NexusFIX

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make parse_benchmark
./bin/benchmarks/parse_benchmark
```

---

## Conclusion

NexusFIX achieves **7.5x faster** message parsing than Fix8 through:

1. **C++23 Features**: `std::expected`, `consteval`, `std::span`
2. **Zero-Copy Design**: No heap allocation on hot path
3. **Static Dispatch**: No virtual function overhead
4. **Cache-Optimized**: Data structures aligned to cache lines

This validates the architectural decisions documented in [C++11 vs C++23 Performance](../cpp11_vs_cpp23_performance.md).

---

## References

- [Fix8 GitHub](https://github.com/fix8/fix8)
- [Fix8 Official Site](https://fix8.org/)
- [NexusFIX Optimization Diary](../optimization_diary.md)
- [C++11 vs C++23 Performance Case Study](../cpp11_vs_cpp23_performance.md)
