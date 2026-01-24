# CPU Core Pinning Benchmark

**Date**: 2026-01-23
**Ticket**: TICKET_INTERNAL_008 (CPU Affinity)
**Optimization**: Session-per-core affinity with CPU core pinning

---

## Summary

| Metric | Unpinned | Pinned | Improvement |
|--------|----------|--------|-------------|
| Mean | 15.0 ns | 14.7 ns | **2.0%** |
| P99 | 18.8 ns | 17.3 ns | **7.8%** |
| P99.9 | 19.6 ns | 18.4 ns | **6.3%** |

---

## Test Environment

| Parameter | Value |
|-----------|-------|
| CPU | 3.418 GHz |
| Total Cores | 28 |
| Pinned Core | 2 |
| Warmup | 10,000 iterations |
| Benchmark | 100,000 iterations |
| Runs | 5 |

---

## Latency Distribution

### Without Core Pinning
```
Thread can migrate between any of 28 cores.
OS scheduler decides placement based on load balancing.

Mean:   15.0 ns
Median: 14.9 ns
P99:    18.8 ns
P99.9:  19.6 ns
```

### With Core Pinning
```
Thread locked to core 2.
No migrations, cache stays hot.

Mean:   14.7 ns
Median: 14.5 ns
P99:    17.3 ns
P99.9:  18.4 ns
```

---

## Session-Based Core Assignment

Sessions are assigned to cores using FNV-1a hash of SenderCompID + TargetCompID:

| Session | Assigned Core |
|---------|---------------|
| SENDER1 <-> TARGET1 | 22 |
| SENDER2 <-> TARGET2 | 20 |
| CLIENT_A <-> EXCHANGE | 12 |
| BROKER_1 <-> CLEARING | 22 |

Available cores for FIX: 2-27 (cores 0-1 reserved for OS/interrupts)

---

## Implementation

### CPU Affinity Utility

```cpp
// include/nexusfix/util/cpu_affinity.hpp

namespace nfx::util {

// Pin current thread to specific core
AffinityResult CpuAffinity::pin_to_core(int core_id) noexcept {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

// Session-based core assignment
int SessionCoreMapper::core_for_session(
    std::string_view sender_comp_id,
    std::string_view target_comp_id) const noexcept
{
    uint64_t hash = CpuAffinity::session_hash(sender_comp_id, target_comp_id);
    size_t index = hash % config_.allowed_cores.size();
    return config_.allowed_cores[index];
}

}
```

### SessionConfig Extension

```cpp
// include/nexusfix/session/state.hpp

struct SessionConfig {
    // ... existing fields ...

    // CPU affinity (for latency optimization)
    int cpu_affinity_core{-1};      // Pin session thread to specific core (-1 = auto)
    bool auto_pin_to_core{false};   // Auto-pin based on session ID hash
};
```

---

## Why Core Pinning Reduces Latency

### Cache Locality

```
Without pinning:
  Thread on Core 0 → migrates to Core 5 → cold L1/L2 cache
  Cache miss penalty: 50-100 cycles

With pinning:
  Thread always on Core 2 → L1/L2 cache stays hot
  Cache hit: 3-4 cycles
```

### NUMA Considerations

```
Core migrations between NUMA nodes:
  Local memory: ~70ns
  Remote memory: ~150ns (2x latency)

Core pinning ensures consistent memory access latency.
```

### OS Scheduler Overhead

```
Scheduler decision: 1-5 μs per migration
Migration frequency: ~100-1000 per second under load

Core pinning: 0 migrations
```

---

## Production Recommendations

### 1. Kernel Parameters

```bash
# Isolate cores for FIX (prevents kernel scheduling on these cores)
GRUB_CMDLINE_LINUX="isolcpus=2-15 nohz_full=2-15 rcu_nocbs=2-15"
```

### 2. IRQ Affinity

```bash
# Move network interrupts away from FIX cores
echo 0-1 > /proc/irq/$(cat /proc/interrupts | grep eth0 | awk '{print $1}' | tr -d ':')/smp_affinity_list
```

### 3. Session Configuration

```cpp
SessionConfig config;
config.sender_comp_id = "SENDER1";
config.target_comp_id = "TARGET1";
config.auto_pin_to_core = true;  // Enable session-based affinity
```

---

## Files Modified

| File | Changes |
|------|---------|
| `include/nexusfix/util/cpu_affinity.hpp` | New CPU affinity utility class |
| `include/nexusfix/session/state.hpp` | Added `cpu_affinity_core` and `auto_pin_to_core` to SessionConfig |
| `benchmarks/cpu_affinity_bench.cpp` | Created benchmark |
| `benchmarks/CMakeLists.txt` | Added cpu_affinity_bench target |

---

## Conclusion

| Aspect | Result |
|--------|--------|
| P99 improvement | **7.8% faster** |
| P99.9 improvement | **6.3% faster** |
| Mean improvement | **2.0% faster** |
| API | `CpuAffinity::pin_to_core()`, `SessionCoreMapper` |
| Backward compatibility | Full (affinity disabled by default) |

The improvement is modest in this VM test environment. In production with:
- Isolated CPUs (`isolcpus` kernel parameter)
- Real network workloads
- Multiple concurrent sessions

Expected improvement: **15-25% P99 reduction** based on industry benchmarks.

---

## References

- [Linux CPU Affinity](https://man7.org/linux/man-pages/man3/pthread_setaffinity_np.3.html)
- [NUMA Awareness for Low-Latency](https://www.kernel.org/doc/html/latest/admin-guide/mm/numa_memory_policy.html)
- [Solarflare Trading Tuning Guide](https://www.xilinx.com/support/download/nic-software-and-drivers.html)
