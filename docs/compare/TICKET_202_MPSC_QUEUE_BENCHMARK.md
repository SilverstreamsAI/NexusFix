# TICKET_202: MPSC Queue Benchmark - Before/After Comparison

**Date**: 2026-01-29
**CPU**: 3.42 GHz (x86_64)
**Queue Capacity**: 4096 entries
**Message Size**: 64 bytes (cache-line aligned)

---

## Summary

| Queue Type | Producers | Throughput | Latency | vs SPSC |
|------------|-----------|------------|---------|---------|
| SPSC (baseline) | 1 | 34.74 M/s | 28.78 ns | --- |
| **MPSC** | 1 | 32.84 M/s | 30.45 ns | 0.95x |
| **MPSC** | 2 | 25.25 M/s | 39.61 ns | 0.73x |
| **MPSC** | 4 | 16.10 M/s | 62.12 ns | 0.46x |
| **MPSC** | 8 | 11.17 M/s | 89.56 ns | 0.32x |
| SimpleMPSC | 1 | 11.61 M/s | 86.10 ns | 0.33x |
| SimpleMPSC | 2 | 10.88 M/s | 91.90 ns | 0.31x |
| SimpleMPSC | 4 | 10.10 M/s | 98.97 ns | 0.29x |
| SimpleMPSC | 8 | 7.09 M/s | 141.08 ns | 0.20x |

**Recommendation**: Use `MPSCQueue` (sequence array design) for best performance.

---

## Analysis

### MPSC Queue (Sequence Array Design)

- **Single Producer**: Only 5.5% overhead vs SPSC (excellent!)
- **Scaling**: Throughput decreases with more producers due to CAS contention
- **Design**: Per-slot sequence numbers allow independent slot claiming

### SimpleMPSC Queue (Claim-Publish Design)

- **Single Producer**: 3x slower than SPSC (significant overhead)
- **Scaling**: Better relative scaling but lower absolute throughput
- **Design**: Global claim/publish counters create serialization point

---

## Implementation Details

### MPSCQueue (Recommended)

```cpp
// Per-slot sequence array - each slot tracks its own state
// Sequence == slot_index: ready for write
// Sequence == slot_index + 1: ready for read
struct PaddedSequence {
    alignas(64) std::atomic<size_t> value;
};
std::array<PaddedSequence, Capacity> sequences_;
```

**Advantages**:
- Minimal contention between producers claiming different slots
- Lock-free CAS on head pointer
- Single producer nearly matches SPSC performance

### SimpleMPSCQueue

```cpp
// Global sequence counters - all producers serialize here
alignas(64) std::atomic<size_t> claim_head_;    // Claim slot
alignas(64) std::atomic<size_t> publish_head_;  // Publish ordering
```

**Disadvantages**:
- All producers must wait in line to publish (FIFO ordering)
- Creates serialization bottleneck

---

## Wait Strategies Implemented

| Strategy | Description | Use Case |
|----------|-------------|----------|
| `BusySpinWait` | CPU pause instruction | HFT hot path |
| `YieldingWait` | `std::this_thread::yield()` | Active trading |
| `SleepingWait<N>` | Sleep N microseconds | Background tasks |
| `BackoffWait` | Adaptive spin->yield->sleep | General purpose |

---

## Files Added

| File | Description |
|------|-------------|
| `include/nexusfix/memory/wait_strategy.hpp` | Wait strategy abstractions |
| `include/nexusfix/memory/mpsc_queue.hpp` | MPSC queue implementations |
| `benchmarks/mpsc_queue_bench.cpp` | Performance benchmark |

---

## Use Cases

1. **Market Data Aggregation**: Multiple feed handlers -> Strategy thread
2. **Order Response Fan-in**: Multiple exchange connections -> Session manager
3. **Logging**: Multiple application threads -> Single log writer
4. **Metrics**: Multiple components -> Metrics collector

---

## Recommendations

1. **For 1-2 producers**: Use `MPSCQueue` - nearly SPSC performance
2. **For many producers**: Consider multiple SPSC queues per producer
3. **For logging**: `MPSCQueue` with `BackoffWait` for power efficiency
4. **For HFT**: `MPSCQueue` with `BusySpinWait` for lowest latency

---

## Raw Benchmark Output

```
======================================================================
  MPSC Queue Benchmark
======================================================================

CPU Frequency: 3.42 GHz
Queue Capacity: 4096 entries
Message Size: 64 bytes

SPSC (baseline):        34.74 M msg/s,  28.78 ns/op
MPSC 1P:                32.84 M msg/s,  30.45 ns/op  (0.95x SPSC)
MPSC 2P:                25.25 M msg/s,  39.61 ns/op  (0.73x SPSC)
MPSC 4P:                16.10 M msg/s,  62.12 ns/op  (0.46x SPSC)
MPSC 8P:                11.17 M msg/s,  89.56 ns/op  (0.32x SPSC)

SimpleMPSC 1P:          11.61 M msg/s,  86.10 ns/op  (0.33x SPSC)
SimpleMPSC 2P:          10.88 M msg/s,  91.90 ns/op  (0.31x SPSC)
SimpleMPSC 4P:          10.10 M msg/s,  98.97 ns/op  (0.29x SPSC)
SimpleMPSC 8P:           7.09 M msg/s, 141.08 ns/op  (0.20x SPSC)
```
