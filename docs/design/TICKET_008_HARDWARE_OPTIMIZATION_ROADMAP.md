# TICKET_008: Hardware-Level Optimization Roadmap

**Status**: PLANNED
**Category**: Performance / Hardware Optimization
**Priority**: Phase 2+
**Created**: 2026-01-22

## 1. Overview

This ticket documents the hardware-level optimizations identified as missing in the current NexusFIX implementation. These features are planned for implementation in subsequent phases following the optimization roadmap.

**Source**: Investigation report (`docs/compare/NEXUSFIX_INVESTIGATION_REPORT.md`)

## 2. Current State Analysis

### 2.1 Phase 1 Completed Features

| Feature | Status | Location |
|---------|--------|----------|
| consteval Parser | **Implemented** | `parser/consteval_parser.hpp` |
| SIMD Scanner (AVX2) | **Implemented** | `parser/simd_scanner.hpp` |
| PMR Buffer Pool | **Implemented** | `memory/buffer_pool.hpp` |
| Zero-copy (`std::span`) | **Implemented** | Throughout parser |
| Strong Types | **Implemented** | `types/field_types.hpp` |
| Cache-line Alignment | **Implemented** | `alignas(64)` on critical structs |

### 2.2 Missing Features (Phase 2+)

| Feature | Code Status | Documentation Status |
|---------|-------------|---------------------|
| Huge Pages | **Not Implemented** | Defined in CLAUDE.md |
| Prefetching (`__builtin_prefetch`) | **Not Implemented** | Defined in CLAUDE.md |
| Hardware Counters (`perf_event_open`) | **Not Implemented** | Defined in TICKET_003 |
| CPU Core Pinning | **Not Implemented** | Defined in TICKET_003 |
| Kernel Bypass (AF_XDP) | **Not Implemented** | Out of scope |

### 2.3 Code-Level Performance Issues

**Source**: Investigation report (`test/1.txt`)

| Issue | Location | Impact | Priority |
|-------|----------|--------|----------|
| Timestamp Generation | `session_manager.hpp:578-588` | High latency on hot path | **HIGH** |
| `std::function` Callbacks | `session_manager.hpp:28-43` | Indirect branch, potential allocation | MEDIUM |
| MessageStore Missing | N/A | Cannot handle ResendRequest (35=2) | **HIGH** |
| Resend Logic TODO | `session_manager.hpp:436` | Non-compliant FIX implementation | **HIGH** |

#### 2.3.1 Timestamp Generation (CRITICAL)

**Current Implementation**:
```cpp
// session_manager.hpp:578-588
[[nodiscard]] std::string_view current_timestamp() noexcept {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
    gmtime_r(&time_t_now, &tm_buf);  // <-- SLOW: system call
    int len = std::snprintf(timestamp_buf_, sizeof(timestamp_buf_),
                           "%04d%02d%02d-%02d:%02d:%02d.%03d",
                           ...);  // <-- SLOW: formatting
}
```

**Problem**: Called on every outgoing message. `gmtime_r` + `snprintf` adds ~500ns+ latency.

**Solution**: Cached timestamp formatter with fast digit updates.

#### 2.3.2 std::function Callbacks

**Current Implementation**:
```cpp
// session_manager.hpp:28-43
struct SessionCallbacks {
    std::function<void(const ParsedMessage&)> on_app_message;
    std::function<void(SessionState, SessionState)> on_state_change;
    std::function<bool(std::span<const char>)> on_send;
    std::function<void(const SessionError&)> on_error;
    std::function<void()> on_logon;
    std::function<void(std::string_view)> on_logout;
};
```

**Problem**: `std::function` uses type erasure, causing:
- Indirect branch (vtable-like dispatch)
- Potential heap allocation for large callables

**Solution**: Template-based policy pattern for compile-time dispatch.

#### 2.3.3 MessageStore (COMPLIANCE CRITICAL)

**Current State**: No implementation exists.

**Impact**: Cannot correctly handle FIX `ResendRequest` (35=2) for message recovery.

**Required Interface**:
```cpp
class IMessageStore {
public:
    virtual void store(uint32_t seq_num, std::span<const char> msg) = 0;
    virtual std::optional<std::span<const char>> retrieve(uint32_t seq_num) = 0;
    virtual void set_next_sender_seq_num(uint32_t seq) = 0;
    virtual void set_next_target_seq_num(uint32_t seq) = 0;
};
```

#### 2.3.4 Resend Logic TODO

**Current Code**:
```cpp
// session_manager.hpp:434-437
void handle_resend_request(const ParsedMessage& msg) noexcept {
    ++stats_.resend_requests_sent;
    // TODO: Implement message resend from store
    // For now, send sequence reset (gap fill)
    ...
}
```

**Impact**: Currently sends `SequenceReset` (gap fill) instead of actual message replay.

## 3. Implementation Plan

### 3.1 Phase 2: Memory Sovereignty

**Target**: Reduce TLB misses, eliminate hot path allocations

| Task | Reference | File | Priority |
|------|-----------|------|----------|
| Huge Page Allocator | CLAUDE.md #12 | `memory/huge_page_allocator.hpp` | HIGH |
| `mmap` with `MAP_HUGETLB` | - | `memory/huge_page_allocator.hpp` | HIGH |
| THP (Transparent Huge Pages) detection | - | `memory/huge_page_allocator.hpp` | MEDIUM |

**Implementation**:

```cpp
// memory/huge_page_allocator.hpp
namespace nfx::memory {

class HugePageAllocator {
public:
    static void* allocate(size_t size) {
        void* ptr = mmap(nullptr, size,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
                        -1, 0);
        if (ptr == MAP_FAILED) {
            // Fallback to standard allocation
            ptr = std::aligned_alloc(4096, size);
        }
        return ptr;
    }

    static void deallocate(void* ptr, size_t size) {
        munmap(ptr, size);
    }
};

} // namespace nfx::memory
```

**Success Criteria**:
- TLB miss rate: < 0.5%
- Hot path malloc: 0

### 3.2 Phase 5: SIMD & Low-level

**Target**: Hardware-level prefetching and branch optimization

| Task | Reference | File | Priority |
|------|-----------|------|----------|
| Prefetch utilities | CLAUDE.md #11 | `include/nexusfix/prefetch.hpp` | MEDIUM |
| Prefetch distance tuning | TICKET_174 | Benchmark | MEDIUM |
| Branch-free parsing | CLAUDE.md #84 | Parser hot paths | LOW |

**Implementation**:

```cpp
// include/nexusfix/prefetch.hpp
namespace nfx {

// Prefetch for read, L1 cache
inline void prefetch_read(const void* ptr) noexcept {
    __builtin_prefetch(ptr, 0, 3);  // Read, high locality (L1)
}

// Prefetch for write
inline void prefetch_write(void* ptr) noexcept {
    __builtin_prefetch(ptr, 1, 3);  // Write, high locality
}

// Prefetch N cache lines ahead
template<size_t CacheLines = 8>
inline void prefetch_ahead(const void* base, size_t offset) noexcept {
    constexpr size_t CACHE_LINE = 64;
    const char* ptr = static_cast<const char*>(base) + offset;
    for (size_t i = 0; i < CacheLines; ++i) {
        __builtin_prefetch(ptr + i * CACHE_LINE, 0, 3);
    }
}

} // namespace nfx
```

**Success Criteria**:
- L1 cache miss: < 5%
- Parse throughput: 8x baseline

### 3.3 Code-Level Performance Fixes

**Target**: Fix hot path performance issues identified in investigation

#### 3.3.1 Fast Timestamp Generator

| Task | File | Priority |
|------|------|----------|
| Cached timestamp formatter | `include/nexusfix/util/fast_timestamp.hpp` | **HIGH** |
| `std::to_chars` for digit conversion | Same | **HIGH** |
| Millisecond-only update optimization | Same | MEDIUM |

**Implementation**:

```cpp
// include/nexusfix/util/fast_timestamp.hpp
namespace nfx::util {

class FastTimestamp {
public:
    // Update only when second changes, fast path for milliseconds
    [[nodiscard]] std::string_view get() noexcept {
        auto now = std::chrono::system_clock::now();
        auto now_sec = std::chrono::floor<std::chrono::seconds>(now);

        if (now_sec != cached_second_) [[unlikely]] {
            update_date_time(now_sec);  // Slow path: ~200ns
            cached_second_ = now_sec;
        }

        // Fast path: only update milliseconds (~10ns)
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - now_sec).count();
        update_milliseconds(static_cast<int>(ms));

        return {buffer_, TIMESTAMP_LEN};
    }

private:
    void update_milliseconds(int ms) noexcept {
        // Direct digit write, no snprintf
        buffer_[18] = '0' + (ms / 100);
        buffer_[19] = '0' + ((ms / 10) % 10);
        buffer_[20] = '0' + (ms % 10);
    }

    alignas(64) char buffer_[24];  // "YYYYMMDD-HH:MM:SS.mmm"
    std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds> cached_second_;
    static constexpr size_t TIMESTAMP_LEN = 21;
};

} // namespace nfx::util
```

**Success Criteria**:
- Timestamp generation: < 50 ns (was ~500+ ns)
- Hot path: millisecond update only (~10 ns)

#### 3.3.2 Template-Based Callbacks (Optional)

| Task | File | Priority |
|------|------|----------|
| Policy-based SessionManager | `include/nexusfix/session/session_manager.hpp` | LOW |
| CRTP callback interface | Same | LOW |

**Note**: This is a larger refactor. `std::function` overhead is acceptable for non-hot-path callbacks (logon/logout). Consider only for `on_app_message` if profiling shows impact.

### 3.4 Compliance Features

**Target**: FIX protocol compliance for production use

#### 3.4.1 MessageStore Implementation

| Task | File | Priority |
|------|------|----------|
| `IMessageStore` interface | `include/nexusfix/store/i_message_store.hpp` | **HIGH** |
| Memory-mapped store | `include/nexusfix/store/mmap_message_store.hpp` | **HIGH** |
| Ring buffer store (optional) | `include/nexusfix/store/ring_message_store.hpp` | MEDIUM |

**Implementation**:

```cpp
// include/nexusfix/store/i_message_store.hpp
namespace nfx::store {

class IMessageStore {
public:
    virtual ~IMessageStore() = default;

    // Store outgoing message for potential replay
    virtual void store(uint32_t seq_num, std::span<const char> msg) = 0;

    // Retrieve message by sequence number
    [[nodiscard]] virtual std::optional<std::span<const char>>
        retrieve(uint32_t seq_num) const = 0;

    // Sequence number persistence
    virtual void set_next_sender_seq_num(uint32_t seq) = 0;
    virtual void set_next_target_seq_num(uint32_t seq) = 0;
    [[nodiscard]] virtual uint32_t get_next_sender_seq_num() const = 0;
    [[nodiscard]] virtual uint32_t get_next_target_seq_num() const = 0;

    // Range retrieval for resend
    [[nodiscard]] virtual std::vector<std::span<const char>>
        retrieve_range(uint32_t begin_seq, uint32_t end_seq) const = 0;
};

} // namespace nfx::store
```

#### 3.4.2 Resend Logic Implementation

| Task | File | Priority |
|------|------|----------|
| Implement `handle_resend_request` | `session_manager.hpp` | **HIGH** |
| Gap detection logic | Same | **HIGH** |
| PossDupFlag handling | Same | MEDIUM |

**Required Changes**:
1. Inject `IMessageStore` into `SessionManager`
2. Store all outgoing messages before send
3. Retrieve and replay on `ResendRequest`
4. Set `PossDupFlag=Y` on replayed messages

#### 3.4.3 High-Performance Logging (Quill)

**Source**: Investigation report (`test/1.txt`) - "No high-performance logging subsystem"

| Task | File | Priority |
|------|------|----------|
| Integrate Quill library | `CMakeLists.txt` | **HIGH** |
| Create NexusFix logger wrapper | `include/nexusfix/util/logger.hpp` | **HIGH** |
| Replace `std::cout` debug output | Throughout codebase | MEDIUM |

**Why Quill**:

| Criteria | Quill | logdump.hpp (existing) |
|----------|-------|------------------------|
| Hot path latency | **~20 ns** | ~500+ ns |
| Lock-free | **Yes** | No (mutex) |
| C++23 support | **Yes** | Yes |
| Maintenance | **Active (2025)** | Internal |
| Log format | **Human readable** | Human readable |

**Integration**:

```cmake
# CMakeLists.txt
include(FetchContent)
FetchContent_Declare(
    quill
    GIT_REPOSITORY https://github.com/odygrd/quill.git
    GIT_TAG v4.5.0
)
FetchContent_MakeAvailable(quill)

target_link_libraries(nexusfix PRIVATE quill::quill)
```

**Wrapper Implementation**:

```cpp
// include/nexusfix/util/logger.hpp
#pragma once

#include <quill/Quill.h>
#include <quill/Logger.h>

namespace nfx::logging {

// Initialize logging (call once at startup)
inline void init(const std::string& log_dir = "logs") {
    // Configure backend thread (runs async writes)
    quill::BackendOptions backend_options;
    backend_options.thread_name = "nfx_log";
    quill::Backend::start(backend_options);

    // Create rotating file handler
    auto file_handler = quill::Frontend::create_or_get_handler<quill::RotatingFileHandler>(
        log_dir + "/nexusfix.log",
        []() {
            quill::RotatingFileHandlerConfig cfg;
            cfg.set_rotation_max_file_size(10 * 1024 * 1024);  // 10MB
            cfg.set_max_backup_files(5);
            return cfg;
        }()
    );

    // Create logger
    quill::Frontend::create_or_get_logger("nfx", std::move(file_handler));
}

// Get logger instance
inline quill::Logger* get() {
    return quill::Frontend::get_logger("nfx");
}

// Shutdown (call before exit)
inline void shutdown() {
    quill::Backend::stop();
}

} // namespace nfx::logging

// Convenience macros
#define NFX_LOG_DEBUG(fmt, ...) LOG_DEBUG(nfx::logging::get(), fmt, ##__VA_ARGS__)
#define NFX_LOG_INFO(fmt, ...)  LOG_INFO(nfx::logging::get(), fmt, ##__VA_ARGS__)
#define NFX_LOG_WARN(fmt, ...)  LOG_WARNING(nfx::logging::get(), fmt, ##__VA_ARGS__)
#define NFX_LOG_ERROR(fmt, ...) LOG_ERROR(nfx::logging::get(), fmt, ##__VA_ARGS__)
```

**Usage**:

```cpp
// Hot path - ~20ns overhead
NFX_LOG_INFO("ExecutionReport parsed: orderId={} status={}", order_id, status);

// Session events
NFX_LOG_INFO("Session {} logged on", session_id);
NFX_LOG_WARN("Sequence gap detected: expected={} received={}", expected, received);
```

**Success Criteria**:
- Log call latency: < 50 ns on hot path
- Zero heap allocation per log call
- Async disk write (no I/O blocking)

**Note**: Existing `src/utils/logdump.hpp` remains available for QuantNexus integration but is NOT used on NexusFix hot paths.

### 3.5 Benchmark Infrastructure

**Target**: Hardware counter integration for performance verification

| Task | Reference | File | Priority |
|------|-----------|------|----------|
| `perf_event_open` wrapper | TICKET_003 | `benchmark/include/perf_counters.hpp` | HIGH |
| CPU core pinning | TICKET_174 | `benchmark/include/benchmark_utils.hpp` | MEDIUM |
| IPC/Cache miss tracking | TICKET_174 | `benchmark/include/perf_counters.hpp` | HIGH |

**Implementation**:

```cpp
// benchmark/include/benchmark_utils.hpp
namespace nfx::bench {

inline bool bind_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    return pthread_setaffinity_np(pthread_self(),
                                   sizeof(cpu_set_t),
                                   &cpuset) == 0;
}

inline bool setup_benchmark_thread(int core_id, int priority = 99) {
    if (!bind_to_core(core_id)) return false;

    struct sched_param param;
    param.sched_priority = priority;
    return sched_setscheduler(0, SCHED_FIFO, &param) == 0;
}

} // namespace nfx::bench
```

**Success Criteria**:
- Benchmark repeatability: < 5% variance
- P99/P50 ratio: < 2x

## 4. Out of Scope

| Feature | Reason |
|---------|--------|
| AF_XDP | `io_uring` sufficient for Phase 1 targets |
| OpenOnload | Vendor-specific, requires special hardware |
| DPDK | Over-engineering for current use case |

These may be reconsidered for Phase 3+ if sub-microsecond latency is required.

## 5. Dependencies

```
Phase 1 (Current - COMPLETED)
    |
    +-- consteval Parser
    +-- SIMD Scanner
    +-- PMR Buffer Pool
    +-- Zero-copy
    |
Phase 2 (This Ticket)
    |
    +-- Huge Page Allocator
    +-- Benchmark Infrastructure (perf_event_open)
    +-- CPU Core Pinning
    |
Phase 5 (Future)
    |
    +-- Prefetching
    +-- Branch-free parsing
    +-- Prefetch distance tuning
```

## 6. Success Metrics

### 6.1 Hardware Optimization Metrics

| Metric | Current | Phase 2 Target | Phase 5 Target |
|--------|---------|----------------|----------------|
| ExecutionReport parse | < 200 ns | < 150 ns | < 100 ns |
| TLB miss rate | Unknown | < 0.5% | < 0.1% |
| L1 cache miss | Unknown | < 5% | < 2% |
| Hot path malloc | 0 | 0 | 0 |
| Branch miss rate | Unknown | < 1% | < 0.5% |

### 6.2 Code-Level Performance Metrics

| Metric | Current | Target |
|--------|---------|--------|
| Timestamp generation | ~500+ ns | < 50 ns |
| Timestamp hot path (ms only) | N/A | < 10 ns |
| Callback overhead | Unknown | Profile first |

### 6.3 Compliance Metrics

| Feature | Current | Target |
|---------|---------|--------|
| MessageStore | **Missing** | Implemented |
| ResendRequest handling | Gap fill only | Full replay |
| PossDupFlag support | **Missing** | Implemented |
| FIX 4.4 Compliance | Partial | Full |

### 6.4 Logging Metrics

| Metric | Current (logdump.hpp) | Target (Quill) |
|--------|----------------------|----------------|
| Hot path log latency | ~500+ ns | **< 50 ns** |
| Lock-free | No | **Yes** |
| Heap allocation per log | Possible | **0** |
| Async disk write | Yes | **Yes** |

## 7. References

- [CLAUDE.md](/data/ws/NexusFix/CLAUDE.md) - Project guidelines and C++ standards
- [TICKET_003](TICKET_003_BENCHMARK_FRAMEWORK.md) - Benchmark framework
- [TICKET_174](/data/ws/NexusFix/docs/TICKET_174_CPP_BENCHMARK_FRAMEWORK.md) - Detailed benchmark specifications
- [TICKET_175](/data/ws/NexusFix/docs/TICKET_175_CPP_OPTIMIZATION_ROADMAP.md) - Optimization roadmap

## 8. Cleanup Actions

The following files are redundant and should be removed:

| File | Reason |
|------|--------|
| `GEMINI.md` | Exact duplicate of `CLAUDE.md` |
| `docs/compare/NEXUSFIX_INVESTIGATION_REPORT.md` | Findings captured in this ticket (Section 2.2) |
| `test/1.txt` | Findings captured in this ticket (Section 2.3) |
