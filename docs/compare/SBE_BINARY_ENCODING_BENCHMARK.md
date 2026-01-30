# TICKET_203: SBE Binary Encoding Benchmark

## Overview

SBE (Simple Binary Encoding) implementation for NexusFix internal IPC. Compares SBE binary decode latency vs FIX text parsing.

## Test Environment

- **CPU**: 3.417 GHz (calibrated via RDTSC)
- **Iterations**: 100,000 per test
- **Message**: ExecutionReport (15 fields)
- **Build**: Release (-O3 -march=native)

## Results

### SBE Decode Performance

| Operation | Min | P50 | P99 | P99.9 |
|-----------|-----|-----|-----|-------|
| Single field | 14.93 ns | 25.46 ns | 31.90 ns | 35.42 ns |
| All 15 fields | 33.37 ns | 35.12 ns | 45.07 ns | 56.20 ns |
| Dispatch + Decode | 14.93 ns | 16.68 ns | 20.78 ns | 23.12 ns |

### SBE Encode Performance

| Operation | Min | P50 | P99 | P99.9 |
|-----------|-----|-----|-----|-------|
| All fields | 12.00 ns | 13.17 ns | 16.10 ns | 16.68 ns |

### FIX Text Parse (Baseline)

| Operation | Min | P50 | P99 | P99.9 |
|-----------|-----|-----|-----|-------|
| ExecutionReport | 188.49 ns | 192.88 ns | 247.91 ns | 308.50 ns |

## Comparison

| Metric | SBE (all fields) | FIX Text | Speedup |
|--------|------------------|----------|---------|
| P50 | 35.12 ns | 192.88 ns | **5.5x** |
| P99 | 45.07 ns | 247.91 ns | **5.5x** |

## Message Sizes

| Message | SBE Size | Typical FIX Text |
|---------|----------|------------------|
| NewOrderSingle | 64 bytes | ~200 bytes |
| ExecutionReport | 144 bytes | ~350 bytes |

## Key Features

- Zero-copy flyweight decode
- 8-byte aligned int64 fields
- Fluent encode API
- Type-safe dispatch by templateId

## Conclusion

SBE provides **5.5x speedup** over FIX text parsing with smaller message sizes. Ready for integration with MPSC queue (TICKET_204) when IPC is needed.
