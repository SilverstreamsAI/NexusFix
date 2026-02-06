# TICKET_211: C++20 Coroutine Patterns - Before/After Benchmark

**Date:** 2026-02-06
**Iterations:** 100,000 (warmup: 10,000)
**CPU:** 3.418 GHz (busy-wait calibrated, pinned to core 0)
**Compiler:** GCC, `-O3 -march=native -flto=auto`
**Optimization Applied:** C++20 coroutine primitives (modernc_quant.md #27)

---

## Executive Summary

TICKET_211 introduces coroutine-based session management (`CoroutineSession`) as an alternative to the existing callback-based `SessionManager`. **This is not a latency optimization** - every coroutine primitive is slower than its traditional equivalent. The value is architectural: non-blocking semantics, structured concurrency, and maintainable session lifecycle code.

| Primitive | Baseline (ns) | Coroutine (ns) | Overhead | Throughput |
|-----------|--------------|----------------|----------|------------|
| Task\<int\> create + get | 9.4 (raw call) | 20.8 | **+11.4 ns (2.2x)** | 48.0 M ops/sec |
| AsyncMutex lock/unlock | 13.6 (std::mutex) | 29.6 | **+16.0 ns (2.2x)** | 33.8 M ops/sec |
| Event wait (fast path) | -- | 26.0 | -- | 38.5 M ops/sec |
| co_await Yield | -- | 25.6 | -- | 39.1 M ops/sec |

**Verdict:** All coroutine primitives are **~2.2x slower** than raw alternatives in isolation. However, they operate in the **20-30 ns range** with throughput exceeding **33 M ops/sec**. For session-level operations (logon, heartbeat, reconnect) that run on millisecond/second timescales, this overhead is negligible (<0.003% of a 1ms heartbeat).

---

## Task Creation & Execution

Measures the full lifecycle: coroutine frame allocation, initial_suspend, resume, co_return, final_suspend with symmetric transfer, frame destruction.

| Operation | Min | Mean | P50 | P90 | P99 |
|-----------|-----|------|-----|-----|-----|
| Raw function call (int) | 8.4 ns | 9.4 ns | 9.4 ns | 9.8 ns | 10.2 ns |
| std::function call (int) | 7.9 ns | 9.1 ns | 9.0 ns | 9.4 ns | 10.0 ns |
| **Task\<int\> create + get** | **19.0 ns** | **20.8 ns** | **20.5 ns** | **21.4 ns** | **32.2 ns** |
| **Task\<void\> create + get** | **19.0 ns** | **20.8 ns** | **20.4 ns** | **21.1 ns** | **32.8 ns** |
| **co_await Yield (suspend/resume)** | **20.2 ns** | **25.6 ns** | **25.5 ns** | **26.0 ns** | **34.0 ns** |

### Analysis

- **Task\<T\> overhead vs raw call: +11.4 ns**. This covers coroutine frame allocation (compiler-optimized, often on stack via HALO), two suspend points (initial + final), and frame destruction. At 20.8 ns mean, this is **~7 CPU cycles per suspend/resume transition** at 3.4 GHz.
- **Task\<int\> vs Task\<void\>**: Identical performance. The `std::optional<T>` storage adds no measurable overhead.
- **Yield adds +4.8 ns** over basic Task. This measures one additional suspend/resume round-trip beyond the initial suspend.
- **P99 tail (~32 ns)**: Occasional spikes from cache misses on coroutine frame allocation. Still under 35 ns.

---

## Synchronization Primitives

Measures lock acquisition and release on the uncontended fast path (CAS succeeds immediately).

| Operation | Min | Mean | P50 | P90 | P99 |
|-----------|-----|------|-----|-----|-----|
| std::mutex lock/unlock | 12.0 ns | 13.6 ns | 13.6 ns | 14.2 ns | 15.0 ns |
| **AsyncMutex lock/unlock (new)** | **27.8 ns** | **29.6 ns** | **29.2 ns** | **29.8 ns** | **40.6 ns** |
| **AsyncMutex lock/unlock (reused)** | **27.8 ns** | **29.6 ns** | **29.2 ns** | **29.8 ns** | **40.7 ns** |
| **Event wait (already set)** | **21.9 ns** | **26.0 ns** | **26.0 ns** | **27.2 ns** | **38.4 ns** |
| **Event set + resume (suspend path)** | **29.3 ns** | **31.4 ns** | **30.8 ns** | **31.6 ns** | **47.7 ns** |

### Analysis

- **AsyncMutex overhead vs std::mutex: +16.0 ns**. The extra cost comes from: coroutine frame allocation for the lock awaiter, CAS on `std::atomic<void*>` state, ScopedLock RAII construction, and symmetric transfer on final_suspend. This is acceptable because AsyncMutex enables **non-blocking suspension** - a blocked coroutine does not consume a thread, unlike std::mutex.
- **AsyncMutex new vs reused: identical**. No warm-up benefit; each lock creates a fresh LockAwaiter in the coroutine frame.
- **Event fast path (26.0 ns)**: `await_ready()` returns true immediately via atomic load. Cost includes coroutine frame + Event construction.
- **Event suspend path (31.4 ns)**: Full suspend/resume cycle - coroutine suspends at `co_await`, `set()` atomically swaps state and calls `handle.resume()`. Only +5.4 ns more than the fast path.

---

## Combinators

Measures higher-level coroutine composition patterns.

| Operation | Min | Mean | P50 | P90 | P99 |
|-----------|-----|------|-----|-----|-----|
| **WhenAll (3 immediate tasks)** | **76.1 ns** | **88.6 ns** | **85.4 ns** | **101.8 ns** | **126.6 ns** |
| **WhenAny (1 immediate + 1 yield)** | **59.1 ns** | **69.1 ns** | **63.2 ns** | **84.0 ns** | **103.0 ns** |
| **with_timeout (completes immediately)** | **112.8 ns** | **126.3 ns** | **120.0 ns** | **140.9 ns** | **180.7 ns** |

### Analysis

- **WhenAll(3) at 88.6 ns**: Creates 3 driver coroutines + 3 task coroutines + WhenAllAwaiter. Approximately 29.5 ns per task, consistent with individual Task overhead. The atomic `fetch_sub` counter adds minimal cost.
- **WhenAny(2) at 69.1 ns**: Creates 2 driver coroutines + WhenAnyAwaiter. The `compare_exchange_strong` for first-completer-wins adds negligible overhead.
- **with_timeout at 126.3 ns**: Composes `when_any` with an operation wrapper and a timeout coroutine. This is the most complex combinator but still completes in **~43 CPU cycles** - well under the microsecond level needed for session timeouts.

---

## Before/After Comparison

| Operation | Before (Baseline) | After (Coroutine) | Mean Delta | P99 Delta |
|-----------|-------------------|-------------------|------------|-----------|
| Function dispatch | 9.4 ns (raw call) | 20.8 ns (Task\<int\>) | +11.4 ns (+121%) | +22.0 ns (+214%) |
| Type-erased dispatch | 9.1 ns (std::function) | 20.8 ns (Task\<int\>) | +11.7 ns (+129%) | +22.2 ns (+224%) |
| Mutex acquire/release | 13.6 ns (std::mutex) | 29.6 ns (AsyncMutex) | +16.0 ns (+117%) | +25.6 ns (+170%) |

### Overhead Budget

| Component | Overhead | Context |
|-----------|----------|---------|
| Task\<int\> vs raw call | +11.4 ns | Coroutine frame alloc + 2 suspend points |
| AsyncMutex vs std::mutex | +16.0 ns | CAS + coroutine frame + ScopedLock RAII |
| Yield (suspend/resume) | +4.8 ns | Single additional suspend/resume cycle |
| Event fast path (total) | 26.0 ns | Atomic load + coroutine frame |
| Event suspend path (total) | 31.4 ns | Atomic CAS + handle.resume() |
| WhenAll per-task | ~29.5 ns | Driver coroutine + atomic counter |
| WhenAny per-task | ~34.6 ns | Driver coroutine + atomic flag |
| with_timeout (total) | 126.3 ns | WhenAny + 2 wrapper coroutines |

---

## Throughput

| Primitive | Ops/sec | Context |
|-----------|---------|---------|
| Task\<int\> create + get | **48.0 M/sec** | Single coroutine lifecycle |
| AsyncMutex lock + unlock | **33.8 M/sec** | Uncontended CAS path |
| Event wait (fast path) | **38.5 M/sec** | Already-set check |
| WhenAll(3) | **11.3 M/sec** | 3 concurrent tasks |
| WhenAny(2) | **14.5 M/sec** | First-completer-wins |
| with_timeout | **7.9 M/sec** | Full timeout wrapper |

---

## Techniques Applied

| Technique | Reference | Application |
|-----------|-----------|-------------|
| Coroutine state machine | modernc_quant.md #27 | Task\<T\>, Task\<void\> with lazy evaluation |
| Atomic memory ordering | modernc_quant.md #19 | AsyncMutex CAS (acquire/release, not seq_cst) |
| Lock-free data structure | modernc_quant.md #23 | Intrusive waiter linked list in AsyncMutex/Event |
| `noexcept` guarantee | modernc_quant.md #21 | All awaiter methods marked noexcept |
| `[[nodiscard]]` enforcement | modernc_quant.md #31 | All factory functions |
| Non-copyable RAII | modernc_quant.md #36 | ScopedLock, Task move-only semantics |
| Zero-copy views | modernc_quant.md #40 | std::span\<const char\> for message sends |

---

## Value Assessment

### What This Is NOT

This is **not a performance optimization**. Every coroutine primitive measured here is slower than its traditional equivalent:

| Comparison | Factor |
|------------|--------|
| Task\<int\> vs raw function call | **2.2x slower** |
| Task\<int\> vs std::function | **2.3x slower** |
| AsyncMutex vs std::mutex | **2.2x slower** |

If the goal were purely "make things faster", TICKET_211 would not achieve that.

### What This IS

An **architectural capability** that trades 11-16 ns of overhead for:

1. **Non-blocking session management**: `std::mutex` blocks the entire thread when contended. `AsyncMutex` suspends only the coroutine - the thread is free to do other work. A suspended coroutine costs zero CPU cycles.

2. **Structured concurrency**: `CoroutineSession::active_phase()` runs heartbeat monitoring, message receiving, and shutdown detection concurrently with a single `when_any` call. The equivalent callback-based code in `SessionManager` requires manually managing three independent lifecycles with shared mutable state.

3. **Linear control flow**: The full session lifecycle (connect -> logon -> active -> logout) reads as sequential code with `co_await`, rather than a state machine with scattered callbacks. This reduces the surface area for bugs in reconnection and error handling logic.

4. **Hot path unaffected**: Message parsing (~200 ns) and field access (~10 ns) remain on the zero-allocation, non-coroutine fast path. Coroutines are used exclusively for session lifecycle management where operations run on millisecond/second timescales.

### Overhead in Context

| Session Operation | Typical Duration | Coroutine Overhead | Overhead Ratio |
|-------------------|------------------|--------------------|----------------|
| Heartbeat interval | 30,000,000,000 ns (30s) | 20.8 ns | 0.00000007% |
| Logon timeout | 5,000,000,000 ns (5s) | 126.3 ns (with_timeout) | 0.000003% |
| TCP round-trip | ~100,000 ns (100us) | 29.6 ns (AsyncMutex) | 0.03% |
| Message parse | ~200 ns | 0 ns (not on this path) | 0% |

## Conclusion

TICKET_211 adds **~2.2x latency overhead** per coroutine operation vs raw alternatives. This overhead is irrelevant for session-level operations but would be unacceptable on the message parsing hot path (which remains untouched). The value is in code maintainability and non-blocking concurrency, not in raw speed. All primitives exceed **33 M ops/sec**, confirming sufficient throughput for production session management.
