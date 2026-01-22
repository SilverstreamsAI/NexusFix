# TICKET_174: C++ Executor Benchmark Framework

## Overview

Establish a comprehensive benchmark framework for the C++ Executor before applying Modern C++ optimizations from [modernc_quant.md](./modernc_quant.md). This ensures data-driven optimization with measurable results.

## Motivation

- **Baseline Establishment**: Quantify current performance before optimization
- **Data-driven Decisions**: Identify actual bottlenecks, avoid premature optimization
- **Regression Prevention**: CI integration to detect performance degradation
- **Value Proof**: Every optimization must show measurable improvement

## Benchmark Dimensions

### 1. Latency Metrics

| Metric | Description | Tool |
|--------|-------------|------|
| P50 | Median latency | `std::chrono::steady_clock` |
| P99 | 99th percentile | `RDTSC` + `lfence` |
| P999 | 99.9th percentile | Hardware counters |
| Jitter | Latency variance | Statistical analysis |

### 2. Throughput Metrics

| Metric | Unit | Description |
|--------|------|-------------|
| Bars/sec | bars/s | Market data processing rate |
| Orders/sec | orders/s | Order generation rate |
| Executions/sec | exec/s | Full backtest cycle rate |

### 3. Memory Metrics

| Metric | Tool | Description |
|--------|------|-------------|
| Peak RSS | `/proc/self/status` | Maximum resident set size |
| Allocation count | Custom allocator | Number of heap allocations |
| Allocation size | Custom allocator | Total bytes allocated |
| Page faults | `perf stat` | Major/minor page faults |

### 4. CPU Metrics

| Metric | Tool | Description |
|--------|------|-------------|
| IPC | `perf stat` | Instructions per cycle |
| L1/L2/L3 miss | `perf stat` | Cache miss rates |
| Branch miss | `perf stat` | Branch misprediction rate |
| CPU cycles | `RDTSC` | Total cycles consumed |
| **iTLB miss** | `perf stat` | Instruction TLB misses (code pages) |
| **dTLB miss** | `perf stat` | Data TLB misses (data pages) |
| **CPB** | Computed | Cycles Per Bar (hardware-normalized) |

> **iTLB Miss Rationale**: With 300K+ lines of code, binary size is large. Instruction page table walks cause P99 jitter even with warm I-Cache.

> **dTLB Miss Rationale**: Large data working sets (order books, market data arrays) can span multiple huge pages. Data page table thrashing is equally deadly to instruction TLB misses.

> **CPB Rationale**: Different CPU frequencies (3.5GHz vs 5.0GHz) yield different latency in seconds, but **cycles are stable**. CPB enables cross-hardware performance comparison.

```bash
# Separate iTLB vs dTLB monitoring
perf stat -e iTLB-load-misses,iTLB-loads,dTLB-load-misses,dTLB-loads ./qnx-executor-bench
```

### 5. Hot/Cold Path Analysis

> Reference: [modernc_quant.md #22 - Instruction Cache Warming](./modernc_quant.md)

| Metric | Description | Purpose |
|--------|-------------|---------|
| Cold start latency | First execution after process start | Measure I-Cache miss impact |
| Warm latency | Latency after 10,000 iterations | Measure steady-state performance |
| Warmup delta | Cold - Warm difference | Quantify I-Cache warming effectiveness |
| Warmup iterations | Iterations to reach stable latency | Determine optimal warmup count |

**Testing Protocol**:
1. Fresh process start → measure first order latency (cold)
2. Run 10,000 iterations
3. Measure next 1,000 iterations (warm)
4. Compare P99 cold vs P99 warm

### 6. Allocator Protocol Audit

> Reference: [modernc_quant.md #16 - PMR Pools](./modernc_quant.md)

| Metric | Target | Description |
|--------|--------|-------------|
| Hot path malloc count | **0** | System allocations during execution |
| PMR pool hit rate | >99.9% | Pool allocation success rate |
| Pool exhaustion events | 0 | Times pool fell back to upstream |
| Peak pool usage | <80% capacity | Headroom for burst |

**Critical Rule**: Any `malloc`/`new` call on hot path is a **benchmark failure**.

### 7. Cache-line Contention Test

> Reference: [modernc_quant.md #9 - Cache-line Alignment](./modernc_quant.md)

| Metric | Description | Tool |
|--------|-------------|------|
| L1 invalidations | Cache line invalidations under contention | `perf stat` |
| False sharing events | Adjacent data access conflicts | `perf c2c` |
| Cross-core latency | Time to sync data between cores | Custom test |

**Testing Protocol**:
1. Spawn N threads operating on adjacent struct fields
2. Compare performance with/without `alignas(64)` padding
3. Measure L1 cache miss delta

```cpp
// Test structure WITHOUT proper alignment (expect contention)
struct BadLayout {
    std::atomic<int> counter1;  // Same cache line
    std::atomic<int> counter2;  // Same cache line - FALSE SHARING
};

// Test structure WITH proper alignment (no contention)
struct GoodLayout {
    alignas(std::hardware_destructive_interference_size)
    std::atomic<int> counter1;
    alignas(std::hardware_destructive_interference_size)
    std::atomic<int> counter2;
};
```

### 8. Data Distribution Sensitivity

> Reference: [modernc_quant.md #35 - Branch Hints](./modernc_quant.md)

| Mode | Description | Purpose |
|------|-------------|---------|
| **Steady State** | Highly regular, predictable data | Baseline best-case |
| **Chaos State** | Random, unpredictable patterns | Stress `[[likely]]`/`[[unlikely]]` |

**Testing Protocol**:
1. Run identical strategy on both datasets
2. Compare branch misprediction rates (`perf stat`)
3. Verify `[[likely]]`/`[[unlikely]]` effectiveness

**Chaos State Characteristics**:
- Random price jumps (no trend)
- Irregular volume spikes
- Alternating signal patterns (buy/sell/hold randomly distributed)

**BTB (Branch Target Buffer) Overflow Test**:

> With 300K+ lines of code, branch targets may overflow BTB capacity, causing additional latency even for correctly predicted branches.

| Metric | Tool | Description |
|--------|------|-------------|
| BTB misses | `perf stat` | `branch-load-misses` counter |
| Branch density | Analysis | Branches per KB of code |
| Address spread | `nm` | Distance between hot functions |

```bash
# Monitor BTB-related events
perf stat -e branch-instructions,branch-misses,branch-load-misses \
    ./qnx-executor-bench --bench=execution
```

**Mitigation**: Use `[[gnu::hot]]` to cluster critical functions, reducing address space spread.

### 9. pybind11 GIL Latency Test

> Reference: [modernc_quant.md #47 - Embedded Interpreter Lifecycle](./modernc_quant.md)

| Metric | Description | Target |
|--------|-------------|--------|
| GIL acquire latency | Time to acquire Python GIL from C++ | < 10us |
| **GIL hold duration** | Time from acquire to release | < 100us |
| GIL jitter | Variance in GIL acquisition time | < 5us stddev |
| First-call latency | Python init to first code execution | < 1ms |
| Callback overhead | C++ → Python → C++ round-trip | < 50us |

**Risk**: When C++ hot path calls back into Python, GIL acquisition can introduce **millisecond-level** delays, destroying determinism.

**Dual Measurement Protocol**:

1. **Acquire Latency**: Time waiting to acquire GIL (contention indicator)
2. **Hold Duration**: Time GIL is held (blocks other threads)

```cpp
// Complete GIL lifecycle measurement
uint64_t acquire_start = rdtsc_precise();
{
    py::gil_scoped_acquire acquire;
    uint64_t acquire_end = rdtsc_precise();
    uint64_t gil_acquire_cycles = acquire_end - acquire_start;  // Contention

    // Python operations here...
    py::exec("strategy.on_bar(data)");

    uint64_t release_start = rdtsc_precise();
    uint64_t gil_hold_cycles = release_start - acquire_end;  // Blocking others
}
// GIL released here
```

**Testing Protocol**:
1. Measure acquire latency (contention from other threads)
2. Measure hold duration (how long Python code runs)
3. Run 10,000 iterations, collect P50/P99/P999 for both
4. Flag if hold duration > 100us (Python code too slow)

**Hold Duration Analysis**:
- If acquire latency high but hold duration low → GIL contention problem
- If hold duration high → Python code optimization needed
- Both high → systemic GIL bottleneck

**Mitigation Strategies**:
- Batch Python calls to amortize GIL overhead
- Use `py::gil_scoped_release` during pure C++ computation
- Consider `nogil` Python code where possible (NumPy operations)
- Profile Python side with `py-spy` if hold duration excessive

### 10. Assembly Audit (Compiler Verification)

> Reference: [modernc_quant.md #35 - Branch Hints](./modernc_quant.md)

**Purpose**: Verify that compiler hints (`[[likely]]`, `[[unlikely]]`, `[[gnu::hot]]`) actually affect generated assembly. Compilers may ignore hints due to complex inlining.

| Check | Tool | Verification |
|-------|------|--------------|
| Branch layout | `objdump -d` | Likely path should be fall-through |
| Hot function placement | `nm --numeric-sort` | Hot functions clustered together |
| Inlining | `objdump -d` | Critical functions fully inlined |
| SIMD usage | `objdump -d` | AVX/SSE instructions present |

**Automated Assembly Extraction**:

```bash
#!/bin/bash
# scripts/audit_assembly.sh

BINARY="./qnx-executor"
OUTPUT_DIR="./audit/assembly"

mkdir -p $OUTPUT_DIR

# Extract hot loop assembly
objdump -d -C $BINARY | \
    awk '/on_bar/,/^$/' > $OUTPUT_DIR/on_bar.asm

# Check for branch hints effectiveness
# Likely branches should use forward jumps (jne .L+)
# Unlikely branches should use backward jumps (jne .L-)
grep -E "j(ne|e|mp)" $OUTPUT_DIR/on_bar.asm > $OUTPUT_DIR/branches.txt

# Verify SIMD instructions
grep -E "(vmov|vadd|vmul|vfma)" $OUTPUT_DIR/on_bar.asm > $OUTPUT_DIR/simd.txt

# Symbol ordering (hot functions should be adjacent)
nm --numeric-sort $BINARY | grep -E "(on_bar|process_|execute_)" > $OUTPUT_DIR/symbol_order.txt

echo "Assembly audit complete. Review $OUTPUT_DIR/"
```

**CI Integration**:

```yaml
- name: Assembly audit
  run: |
    ./scripts/audit_assembly.sh
    # Fail if likely/unlikely hints not respected
    if grep -q "jmp.*backward" audit/assembly/likely_branches.txt; then
      echo "WARNING: [[likely]] hint may not be respected"
    fi
```

**Manual Verification Checklist**:
- [ ] `[[likely]]` branches use fall-through (no jump for common case)
- [ ] `[[unlikely]]` code moved to end of function
- [ ] `[[gnu::hot]]` functions appear early in `.text` section
- [ ] Inner loops use SIMD instructions where expected
- [ ] No unexpected function calls in hot path (should be inlined)

### 11. Prefetch Distance Tuning

> Reference: [modernc_quant.md #11 - Explicit Prefetching](./modernc_quant.md)

**Problem**: Prefetch timing is hardware-dependent. Too late = latency not hidden. Too early = data evicted from L1 before use.

| Parameter | Description | Typical Range |
|-----------|-------------|---------------|
| Distance (iterations) | How many iterations ahead to prefetch | 4-64 |
| Locality hint | L1/L2/L3 target cache level | 0-3 |

**Sweep Test Protocol**:

```cpp
// Prefetch distance sweep
for (int distance = 4; distance <= 64; distance *= 2) {
    auto start = rdtsc_precise();
    for (size_t i = 0; i < data.size(); ++i) {
        if (i + distance < data.size()) {
            __builtin_prefetch(&data[i + distance], 0, 3);  // Read, L1
        }
        process(data[i]);
    }
    auto end = rdtsc_precise();
    record_result(distance, end - start);
}
```

**Output**: Find the "sweet spot" distance for target hardware.

| Hardware | Typical Sweet Spot |
|----------|-------------------|
| Intel Xeon (server) | 16-32 iterations |
| AMD EPYC | 8-16 iterations |
| Apple M-series | 32-64 iterations |

**CI Integration**: Run prefetch sweep on each target platform, store optimal values in config.

## Critical Path Benchmarks

### Phase 1: Data Loading

```
[Parquet File] → [Arrow Read] → [DataFrame] → [NumPy Array]
```

| Checkpoint | Measurement |
|------------|-------------|
| File open | Time to open Parquet |
| Column extraction | Time per column |
| Type conversion | Arrow → C++ vector |
| NumPy binding | C++ → Python transfer |

### Phase 2: Strategy Execution

```
[Python Init] → [Strategy Load] → [Bar Loop] → [Signal Generation]
```

| Checkpoint | Measurement |
|------------|-------------|
| Interpreter init | `py::scoped_interpreter` overhead |
| Module import | Strategy file loading |
| Per-bar latency | Single bar processing time |
| Callback overhead | Progress/increment callbacks |

### Phase 3: Result Collection

```
[Trade List] → [Equity Curve] → [Metrics] → [JSON Serialization]
```

| Checkpoint | Measurement |
|------------|-------------|
| Result extraction | Python → C++ transfer |
| JSON serialization | `nlohmann::json` overhead |
| File write | Result persistence |

## Benchmark Infrastructure

### Directory Structure

```
packages/executor/
├── benchmark/
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── benchmark_utils.hpp    # Timing utilities
│   │   ├── memory_tracker.hpp     # Allocation tracking
│   │   └── perf_counters.hpp      # Hardware counters
│   ├── src/
│   │   ├── bench_data_loading.cpp
│   │   ├── bench_execution.cpp
│   │   ├── bench_serialization.cpp
│   │   ├── bench_cache_contention.cpp  # Cache-line false sharing test
│   │   ├── bench_data_sensitivity.cpp  # Data distribution + BTB test
│   │   ├── bench_gil_latency.cpp       # pybind11 GIL measurement
│   │   └── bench_prefetch.cpp          # Prefetch distance tuning
│   ├── scripts/
│   │   ├── audit_assembly.sh      # Assembly verification
│   │   ├── generate_test_data.py  # Steady/Chaos data generator
│   │   └── compare_baseline.py    # Regression detection
│   └── data/
│       ├── steady_state.parquet   # Regular, predictable patterns
│       ├── chaos_state.parquet    # Random, unpredictable patterns
│       └── README.md              # Data generation methodology
└── ...
```

### Timing Utilities

```cpp
// benchmark_utils.hpp
#pragma once

#include <chrono>
#include <cstdint>
#include <sched.h>
#include <pthread.h>

namespace qnx::bench {

// High-resolution timing
struct ScopedTimer {
    using Clock = std::chrono::steady_clock;
    Clock::time_point start;
    std::chrono::nanoseconds& output;

    explicit ScopedTimer(std::chrono::nanoseconds& out)
        : start(Clock::now()), output(out) {}

    ~ScopedTimer() {
        output = Clock::now() - start;
    }
};

// RDTSC for cycle counting (basic version)
inline uint64_t rdtsc() {
    uint64_t lo, hi;
    asm volatile ("lfence; rdtsc" : "=a"(lo), "=d"(hi));
    return (hi << 32) | lo;
}

// Enhanced RDTSC with full pipeline serialization
// Reference: modernc_quant.md #98 - RDTSC Cycle Counting
// WARNING: cpuid can trigger VM Exit on virtualized servers (adds thousands of cycles)
inline uint64_t rdtsc_precise() {
    uint32_t lo, hi;
    asm volatile (
        "cpuid\n\t"  // Serialize: flush pipeline completely
        "rdtsc\n\t"
        : "=a"(lo), "=d"(hi)
        :
        : "%rbx", "%rcx"  // cpuid clobbers these
    );
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

// VM-safe RDTSC: Use lfence instead of cpuid to avoid VM Exit penalty
// Prefer this version on cloud/virtualized environments (AWS, GCP, Azure)
inline uint64_t rdtsc_vm_safe() {
    uint64_t lo, hi;
    asm volatile (
        "lfence\n\t"  // Lighter serialization, no VM Exit
        "rdtsc\n\t"
        "lfence\n\t"  // Prevent reordering after
        : "=a"(lo), "=d"(hi)
    );
    return (hi << 32) | lo;
}

// RDTSCP: read TSC with processor ID (partial ordering guarantee)
inline uint64_t rdtscp(uint32_t* aux = nullptr) {
    uint32_t lo, hi, aux_val;
    asm volatile ("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux_val));
    if (aux) *aux = aux_val;
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

// Auto-select best RDTSC based on environment
inline uint64_t rdtsc_auto(bool is_virtualized = false) {
    return is_virtualized ? rdtsc_vm_safe() : rdtsc_precise();
}

// Prevent compiler reordering
inline void compiler_barrier() {
    asm volatile("" ::: "memory");
}

// CPU Core Affinity binding
// Reference: modernc_quant.md #20 - CPU Isolation & Core Affinity
inline bool bind_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
}

// Isolate benchmark thread from OS scheduler interference
inline bool setup_benchmark_thread(int core_id, int priority = 99) {
    // Bind to specific core
    if (!bind_to_core(core_id)) return false;

    // Set real-time scheduling (requires CAP_SYS_NICE or root)
    struct sched_param param;
    param.sched_priority = priority;
    return sched_setscheduler(0, SCHED_FIFO, &param) == 0;
}

} // namespace qnx::bench
```

### Memory Tracking

```cpp
// memory_tracker.hpp
#pragma once

#include <atomic>
#include <cstddef>
#include <source_location>
#include <vector>
#include <string>

namespace qnx::bench {

// Basic allocation statistics
struct MemoryStats {
    std::atomic<size_t> allocation_count{0};
    std::atomic<size_t> deallocation_count{0};
    std::atomic<size_t> bytes_allocated{0};
    std::atomic<size_t> peak_bytes{0};

    void reset() {
        allocation_count = 0;
        deallocation_count = 0;
        bytes_allocated = 0;
        peak_bytes = 0;
    }
};

// PMR Pool audit statistics
// Reference: modernc_quant.md #16 - PMR Pools
struct PMRStats {
    std::atomic<size_t> pool_allocations{0};      // Allocations from pool
    std::atomic<size_t> upstream_allocations{0};  // Fallback to upstream (BAD)
    std::atomic<size_t> pool_deallocations{0};
    std::atomic<size_t> pool_bytes_used{0};
    std::atomic<size_t> pool_capacity{0};

    [[nodiscard]] double hit_rate() const {
        size_t total = pool_allocations + upstream_allocations;
        return total > 0 ? static_cast<double>(pool_allocations) / total : 1.0;
    }

    [[nodiscard]] double usage_ratio() const {
        return pool_capacity > 0
            ? static_cast<double>(pool_bytes_used) / pool_capacity
            : 0.0;
    }

    void reset() {
        pool_allocations = 0;
        upstream_allocations = 0;
        pool_deallocations = 0;
        pool_bytes_used = 0;
    }
};

// Hot path violation tracking
struct HotPathViolation {
    std::string location;      // Source location
    size_t size;               // Allocation size
    uint64_t timestamp_cycles; // When it occurred
};

struct HotPathAudit {
    std::atomic<bool> audit_enabled{false};
    std::atomic<size_t> violation_count{0};
    std::vector<HotPathViolation> violations;  // First N violations

    void record_violation(size_t size, const std::source_location& loc) {
        ++violation_count;
        if (violations.size() < 100) {  // Cap stored violations
            violations.push_back({
                std::string(loc.file_name()) + ":" + std::to_string(loc.line()),
                size,
                rdtsc()
            });
        }
    }

    [[nodiscard]] bool passed() const { return violation_count == 0; }
};

// Global stats accessible via tracking allocator
MemoryStats& global_memory_stats();
PMRStats& global_pmr_stats();
HotPathAudit& global_hotpath_audit();

// RAII guard to enable hot path auditing
struct ScopedHotPathAudit {
    ScopedHotPathAudit() { global_hotpath_audit().audit_enabled = true; }
    ~ScopedHotPathAudit() { global_hotpath_audit().audit_enabled = false; }
};

} // namespace qnx::bench
```

## Benchmark Execution

### Command Line Interface

```bash
# Run all benchmarks
./qnx-executor-bench --all

# Run specific benchmark
./qnx-executor-bench --bench=data_loading

# Output formats
./qnx-executor-bench --format=json --output=results.json
./qnx-executor-bench --format=csv --output=results.csv

# Iterations for statistical significance
./qnx-executor-bench --iterations=100 --warmup=10

# CPU isolation (requires root or CAP_SYS_NICE)
./qnx-executor-bench --core=3 --realtime

# Hot/cold path analysis
./qnx-executor-bench --bench=hotcold --cold-iterations=1 --warm-iterations=10000

# PMR audit mode (fails on any hot path allocation)
./qnx-executor-bench --audit-allocations --strict
```

### CI Integration

```yaml
# .github/workflows/benchmark.yml
name: Performance Benchmark

on:
  push:
    paths: ['packages/executor/**']
  pull_request:
    paths: ['packages/executor/**']

jobs:
  # ============================================
  # Functional benchmarks (no special permissions)
  # ============================================
  benchmark-functional:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Capture hardware info
        run: |
          echo "## Hardware Info" >> $GITHUB_STEP_SUMMARY
          echo "CPU Model: $(grep 'model name' /proc/cpuinfo | head -1 | cut -d: -f2)" >> $GITHUB_STEP_SUMMARY
          echo "Max Frequency: $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq 2>/dev/null || echo 'N/A') kHz" >> $GITHUB_STEP_SUMMARY
          echo "Virtualized: $(grep -q hypervisor /proc/cpuinfo && echo 'Yes' || echo 'No')" >> $GITHUB_STEP_SUMMARY
          # Store in JSON for normalization
          cat > hw_info.json << EOF
          {
            "cpu_model": "$(grep 'model name' /proc/cpuinfo | head -1 | cut -d: -f2 | xargs)",
            "max_freq_khz": $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq 2>/dev/null || echo 0),
            "is_vm": $(grep -q hypervisor /proc/cpuinfo && echo true || echo false)
          }
          EOF

      - name: Run functional benchmarks
        run: |
          ./qnx-executor-bench --format=json --output=bench.json \
            --no-realtime --vm-safe

      - name: Assembly audit
        run: |
          ./scripts/audit_assembly.sh
          echo "## Assembly Audit" >> $GITHUB_STEP_SUMMARY
          echo "Branch hints verified: $(grep -c 'fall-through' audit/assembly/branches.txt || echo 0)" >> $GITHUB_STEP_SUMMARY

      - name: Upload assembly artifacts
        uses: actions/upload-artifact@v4
        with:
          name: assembly-audit-${{ github.sha }}
          path: audit/assembly/
          retention-days: 30

      - name: Compare with baseline
        run: ./scripts/compare_benchmark.py bench.json baseline.json --threshold=5%

  # ============================================
  # Real-time benchmarks (requires CAP_SYS_NICE)
  # ============================================
  benchmark-realtime:
    runs-on: self-hosted  # Must have elevated permissions
    if: github.event_name == 'push' && github.ref == 'refs/heads/main'
    container:
      image: ubuntu:22.04
      options: --cap-add SYS_NICE --privileged
    steps:
      - uses: actions/checkout@v4

      - name: Capture hardware info
        run: |
          cat > hw_info.json << EOF
          {
            "cpu_model": "$(grep 'model name' /proc/cpuinfo | head -1 | cut -d: -f2 | xargs)",
            "max_freq_khz": $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq 2>/dev/null || echo 0),
            "is_vm": false,
            "isolated_cores": "$(cat /sys/devices/system/cpu/isolated 2>/dev/null || echo 'none')"
          }
          EOF

      - name: Run real-time benchmarks
        run: |
          ./qnx-executor-bench --format=json --output=bench_rt.json \
            --core=3 --realtime --iterations=1000

      - name: Upload real-time results
        uses: actions/upload-artifact@v4
        with:
          name: benchmark-realtime-${{ github.sha }}
          path: |
            bench_rt.json
            hw_info.json
```

**CI Test Categories**:

| Category | Runner | Permissions | Purpose |
|----------|--------|-------------|---------|
| `benchmark-functional` | `ubuntu-latest` | None | Basic regression detection |
| `benchmark-realtime` | `self-hosted` | `CAP_SYS_NICE` | Accurate P99/P999 measurement |

**Assembly Audit Artifacts**: Stored for 30 days, enabling cross-commit diff when investigating regressions.

## Baseline Targets

### Current Estimates (to be measured)

| Metric | Current | Target (Post-optimization) |
|--------|---------|---------------------------|
| Data load (1M bars) | ~500ms | <100ms |
| Per-bar latency | ~10us | <1us |
| Memory (1M bars) | ~200MB | <100MB |
| Allocations (backtest) | ~10000 | <100 |

### Strict Performance Gates

| Metric | Pass Criteria | Fail Action |
|--------|---------------|-------------|
| **Hot path malloc** | **= 0** | Block merge |
| PMR hit rate | > 99.9% | Warning |
| P99 cold/warm ratio | < 10x | Investigate |
| Branch miss rate | < 1% | Profile |
| L1 cache miss rate | < 5% | Review data layout |

### Warmup Analysis Targets

| Metric | Target |
|--------|--------|
| Cold start P99 | < 100us |
| Warm P99 | < 10us |
| Iterations to stable | < 1000 |

### Cache Contention Targets

| Metric | Target |
|--------|--------|
| False sharing speedup | > 5x with proper alignment |
| L1 miss delta | < 1% between aligned/unaligned |

### Data Sensitivity Targets

| Metric | Steady State | Chaos State | Delta |
|--------|--------------|-------------|-------|
| Branch miss rate | < 0.5% | < 2% | < 4x |
| P99 latency | baseline | < 2x baseline | acceptable |

### GIL Latency Targets

| Metric | Target | Action if exceeded |
|--------|--------|-------------------|
| Acquire latency P99 | < 10us | Investigate contention |
| Hold duration P99 | < 100us | Optimize Python code |
| Acquire/Hold ratio | > 0.1 | Batch Python calls |

### TLB Targets

| Metric | Target |
|--------|--------|
| iTLB miss rate | < 0.1% |
| dTLB miss rate | < 0.5% |

## Implementation Phases

### Phase 1: Infrastructure (Week 1)
- [ ] Create benchmark directory structure
- [ ] Implement timing utilities (`benchmark_utils.hpp`)
- [ ] Implement memory tracking (`memory_tracker.hpp`)
- [ ] Implement perf counters wrapper (`perf_counters.hpp`)
- [ ] Generate steady_state.parquet test data
- [ ] Generate chaos_state.parquet test data

### Phase 2: Core Benchmarks (Week 2)
- [ ] Data loading benchmark (`bench_data_loading.cpp`)
- [ ] Execution loop benchmark (`bench_execution.cpp`)
- [ ] Serialization benchmark (`bench_serialization.cpp`)
- [ ] End-to-end benchmark

### Phase 3: Advanced Benchmarks (Week 3)
- [ ] Cache contention benchmark (`bench_cache_contention.cpp`)
- [ ] Data sensitivity + BTB benchmark (`bench_data_sensitivity.cpp`)
- [ ] GIL latency benchmark (`bench_gil_latency.cpp`)
- [ ] Prefetch distance tuning (`bench_prefetch.cpp`)
- [ ] Hot/cold path analysis
- [ ] PMR audit integration

### Phase 4: Integration (Week 4)
- [ ] CLI interface with all options
- [ ] JSON/CSV output
- [ ] Baseline recording
- [ ] CI integration with regression gates
- [ ] Assembly audit script (`audit_assembly.sh`)

### Phase 5: Verification & Documentation (Week 5)
- [ ] Assembly audit CI integration
- [ ] Benchmark usage guide
- [ ] Result interpretation guide
- [ ] Optimization tracking log
- [ ] Test data generation methodology
- [ ] Assembly verification checklist

## Known Issues & Pitfalls

### VM Exit Penalty (cpuid)

**Problem**: `cpuid` instruction triggers VM Exit on virtualized servers (AWS, GCP, Azure), adding **thousands of cycles** to measurements.

**Detection**: If P99 shows abnormally high values (>10x expected), suspect VM Exit.

**Solution**: Use `rdtsc_vm_safe()` instead of `rdtsc_precise()` on cloud environments.

```bash
# Detect virtualization
grep -q hypervisor /proc/cpuinfo && echo "VM detected"
```

### SCHED_FIFO Permissions

**Problem**: `setup_benchmark_thread()` requires root or `CAP_SYS_NICE` capability.

**CI Impact**: Standard GitHub Actions runners do NOT have this capability.

**Solutions**:
1. **Self-hosted runner** with elevated permissions
2. **Docker with `--cap-add SYS_NICE`**
3. **Skip realtime tests** on standard CI (use `--no-realtime` flag)

```yaml
# CI workflow with capability
jobs:
  benchmark:
    runs-on: self-hosted  # Must have CAP_SYS_NICE
    container:
      image: ubuntu:22.04
      options: --cap-add SYS_NICE
```

### GIL Contention in pybind11

**Problem**: Python GIL acquisition from C++ can cause millisecond-level delays.

**Symptoms**: High variance in P99 for any benchmark involving Python callbacks.

**Mitigation**:
- Batch Python operations
- Use `py::gil_scoped_release` during C++ computation
- Profile with `py-spy` to identify GIL hotspots

## References

- [Modern C++ for Quantitative Trading](./modernc_quant.md) - 100 optimization techniques
- [Google Benchmark](https://github.com/google/benchmark) - Micro-benchmarking library
- [perf Wiki](https://perf.wiki.kernel.org/) - Linux performance tools
- [TICKET_133: V3 Architecture](../design/TICKET_133_V3_ARCHITECTURE_REFACTORING.md)
- [pybind11 GIL Documentation](https://pybind11.readthedocs.io/en/stable/advanced/misc.html#global-interpreter-lock-gil)
