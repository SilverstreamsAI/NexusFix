# TICKET_003: NexusFIX Benchmark Framework

**Status**: IMPLEMENTED
**Category**: Performance / Testing
**Code Location**: `/benchmark`

## 1. Overview

High-precision benchmark framework for measuring FIX engine performance. Adapted from QuantNexus TICKET_174.

## 2. Core Components

### 2.1 Header Files (`benchmark/include/`)

| File | Purpose |
|------|---------|
| `benchmark_utils.hpp` | RDTSC timing, CPU affinity, latency statistics |
| `memory_tracker.hpp` | Allocation tracking, hot path audit |
| `perf_counters.hpp` | Linux perf_event hardware counters |

### 2.2 Benchmark Executables (`benchmark/src/`)

| Executable | Purpose | Target |
|------------|---------|--------|
| `bench_fix_parse` | FIX message parsing latency | < 200 ns |
| `bench_memory_pool` | PMR pool vs std::allocator | 0 hot path malloc |
| `bench_simd_scanner` | AVX2 vs scalar SOH scanning | 8x speedup |
| `nfx-bench` | Unified CLI runner | - |

## 3. Key Features

### 3.1 High-Precision Timing

```cpp
// VM-safe RDTSC (use on cloud environments)
uint64_t cycles = nfx::bench::rdtsc_vm_safe();

// Convert to nanoseconds
double ns = nfx::bench::cycles_to_ns(cycles, freq_ghz);
```

### 3.2 Hot Path Allocation Audit

```cpp
{
    nfx::bench::ScopedHotPathAudit audit;
    // Any malloc here is a VIOLATION
    parse_fix_message(msg);
}
// audit.passed() == true means zero allocations
```

### 3.3 Hardware Performance Counters

```cpp
nfx::bench::PerfCounterGroup counters;
counters.add(PerfEvent::CPU_CYCLES);
counters.add(PerfEvent::L1D_READ_MISS);
counters.add(PerfEvent::BRANCH_MISSES);

{
    ScopedPerfCounters scope(counters);
    // Code to measure
}

auto results = counters.read();
auto metrics = DerivedMetrics::compute(results);
// metrics.ipc, metrics.branch_miss_rate, etc.
```

### 3.4 Latency Statistics

```cpp
nfx::bench::LatencyStats stats;
stats.compute(samples);  // vector<double> of latencies

// Access percentiles
stats.p50_ns;   // Median
stats.p99_ns;   // 99th percentile
stats.p999_ns;  // 99.9th percentile
```

## 4. CLI Usage

```bash
# Run all benchmarks
nfx-bench --all

# Run specific benchmarks
nfx-bench --bench=parse,memory

# Output JSON
nfx-bench --all --format=json --output=results.json

# Bind to CPU core for stable results
nfx-bench --all --cpu=2

# VM-safe mode (for cloud environments)
nfx-bench --all --vm-safe

# Custom iterations
nfx-bench --bench=parse --iterations=50000
```

## 5. Performance Targets

| Metric | Target | Validation |
|--------|--------|------------|
| ExecutionReport parse | < 200 ns | `bench_fix_parse` |
| Hot path allocations | 0 | `bench_memory_pool` |
| SIMD vs scalar speedup | > 4x | `bench_simd_scanner` |
| P99/P50 ratio | < 2x | All benchmarks |
| Branch miss rate | < 1% | `nfx-bench --bench=perf` |
| L1 cache miss rate | < 5% | `nfx-bench --bench=perf` |

## 6. Build Configuration

```cmake
# CMakeLists.txt options
option(NFX_BENCHMARK_ENABLE_PERF "Enable perf_event counters" ON)
option(NFX_BENCHMARK_TRACK_ALLOCATIONS "Enable allocation tracking" OFF)

# Compiler flags
-O3 -march=native -mavx2 -fno-omit-frame-pointer
```

## 7. Benchmark Development Guide

### Adding New Benchmark

```cpp
// 1. Create src/bench_new_feature.cpp
#include "benchmark_utils.hpp"
#include "memory_tracker.hpp"

using namespace nfx::bench;

int main() {
    // Warmup
    warmup([&]{ /* code */ }, 1000);

    // Measure
    auto stats = benchmark([&]{ /* code */ }, 1000, 10000);

    // Report
    std::cout << "P50: " << stats.p50_ns << " ns\n";
    return 0;
}

// 2. Add to CMakeLists.txt
add_executable(bench_new_feature src/bench_new_feature.cpp)
target_link_libraries(bench_new_feature PRIVATE nfx_bench_utils)
```

### Registering in CLI Runner

```cpp
// In bench_runner.cpp
std::map<std::string, BenchmarkFunc> get_benchmark_registry() {
    return {
        {"parse", run_parse_bench},
        {"memory", run_memory_bench},
        {"new_feature", run_new_feature_bench},  // Add here
    };
}
```

## 8. Integration with CI

```yaml
# .github/workflows/benchmark.yml
- name: Run Benchmarks
  run: |
    ./build/benchmark/nfx-bench --all --format=json --output=bench.json
    python scripts/check_regression.py bench.json
```

## 9. File Summary

```
benchmark/
├── CMakeLists.txt           # Build configuration
├── README.md                # Usage documentation
├── include/
│   ├── benchmark_utils.hpp  # Timing utilities (307 lines)
│   ├── memory_tracker.hpp   # Allocation tracking (343 lines)
│   └── perf_counters.hpp    # Hardware counters (434 lines)
└── src/
    ├── bench_fix_parse.cpp      # FIX parsing benchmark
    ├── bench_memory_pool.cpp    # Memory pool benchmark
    ├── bench_simd_scanner.cpp   # SIMD scanner benchmark
    └── bench_runner.cpp         # Unified CLI runner
```
