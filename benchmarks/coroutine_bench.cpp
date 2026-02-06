/*
    TICKET_211: C++20 Coroutine Patterns Benchmark

    Measures overhead of coroutine primitives vs traditional alternatives.
    Before = raw function call / std::mutex / manual state machine
    After  = Task<T> / AsyncMutex / Event / WhenAll / WhenAny
*/

#include <cstdio>
#include <cstdint>
#include <mutex>
#include <vector>
#include <functional>
#include <algorithm>

#include "benchmark_utils.hpp"
#include "nexusfix/session/coroutine.hpp"
#include "nexusfix/session/async_primitives.hpp"

using namespace nfx;
using namespace nfx::bench;

// ============================================================================
// Configuration
// ============================================================================

static constexpr int WARMUP_ITERATIONS = 10000;
static constexpr int BENCHMARK_ITERATIONS = 100000;

// ============================================================================
// Coroutine helpers (free functions, not lambdas)
// ============================================================================

static Task<int> coro_return_int(int v) {
    co_return v;
}

static Task<void> coro_return_void() {
    co_return;
}

static Task<int> coro_mutex_lock_return(AsyncMutex& m, int v) {
    auto lock = co_await m.scoped_lock();
    co_return v;
}

static Task<int> coro_event_wait_return(Event& e, int v) {
    co_await e;
    co_return v;
}

static Task<void> coro_yield_once() {
    co_await Yield{};
}

static Task<void> coro_immediate() {
    co_return;
}

static Task<void> coro_yield_forever() {
    while (true) {
        co_await Yield{};
    }
}

// ============================================================================
// Baseline: raw function equivalents
// ============================================================================

static int raw_return_int(int v) {
    return v;
}

[[maybe_unused]]
static void raw_return_void() {
}

// ============================================================================
// Benchmark runner
// ============================================================================

struct BenchResult {
    LatencyStats stats;
    const char* label;
};

template <typename Func>
BenchResult run_bench(const char* label, Func&& func,
                      double freq_ghz, int iterations = BENCHMARK_ITERATIONS) {
    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
        compiler_barrier();
        func();
        compiler_barrier();
    }

    std::vector<uint64_t> cycles(static_cast<size_t>(iterations));

    for (int i = 0; i < iterations; ++i) {
        uint64_t start = rdtsc_vm_safe();
        compiler_barrier();
        func();
        compiler_barrier();
        uint64_t end = rdtsc_vm_safe();
        cycles[static_cast<size_t>(i)] = end - start;
    }

    BenchResult result;
    result.label = label;
    result.stats.compute(cycles, freq_ghz);
    return result;
}

static void print_stats_row(const char* label, const LatencyStats& s) {
    std::printf("  %-40s %8.1f  %8.1f  %8.1f  %8.1f  %8.1f\n",
        label, s.min_ns, s.mean_ns, s.p50_ns, s.p90_ns, s.p99_ns);
}

static void print_comparison_row(const char* label,
                                 const LatencyStats& before,
                                 const LatencyStats& after) {
    double delta_mean = before.mean_ns - after.mean_ns;
    double delta_pct = (delta_mean / before.mean_ns) * 100.0;
    double delta_p99 = (before.p99_ns - after.p99_ns) / before.p99_ns * 100.0;

    std::printf("  %-30s %8.1f  %8.1f  %+7.1f%%  %8.1f  %8.1f  %+7.1f%%\n",
        label,
        before.mean_ns, after.mean_ns, -delta_pct,
        before.p99_ns, after.p99_ns, -delta_p99);
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::printf("=============================================================\n");
    std::printf("  TICKET_211: Coroutine Patterns Benchmark\n");
    std::printf("  NexusFIX Before/After Comparison\n");
    std::printf("=============================================================\n\n");

    // CPU frequency calibration
    std::printf("Calibrating CPU frequency (busy-wait)...\n");
    double freq_ghz = estimate_cpu_freq_ghz_busy();
    std::printf("CPU frequency: %.3f GHz\n", freq_ghz);
    std::printf("Iterations: %d (warmup: %d)\n\n", BENCHMARK_ITERATIONS, WARMUP_ITERATIONS);

    // Try to pin to core 0
    if (bind_to_core(0)) {
        std::printf("Pinned to core 0\n\n");
    } else {
        std::printf("Warning: Could not pin to core 0\n\n");
    }

    // ========================================================================
    // Section 1: Task Creation & Execution Overhead
    // ========================================================================

    std::printf("--- Section 1: Task Creation & Execution ---\n");
    std::printf("  %-40s %8s  %8s  %8s  %8s  %8s\n",
        "Operation", "Min", "Mean", "P50", "P90", "P99");
    std::printf("  %s\n", std::string(88, '-').c_str());

    volatile int sink = 0;

    auto b_raw_call = run_bench("Raw function call (int)", [&]() {
        sink = raw_return_int(42);
    }, freq_ghz);
    print_stats_row(b_raw_call.label, b_raw_call.stats);

    auto b_std_function = run_bench("std::function call (int)", [&]() {
        std::function<int(int)> fn = raw_return_int;
        sink = fn(42);
    }, freq_ghz);
    print_stats_row(b_std_function.label, b_std_function.stats);

    auto b_task_int = run_bench("Task<int> create + get", [&]() {
        auto task = coro_return_int(42);
        sink = task.get();
    }, freq_ghz);
    print_stats_row(b_task_int.label, b_task_int.stats);

    auto b_task_void = run_bench("Task<void> create + get", [&]() {
        auto task = coro_return_void();
        task.get();
    }, freq_ghz);
    print_stats_row(b_task_void.label, b_task_void.stats);

    auto b_yield = run_bench("co_await Yield (suspend/resume)", [&]() {
        auto task = coro_yield_once();
        task.get();
    }, freq_ghz);
    print_stats_row(b_yield.label, b_yield.stats);

    // ========================================================================
    // Section 2: Synchronization Primitives
    // ========================================================================

    std::printf("\n--- Section 2: Synchronization Primitives ---\n");
    std::printf("  %-40s %8s  %8s  %8s  %8s  %8s\n",
        "Operation", "Min", "Mean", "P50", "P90", "P99");
    std::printf("  %s\n", std::string(88, '-').c_str());

    auto b_std_mutex = run_bench("std::mutex lock/unlock", [&]() {
        std::mutex mtx;
        std::lock_guard<std::mutex> lock(mtx);
        sink = 42;
    }, freq_ghz);
    print_stats_row(b_std_mutex.label, b_std_mutex.stats);

    auto b_async_mutex = run_bench("AsyncMutex lock/unlock (uncontended)", [&]() {
        AsyncMutex mtx;
        auto task = coro_mutex_lock_return(mtx, 42);
        sink = task.get();
    }, freq_ghz);
    print_stats_row(b_async_mutex.label, b_async_mutex.stats);

    // Reusable AsyncMutex (measure without construction overhead)
    AsyncMutex reusable_mutex;
    auto b_async_mutex_reuse = run_bench("AsyncMutex lock/unlock (reused)", [&]() {
        auto task = coro_mutex_lock_return(reusable_mutex, 42);
        sink = task.get();
    }, freq_ghz);
    print_stats_row(b_async_mutex_reuse.label, b_async_mutex_reuse.stats);

    auto b_event_already_set = run_bench("Event wait (already set)", [&]() {
        Event evt;
        evt.set();
        auto task = coro_event_wait_return(evt, 42);
        sink = task.get();
    }, freq_ghz);
    print_stats_row(b_event_already_set.label, b_event_already_set.stats);

    // Event with suspend/resume path
    auto b_event_suspend = run_bench("Event set + resume (suspend path)", [&]() {
        Event evt;
        auto task = coro_event_wait_return(evt, 42);
        // Start the coroutine - it will suspend at co_await evt
        task.resume();
        // Now set the event to resume it
        evt.set();
        sink = task.get();
    }, freq_ghz);
    print_stats_row(b_event_suspend.label, b_event_suspend.stats);

    // ========================================================================
    // Section 3: Combinators
    // ========================================================================

    std::printf("\n--- Section 3: Combinators ---\n");
    std::printf("  %-40s %8s  %8s  %8s  %8s  %8s\n",
        "Operation", "Min", "Mean", "P50", "P90", "P99");
    std::printf("  %s\n", std::string(88, '-').c_str());

    auto b_when_all_3 = run_bench("WhenAll (3 immediate tasks)", [&]() {
        std::vector<Task<void>> tasks;
        tasks.reserve(3);
        tasks.push_back(coro_immediate());
        tasks.push_back(coro_immediate());
        tasks.push_back(coro_immediate());
        auto task = when_all(std::move(tasks));
        task.get();
    }, freq_ghz);
    print_stats_row(b_when_all_3.label, b_when_all_3.stats);

    auto b_when_any_2 = run_bench("WhenAny (1 immediate + 1 yield)", [&]() {
        std::vector<Task<void>> tasks;
        tasks.reserve(2);
        tasks.push_back(coro_immediate());
        tasks.push_back(coro_yield_forever());
        auto task = when_any(std::move(tasks));
        static_cast<void>(task.get());
    }, freq_ghz);
    print_stats_row(b_when_any_2.label, b_when_any_2.stats);

    auto b_timeout = run_bench("with_timeout (completes immediately)", [&]() {
        auto task = with_timeout(coro_return_int(42), std::chrono::milliseconds{1000});
        auto result = task.get();
        if (result.has_value()) sink = *result;
    }, freq_ghz);
    print_stats_row(b_timeout.label, b_timeout.stats);

    // ========================================================================
    // Section 4: Before/After Comparison Summary
    // ========================================================================

    std::printf("\n=============================================================\n");
    std::printf("  Before/After Comparison (Baseline vs Coroutine)\n");
    std::printf("=============================================================\n");
    std::printf("  %-30s %8s  %8s  %8s  %8s  %8s  %8s\n",
        "Operation", "Base", "Coro", "Mean%", "BaseP99", "CoroP99", "P99%");
    std::printf("  %s\n", std::string(98, '-').c_str());

    print_comparison_row("Function call -> Task<int>",
        b_raw_call.stats, b_task_int.stats);

    print_comparison_row("std::function -> Task<int>",
        b_std_function.stats, b_task_int.stats);

    print_comparison_row("std::mutex -> AsyncMutex",
        b_std_mutex.stats, b_async_mutex_reuse.stats);

    // ========================================================================
    // Section 5: Overhead Budget
    // ========================================================================

    std::printf("\n--- Overhead Budget ---\n");
    double task_overhead = b_task_int.stats.mean_ns - b_raw_call.stats.mean_ns;
    double mutex_overhead = b_async_mutex_reuse.stats.mean_ns - b_std_mutex.stats.mean_ns;
    double yield_overhead = b_yield.stats.mean_ns - b_task_void.stats.mean_ns;

    std::printf("  Task<int> overhead vs raw call:      %+.1f ns\n", task_overhead);
    std::printf("  AsyncMutex overhead vs std::mutex:   %+.1f ns\n", mutex_overhead);
    std::printf("  Yield overhead (suspend+resume):     %+.1f ns\n", yield_overhead);
    std::printf("  Event already-set fast path:          %.1f ns\n", b_event_already_set.stats.mean_ns);
    std::printf("  Event suspend+resume path:            %.1f ns\n", b_event_suspend.stats.mean_ns);
    std::printf("  WhenAll(3) total:                     %.1f ns\n", b_when_all_3.stats.mean_ns);
    std::printf("  WhenAny(2) total:                     %.1f ns\n", b_when_any_2.stats.mean_ns);
    std::printf("  with_timeout total:                   %.1f ns\n", b_timeout.stats.mean_ns);

    std::printf("\n--- Per-operation throughput ---\n");
    double task_ops = 1e9 / b_task_int.stats.mean_ns;
    double mutex_ops = 1e9 / b_async_mutex_reuse.stats.mean_ns;
    double event_ops = 1e9 / b_event_already_set.stats.mean_ns;
    std::printf("  Task<int> create+get:   %.2f M ops/sec\n", task_ops / 1e6);
    std::printf("  AsyncMutex lock+unlock: %.2f M ops/sec\n", mutex_ops / 1e6);
    std::printf("  Event wait (fast path): %.2f M ops/sec\n", event_ops / 1e6);

    std::printf("\n=============================================================\n");
    std::printf("  Benchmark complete.\n");
    std::printf("=============================================================\n");

    return 0;
}
