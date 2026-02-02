# TICKET_204: Aeron IPC Benchmark

## Overview

Aeron high-performance messaging evaluation for NexusFix IPC. Compares Aeron IPC (shared memory) and UDP latency against MPSC Queue baseline.

## Test Environment

- **CPU**: 3.42 GHz (calibrated via RDTSC)
- **Iterations**: 100,000 (one-way), 10,000 (round-trip)
- **Message Sizes**: 64B, 256B, 1024B
- **Aeron Version**: 1.46.7
- **Build**: Release (-O3 -march=native)
- **Media Driver**: External process (aeronmd)

## Test Scenarios

1. **One-Way Latency**: Producer -> Aeron/MPSC -> Consumer
2. **Round-Trip Latency**: Ping-Pong (half RTT reported)
3. **Message Sizes**: 64B (cache-line), 256B, 1024B

## Results

### MPSC Queue Baseline (Aeron driver unavailable)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DNFX_ENABLE_AERON=ON
cmake --build build -j
./build/bin/benchmarks/aeron_ipc_bench
```

### Round-Trip Latency (Primary Metric)

| Transport | Size | P50 | P99 | P999 | Throughput |
|-----------|------|-----|-----|------|------------|
| **MPSC RTT** | 64B | **88.1 ns** | 156.5 ns | 175.9 ns | 10.32 M/s |
| Aeron IPC RTT | 64B | TBD | TBD | TBD | TBD |

### One-Way Latency

| Transport | Size | P50 | P99 | P999 |
|-----------|------|-----|-----|------|
| MPSC Queue | 64B | ~30-50 ns* | varies | varies |
| Aeron IPC | 64B | TBD | TBD | TBD |

*Note: One-way latency varies due to thread scheduling. Round-trip (ping-pong) provides more stable measurements.*

### Aeron UDP (Reference)

| Transport | Size | P50 | P99 |
|-----------|------|-----|-----|
| Aeron UDP | 64B | TBD | TBD |

## Expected Results

| Transport | Expected P50 | Notes |
|-----------|-------------|-------|
| MPSC Queue | ~30-50 ns | In-process, lock-free |
| Aeron IPC | ~100-200 ns | Cross-process capable |
| Aeron UDP | ~1-5 us | Network transport |

## Decision Criteria

- If Aeron IPC < 200ns P50: **Viable** for internal messaging
- If Aeron IPC overhead > 5x MPSC: Use MPSC for in-process, Aeron for cross-process only

## Aeron Configuration

```cpp
// Embedded Media Driver
aeron::driver::Context ctx;
ctx.aeronDir("/dev/shm/aeron-nexusfix-bench");
ctx.threadingMode(aeron::driver::Configuration::SHARED);
ctx.dirDeleteOnStart(true);
ctx.dirDeleteOnShutdown(true);

// Client
aeron::Context client_ctx;
client_ctx.aeronDir(aeron_dir);
client_ctx.preTouchMappedMemory(true);
```

## Channels

| Channel | Description |
|---------|-------------|
| `aeron:ipc` | Shared memory IPC (lowest latency) |
| `aeron:udp?endpoint=127.0.0.1:40123` | UDP loopback |

## Files Added

| File | Description |
|------|-------------|
| `benchmarks/aeron_ipc_bench.cpp` | Aeron vs MPSC benchmark |
| `CMakeLists.txt` | NFX_ENABLE_AERON option |
| `docs/compare/TICKET_204_AERON_IPC_BENCHMARK.md` | This report |

## Build Instructions

```bash
# Enable Aeron (optional dependency)
cmake -B build -DCMAKE_BUILD_TYPE=Release -DNFX_ENABLE_AERON=ON

# Build
cmake --build build -j

# Run benchmark
./build/bin/benchmarks/aeron_ipc_bench
```

## Current Status

**Phase 1 Complete**: Aeron optional dependency added. MPSC baseline benchmarked.

**To complete Aeron benchmarks**:
1. Install Aeron media driver: `sudo apt install aeron-driver` or use Java driver
2. Run benchmark: `./build/bin/benchmarks/aeron_ipc_bench`

## Preliminary Conclusion

Based on MPSC round-trip baseline (~84 ns P50), expected comparison:

| Transport | Expected P50 | Use Case |
|-----------|-------------|----------|
| MPSC Queue | ~30-50 ns | In-process, same address space |
| Aeron IPC | ~100-200 ns | Cross-process, shared memory |
| Aeron UDP | ~1-5 us | Network transport |

**Recommendation**: Use MPSC for in-process messaging, Aeron for cross-process IPC when needed.

## References

- [Aeron GitHub](https://github.com/real-logic/aeron)
- [Aeron Design Overview](https://github.com/real-logic/aeron/wiki/Design-Overview)
- [LMAX Disruptor](https://lmax-exchange.github.io/disruptor/)
