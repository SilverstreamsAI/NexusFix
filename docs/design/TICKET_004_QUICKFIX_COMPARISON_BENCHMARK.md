# TICKET_004: QuickFIX Comparison Benchmark

## Overview

Implement benchmark tests to compare NexusFIX performance against QuickFIX, the industry-standard FIX engine. This provides concrete evidence of NexusFIX performance improvements.

## Objective

Measure and compare:
- Parse latency (NexusFIX vs QuickFIX)
- Field access latency
- Message construction throughput
- Memory allocation patterns

## Target Metrics

| Metric | QuickFIX (Expected) | NexusFIX Target | Improvement |
|--------|---------------------|-----------------|-------------|
| ExecutionReport Parse | 2,000-5,000 ns | < 200 ns | 10-25x |
| Field Access | ~100 ns | < 15 ns | 6-7x |
| Throughput | ~50K msg/sec | > 500K msg/sec | 10x |

## Implementation

### Directory Structure

```
benchmarks/
├── vs_quickfix/
│   ├── CMakeLists.txt
│   └── quickfix_parse_benchmark.cpp
```

### Dependencies

- `libquickfix-dev` (installed via apt-get)
- Headers: `/usr/include/quickfix/`
- Library: `-lquickfix`

### Test Messages

Use identical FIX 4.4 messages for both engines:
- ExecutionReport (35=8)
- NewOrderSingle (35=D)
- Heartbeat (35=0)

### Benchmark Functions

1. **QuickFIX Parse Benchmark**
   - `FIX::Message::setString()` - parse from raw string
   - Measure latency with RDTSC

2. **QuickFIX Field Access Benchmark**
   - `FIX::FieldMap::getField()` - retrieve field values
   - Compare O(1) vs O(log n) lookup

3. **Comparison Output**
   - Side-by-side latency comparison
   - Speedup ratio calculation

## Success Criteria

- [ ] QuickFIX benchmark compiles and runs
- [ ] Produces comparable metrics to NexusFIX benchmarks
- [ ] Demonstrates >10x parse latency improvement
- [ ] Results documented in benchmark output

## References

- TICKET_003: Benchmark Framework
- QuickFIX Documentation: https://quickfixengine.org/
