# GEMINI.md

# NexusFIX: High Performance FIX Protocol Engine

**Project Status**: DEFERRED (Long-term Planning)
**Category**: Infrastructure / Trading Connectivity
**C++ Standard**: C++23

## 1. Project Overview

NexusFIX is a Modern C++ FIX protocol engine optimized for low-latency quantitative trading, designed as an alternative to QuickFIX.

**Target Performance**:

| Metric | QuickFIX | NexusFIX Target |
|--------|----------|-----------------|
| ExecutionReport Parse | 2,000-5,000 ns | < 200 ns |
| Hot Path Allocations | Multiple per message | 0 |
| P99 Latency Stability | Spiky | Flat line |

**Supported (Phase 1)**:
- FIX 4.4 (mainstream standard)
- Core message types: `NewOrderSingle` (35=D), `ExecutionReport` (35=8), `OrderCancelRequest` (35=F), `Logon/Logout/Heartbeat`

**Design Reference**: [TICKET_182_NEXUSFIX_HIGH_PERFORMANCE_FIX_ENGINE.md](/data/ws/QuantNexus/docs/design/TICKET_182_NEXUSFIX_HIGH_PERFORMANCE_FIX_ENGINE.md)

---

# CRITICAL RULES

## NO WORKAROUNDS - ROOT CAUSE ONLY

**ABSOLUTE PROHIBITION**:
- NO WORKAROUNDS of any kind
- NO BYPASSING errors or issues
- NO TEMPORARY FIXES or patches
- NO "QUICK SOLUTIONS" that avoid the real problem
- NO MOCKUP CODE - Never create placeholder, mock, or stub implementations

**MANDATORY APPROACH**:
- FIND ROOT CAUSE - Always investigate the true source of every problem
- FIX THE SOURCE - Address the actual underlying issue, never the symptoms
- DEEP INVESTIGATION - Trace problems through the complete system flow
- NO ASSUMPTIONS - Verify every hypothesis with evidence

## NO UNRELATED CODE CHANGES

**ABSOLUTE PROHIBITION**:
- NO UNRELATED MODIFICATIONS - Never modify code that is not directly related to the specific issue being addressed
- NO SCOPE CREEP - Do not expand changes beyond the identified problem area
- NO OPPORTUNISTIC FIXES - Do not fix other issues discovered during investigation unless explicitly requested
- NO FORMATTING CHANGES - Do not make cosmetic or formatting changes unrelated to the core issue
- NO REFACTORING - Do not refactor unrelated code during bug fixes

**MANDATORY APPROACH**:
- SURGICAL PRECISION - Make only the minimal changes required to fix the specific issue
- ISOLATED CHANGES - Ensure all modifications are directly traceable to the root cause
- CHANGE JUSTIFICATION - Every line changed must have a clear relationship to the problem being solved
- FOCUSED SCOPE - Maintain strict boundaries around the problem domain

## CODE REUSE MANDATORY

**ABSOLUTE PROHIBITION**: Implementing new code when existing, tested implementations can be reused.

**MANDATORY APPROACH**:
- SEARCH BEFORE IMPLEMENT - Always search for existing implementations before writing new code
- REUSE UTILITIES - Use existing ID generators, validators, constants, and response builders
- REUSE PATTERNS - Follow established validation and error handling patterns

## ENGLISH-ONLY CHARACTERS

**ABSOLUTE PROHIBITION**:
- NO NON-ENGLISH CHARACTERS - All characters in all code and documentation files must be English characters. This includes comments, variable names, and all other text.

---

# C++ Standards

**Reference**: [Modern C++ for Quantitative Trading](docs/modernc_quant.md) - 100 techniques across 14 categories.
**Benchmark**: [TICKET_003](docs/design/TICKET_003_BENCHMARK_FRAMEWORK.md) - RDTSC timing, hot path audit, perf counters (code in `/benchmark`)
**Optimization**: [TICKET_175](docs/TICKET_175_CPP_OPTIMIZATION_ROADMAP.md) - Optimization Roadmap

**Architecture**: [TICKET_001](docs/design/TICKET_001_MODULAR_ARCHITECTURE.md) - Interface/Implementation modular separation
**Implementation**: [TICKET_002](docs/design/TICKET_002_IMPLEMENTATION_PLAN.md) - 6-phase implementation plan (12 weeks)

## Mandatory Patterns (enforced for all C++ code)

- **C++23 Standard** - Use latest language features (`std::expected`, `std::format`, concepts)
- **Zero-copy Data Flow** - `std::span`, `std::move` semantics, no memcpy on hot path
- **Compile-time Optimization** - `consteval`, `constexpr`, static assertions, NTTP
- **Memory Sovereignty** - PMR pools, vector pre-allocation, cache-line alignment
- **Type Safety** - Strong types, `[[nodiscard]]`, `std::optional`, non-copyable RAII
- **Deterministic Execution** - `noexcept`, `std::chrono::steady_clock`, no exceptions on hot path

## Prohibited Patterns

- `new`/`delete` on hot paths (use PMR or object pools)
- `std::endl` (use `\n`)
- `virtual` functions in performance-critical code (use `std::variant` + `std::visit`)
- `std::shared_ptr` on hot paths (use raw pointers with clear ownership)
- Floating-point for prices (use fixed-point arithmetic)
- Dynamic memory allocation during message parsing

## Core Techniques (from C++ HPC Axioms)

| Axiom | Application |
|-------|-------------|
| #1 `consteval` Protocol Hardening | Compile-time field offset calculation |
| #40 `std::span` Zero-Copy View | Data flows from NIC to strategy without memcpy |
| #27 Coroutine Async State Machine | Logon/Heartbeat/Reconnect logic |
| #16 PMR Memory Pool | Pre-allocated buffers for message handling |
| #9 Cache Line Alignment | Prevent false sharing in hot path |
| #56 SIMD Intrinsics | Batch field parsing with AVX2/AVX-512 |
| #66 SBE Zero-copy | Schema-driven binary encoding |

---

# Modern C++ for Quantitative Trading

## Part 1: Compile-time Optimization & Metaprogramming

### 1. `consteval` Protocol Hardening
- Compute FIX/SBE protocol field offsets at compile time
- Quant Value: Collapses all parsing logic to compile time

### 2. `std::source_location` Zero-overhead Logging
- Replace `__FILE__` macros with compile-time source location
- Quant Value: Eliminates runtime string formatting overhead

### 3. User-defined Literals (UDL)
- Implement literals like `100_shares` or `5.5_ticks`
- Quant Value: Type-level unit safety prevents quantity/price confusion

### 4. `std::is_constant_evaluated()` Branch Optimization
- Same logic takes efficient path at compile time, safe path at runtime

### 5. NTTP (Non-Type Template Parameters) String Passing
- Pass symbol names as template parameters
- Quant Value: Generates symbol-specific optimized assembly

### 6. `std::remove_cvref_t` Perfect Forwarding
- Ensure perfect value category preservation in generic callbacks

### 7. Static Reflection (Simulated)
- Template metaprogramming for automatic serialization generation

### 8. Concepts-based Strategy Constraints
- Define `template <typename T> concept IsStrategy = ...`
- Quant Value: Compile-time rejection of non-conforming types

## Part 2: Memory Sovereignty & Cache Optimization

### 9. Cache-line Alignment & False Sharing Elimination
- Use `alignas(std::hardware_destructive_interference_size)`
- Quant Value: Ensures read/write pointers on different cache lines

### 10. `std::assume_aligned` SIMD Optimization
- Inform compiler that data is properly aligned
- Quant Value: Triggers optimal AVX-512/SIMD instructions

### 11. Explicit Prefetching
- Use `__builtin_prefetch` before order logic execution
- Quant Value: Pre-loads data into L1 cache

### 12. Huge Pages (2MB/1GB) Support
- Framework-level huge page memory allocation
- Quant Value: Reduces TLB misses

### 13. Placement New on Shared Memory
- Construct objects directly on pre-allocated shared memory

### 14. `std::uninitialized_copy` Batch Operations
- Direct byte-stream manipulation on hot paths

### 15. Flat Data Structures
- Use `std::vector` to simulate trees/graphs
- Quant Value: Memory contiguity maximizes L1 cache hit rate

### 16. PMR (Polymorphic Memory Resources) Pools
- Use `std::pmr::monotonic_buffer_resource`
- Quant Value: O(1) allocation on hot paths with no syscalls

### 17. Columnar Storage
- Store fields in separate arrays rather than struct arrays
- Quant Value: Cache-friendly layout for vectorized operations

### 18. Vector Reserve + Push Pattern
- Pre-allocate vector capacity before bulk insertion
- Quant Value: Eliminates reallocations during data loading

## Part 3: Execution Precision & Determinism

### 19. `std::atomic` Memory Ordering Control
- Use `memory_order_acquire/release` instead of `seq_cst`
- Quant Value: Minimizes memory barriers

### 20. CPU Isolation & Core Affinity
- Encapsulate process affinity settings in code
- Quant Value: Trading thread exclusive ownership of physical cores

### 21. `noexcept` No-Exception Guarantee
- Mark entire library with `noexcept`
- Quant Value: No unwind tables, reduced binary size

### 22. Instruction Cache Warming
- Loop through core paths before market open
- Quant Value: Ensures instructions resident in I-Cache

### 23. Lock-free MPSC Queue
- Multi-Producer-Single-Consumer queue for market data

### 24. Spin-lock with `_mm_pause()` Hint
- Use pause instruction in spin loops

### 25. Avoid `std::endl`
- Use `\n` instead of `std::endl`

### 26. `std::expected` Zero-cost Error Handling
- Use `std::expected<Value, Error>` for failures
- Quant Value: Deterministic control flow without exception overhead

### 27. Coroutines for Async State Machines
- C++20 coroutines for async operations

### 28. `std::chrono::steady_clock` Precision Timing
- Use steady clock for execution time measurement

## Part 4: Type System & Safety

### 29. `std::variant` + `std::visit` De-virtualization
- Replace virtual functions with static polymorphism

### 30. Tag Dispatching
- Select optimal logic per exchange at compile time

### 31. `[[nodiscard]]` Enforcement
- Mark all APIs with `[[nodiscard]]`

### 32. Opaque Typedefs (Strong Types)
- Distinguish `Price` from `Volume` even when both are `double`

### 33. `std::visit` Multi-dispatch
- Match multiple `std::variant` arguments simultaneously

### 34. `consteval` Hardcoded Limits
- Compile-time validation of parameters

### 35. `[[likely]]` / `[[unlikely]]` Branch Hints
- Guide compiler on branch probability

### 36. Non-copyable / Non-movable Resource Owners
- Delete copy/move constructors for state-owning classes

### 37. `std::optional<T>` Nullable Fields
- Explicit optional semantics for configuration fields

## Part 5: Modern Architecture & Engineering

### 38. C++20 Modules
- Replace header includes with module imports

### 39. `std::format` Safe Formatting
- Use `std::format` for non-hot-path logging

### 40. `std::span<const std::byte>` Zero-copy Views
- Use span views across all interfaces

### 41. Structured Bindings
- Destructure multi-value returns

### 42. Range-based Algorithms (`std::ranges`)
- Use ranges for data transformations

### 43. Static Assertions for Layout
- Compile-time struct size validation

### 44. `[[maybe_unused]]` Attribute
- Suppress warnings for conditionally-used variables

### 45. `std::filesystem` Cross-platform I/O
- Portable filesystem operations

---

# TICKET_174: C++ Benchmark Framework

## Overview

Establish a comprehensive benchmark framework before applying Modern C++ optimizations. This ensures data-driven optimization with measurable results.

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
| Messages/sec | msg/s | FIX message processing rate |
| Parse/sec | parse/s | Field parsing rate |

### 3. Memory Metrics

| Metric | Tool | Description |
|--------|------|-------------|
| Peak RSS | `/proc/self/status` | Maximum resident set size |
| Allocation count | Custom allocator | Number of heap allocations |
| Allocation size | Custom allocator | Total bytes allocated |

### 4. CPU Metrics

| Metric | Tool | Description |
|--------|------|-------------|
| IPC | `perf stat` | Instructions per cycle |
| L1/L2/L3 miss | `perf stat` | Cache miss rates |
| Branch miss | `perf stat` | Branch misprediction rate |
| CPU cycles | `RDTSC` | Total cycles consumed |
| iTLB miss | `perf stat` | Instruction TLB misses |
| dTLB miss | `perf stat` | Data TLB misses |
| CPB | Computed | Cycles Per Bar (hardware-normalized) |

### 5. Hot/Cold Path Analysis

| Metric | Description | Purpose |
|--------|-------------|---------|
| Cold start latency | First execution after process start | Measure I-Cache miss impact |
| Warm latency | Latency after 10,000 iterations | Measure steady-state performance |
| Warmup delta | Cold - Warm difference | Quantify I-Cache warming effectiveness |

### 6. Allocator Protocol Audit

| Metric | Target | Description |
|--------|--------|-------------|
| Hot path malloc count | **0** | System allocations during execution |
| PMR pool hit rate | >99.9% | Pool allocation success rate |
| Pool exhaustion events | 0 | Times pool fell back to upstream |

**Critical Rule**: Any `malloc`/`new` call on hot path is a **benchmark failure**.

### 7. Cache-line Contention Test

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

## Timing Utilities

```cpp
// benchmark_utils.hpp
namespace nfx::bench {

// High-resolution timing
inline uint64_t rdtsc() {
    uint64_t lo, hi;
    asm volatile ("lfence; rdtsc" : "=a"(lo), "=d"(hi));
    return (hi << 32) | lo;
}

// VM-safe RDTSC
inline uint64_t rdtsc_vm_safe() {
    uint64_t lo, hi;
    asm volatile (
        "lfence\n\t"
        "rdtsc\n\t"
        "lfence\n\t"
        : "=a"(lo), "=d"(hi)
    );
    return (hi << 32) | lo;
}

// CPU Core Affinity binding
inline bool bind_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
}

} // namespace nfx::bench
```

## Baseline Targets

| Metric | Current | Target (Post-optimization) |
|--------|---------|---------------------------|
| ExecutionReport parse | ~5000ns | <200ns |
| Per-message latency | ~10us | <1us |
| Hot path allocations | ~100 | 0 |

---

# TICKET_175: C++ Optimization Roadmap

## Optimization Phases

### Phase 1: Zero-copy Message Pipeline

| Technique | Reference | Target | Metric |
|-----------|-----------|--------|--------|
| `std::span` views | #40 | parser.hpp | Allocations |
| Move semantics | #53 | message types | Copy count |
| Vector pre-allocation | #18 | buffers | Reallocs |

**Success Criteria**:
- Hot path allocations: 0
- Memory copies: 0

### Phase 2: Memory Sovereignty

| Technique | Reference | Target | Metric |
|-----------|-----------|--------|--------|
| PMR monotonic buffer | #16 | memory_pool.hpp | malloc count |
| Cache-line alignment | #9 | message structs | L1 miss |
| Huge pages | #12 | large buffers | TLB miss |

**Success Criteria**:
- Hot path malloc: **0**
- L1 cache miss: < 5%

### Phase 3: Execution Determinism

| Technique | Reference | Target | Metric |
|-----------|-----------|--------|--------|
| `noexcept` guarantee | #21 | All headers | Binary size |
| `std::expected` errors | #26 | error handling | Exception count |
| Branch hints | #35 | Hot loops | Branch miss |
| I-Cache warming | #22 | startup | Cold/warm ratio |

**Success Criteria**:
- P99 cold/warm ratio: < 10x
- Branch miss rate: < 1%

### Phase 4: Compile-time Optimization

| Technique | Reference | Target | Metric |
|-----------|-----------|--------|--------|
| `consteval` field offsets | #1, #34 | parser.hpp | Runtime checks |
| Strong types | #32 | types.hpp | Type errors |
| `constexpr` parsing | #91 | field extraction | Compile time |

**Success Criteria**:
- Field offset calculation: compile-time only
- Type confusion bugs: 0

### Phase 5: SIMD & Low-level

| Technique | Reference | Target | Metric |
|-----------|-----------|--------|--------|
| SIMD intrinsics | #56 | simd_parser.hpp | Throughput |
| Prefetch tuning | #11 | Data loops | L1 miss |
| Branch-free code | #84 | Hot conditionals | Branch miss |

**Success Criteria**:
- Parse throughput: 8x baseline

### Phase 6: Lock-free Session Management

| Technique | Reference | Target | Metric |
|-----------|-----------|--------|--------|
| Lock-free queue | #64 | session.hpp | Contention |
| Atomic operations | #65 | sequence numbers | Lock count |
| Coroutine state machine | #27 | session lifecycle | Complexity |

**Success Criteria**:
- Lock contention: 0 on hot path

## Optimization Workflow

```
For Each Optimization:
  1. Baseline    - Run benchmark framework
  2. Profile     - Identify bottleneck with perf
  3. Implement   - Apply modernc_quant.md technique
  4. Verify      - Assembly audit (objdump)
  5. Benchmark   - Compare with baseline
  6. Document    - Record improvement in log
```

## Success Metrics Summary

| Metric | Target |
|--------|--------|
| ExecutionReport parse | < 200 ns |
| Per-message latency | < 1 us |
| Hot path allocations | 0 |
| P99/P50 ratio | < 2x |
| Branch miss rate | < 1% |
| L1 cache miss | < 5% |

---

# Project Structure

```
nexusfix/
├── CMakeLists.txt
├── GEMINI.md                    # This file
├── include/
│   └── nexusfix/
│       ├── parser.hpp           # consteval-based parser
│       ├── messages/
│       │   ├── new_order_single.hpp
│       │   ├── execution_report.hpp
│       │   └── order_cancel.hpp
│       ├── session.hpp          # Session management (coroutine-based)
│       ├── transport.hpp        # Network abstraction
│       ├── types.hpp            # Strong types (Price, Volume, etc.)
│       ├── memory_pool.hpp      # PMR pools
│       └── benchmark_utils.hpp  # Timing utilities
├── src/
│   ├── parser.cpp
│   ├── session.cpp
│   └── transport.cpp
├── codegen/
│   └── generate_from_xml.py     # XML -> C++ struct generator
├── benchmarks/
│   └── vs_quickfix/             # Performance comparison
└── tests/
```

## Code Estimation

| Module | Lines | Description |
|--------|-------|-------------|
| consteval Parser Engine | 500-800 | Core parsing with compile-time offsets |
| Message Structs (codegen) | 400-600 | Auto-generated from XML |
| Session Management | 300-400 | Logon, Heartbeat, Reconnect |
| Transport Layer | 300-500 | io_uring or standard socket |
| **Total** | **1,500-2,300** | Core library |

---

# Reference Libraries

| Library | Reference Point | What to Learn |
|---------|-----------------|---------------|
| SBE (Simple Binary Encoding) | Schema-based codegen | Replace string parsing with struct mapping |
| Aeron | Transport layer | Shared memory IPC, lock-free queues |
| Disruptor (LMAX) | Core pipeline | Single-threaded consumer, cache-line padding |
| io_uring | System calls | Kernel bypass for network I/O |

---

# Communication

- No workarounds, no temporary solutions, no assumptions
- Always recommend best practices
- Short responses preferred

```