/*
    NexusFIX Hardware Performance Counters

    Linux perf_event_open wrapper for hardware counter access:
    - CPU cycles
    - Instructions
    - Cache misses (L1, LLC)
    - Branch misses
    - TLB misses
*/

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <optional>
#include <array>

#ifdef __linux__
#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstring>
#endif

namespace nfx::bench {

// ============================================================================
// Performance Event Types
// ============================================================================

enum class PerfEvent : uint32_t {
    // Hardware events
    CpuCycles,
    Instructions,
    CacheReferences,
    CacheMisses,
    BranchInstructions,
    BranchMisses,

    // Cache events
    L1dReadMiss,
    L1dWriteMiss,
    L1iReadMiss,
    LLCReadMiss,
    LLCWriteMiss,

    // TLB events
    DTlbReadMiss,
    DTlbWriteMiss,
    ITlbReadMiss
};

/// Get human-readable name for event
[[nodiscard]] inline const char* event_name(PerfEvent event) noexcept {
    switch (event) {
        case PerfEvent::CpuCycles:          return "cpu-cycles";
        case PerfEvent::Instructions:       return "instructions";
        case PerfEvent::CacheReferences:    return "cache-references";
        case PerfEvent::CacheMisses:        return "cache-misses";
        case PerfEvent::BranchInstructions: return "branch-instructions";
        case PerfEvent::BranchMisses:       return "branch-misses";
        case PerfEvent::L1dReadMiss:        return "L1-dcache-read-misses";
        case PerfEvent::L1dWriteMiss:       return "L1-dcache-write-misses";
        case PerfEvent::L1iReadMiss:        return "L1-icache-read-misses";
        case PerfEvent::LLCReadMiss:        return "LLC-read-misses";
        case PerfEvent::LLCWriteMiss:       return "LLC-write-misses";
        case PerfEvent::DTlbReadMiss:       return "dTLB-read-misses";
        case PerfEvent::DTlbWriteMiss:      return "dTLB-write-misses";
        case PerfEvent::ITlbReadMiss:       return "iTLB-read-misses";
    }
    return "unknown";
}

// ============================================================================
// Performance Counter Results
// ============================================================================

struct PerfResult {
    PerfEvent event;
    uint64_t value{0};
    bool valid{false};
};

struct DerivedMetrics {
    double ipc{0};              // Instructions per cycle
    double branch_miss_rate{0}; // Branch misses / branch instructions
    double cache_miss_rate{0};  // Cache misses / cache references
    double l1d_miss_rate{0};    // L1 data cache miss rate
    double tlb_miss_rate{0};    // TLB miss rate (approximate)

    static DerivedMetrics compute(const std::vector<PerfResult>& results) {
        DerivedMetrics m;

        uint64_t cycles = 0, instructions = 0;
        uint64_t cache_refs = 0, cache_misses = 0;
        uint64_t branch_insns = 0, branch_misses = 0;

        for (const auto& r : results) {
            if (!r.valid) continue;

            switch (r.event) {
                case PerfEvent::CpuCycles:          cycles = r.value; break;
                case PerfEvent::Instructions:       instructions = r.value; break;
                case PerfEvent::CacheReferences:    cache_refs = r.value; break;
                case PerfEvent::CacheMisses:        cache_misses = r.value; break;
                case PerfEvent::BranchInstructions: branch_insns = r.value; break;
                case PerfEvent::BranchMisses:       branch_misses = r.value; break;
                default: break;
            }
        }

        if (cycles > 0) {
            m.ipc = static_cast<double>(instructions) / static_cast<double>(cycles);
        }
        if (branch_insns > 0) {
            m.branch_miss_rate = static_cast<double>(branch_misses) /
                                 static_cast<double>(branch_insns) * 100.0;
        }
        if (cache_refs > 0) {
            m.cache_miss_rate = static_cast<double>(cache_misses) /
                                static_cast<double>(cache_refs) * 100.0;
        }

        return m;
    }
};

// ============================================================================
// Performance Counter Group
// ============================================================================

#ifdef __linux__

class PerfCounterGroup {
public:
    PerfCounterGroup() = default;

    ~PerfCounterGroup() {
        for (int fd : fds_) {
            if (fd >= 0) close(fd);
        }
    }

    // Non-copyable
    PerfCounterGroup(const PerfCounterGroup&) = delete;
    PerfCounterGroup& operator=(const PerfCounterGroup&) = delete;

    /// Add an event to monitor
    bool add(PerfEvent event) {
        perf_event_attr attr{};
        attr.size = sizeof(attr);
        attr.disabled = 1;
        attr.exclude_kernel = 1;
        attr.exclude_hv = 1;

        if (!configure_event(event, attr)) {
            return false;
        }

        int group_fd = fds_.empty() ? -1 : fds_[0];
        int fd = static_cast<int>(syscall(__NR_perf_event_open, &attr, 0, -1, group_fd, 0));

        if (fd < 0) {
            return false;
        }

        fds_.push_back(fd);
        events_.push_back(event);
        return true;
    }

    /// Start counting
    void start() {
        if (!fds_.empty()) {
            ioctl(fds_[0], PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
            ioctl(fds_[0], PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
        }
    }

    /// Stop counting
    void stop() {
        if (!fds_.empty()) {
            ioctl(fds_[0], PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
        }
    }

    /// Read counter values
    [[nodiscard]] std::vector<PerfResult> read() const {
        std::vector<PerfResult> results;
        results.reserve(events_.size());

        for (size_t i = 0; i < fds_.size(); ++i) {
            PerfResult r;
            r.event = events_[i];

            if (::read(fds_[i], &r.value, sizeof(r.value)) == sizeof(r.value)) {
                r.valid = true;
            }

            results.push_back(r);
        }

        return results;
    }

    /// Get number of configured events
    [[nodiscard]] size_t size() const noexcept { return events_.size(); }

    /// Check if any events are configured
    [[nodiscard]] bool empty() const noexcept { return events_.empty(); }

private:
    bool configure_event(PerfEvent event, perf_event_attr& attr) {
        switch (event) {
            case PerfEvent::CpuCycles:
                attr.type = PERF_TYPE_HARDWARE;
                attr.config = PERF_COUNT_HW_CPU_CYCLES;
                break;
            case PerfEvent::Instructions:
                attr.type = PERF_TYPE_HARDWARE;
                attr.config = PERF_COUNT_HW_INSTRUCTIONS;
                break;
            case PerfEvent::CacheReferences:
                attr.type = PERF_TYPE_HARDWARE;
                attr.config = PERF_COUNT_HW_CACHE_REFERENCES;
                break;
            case PerfEvent::CacheMisses:
                attr.type = PERF_TYPE_HARDWARE;
                attr.config = PERF_COUNT_HW_CACHE_MISSES;
                break;
            case PerfEvent::BranchInstructions:
                attr.type = PERF_TYPE_HARDWARE;
                attr.config = PERF_COUNT_HW_BRANCH_INSTRUCTIONS;
                break;
            case PerfEvent::BranchMisses:
                attr.type = PERF_TYPE_HARDWARE;
                attr.config = PERF_COUNT_HW_BRANCH_MISSES;
                break;
            case PerfEvent::L1dReadMiss:
                attr.type = PERF_TYPE_HW_CACHE;
                attr.config = PERF_COUNT_HW_CACHE_L1D |
                             (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                             (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);
                break;
            case PerfEvent::L1dWriteMiss:
                attr.type = PERF_TYPE_HW_CACHE;
                attr.config = PERF_COUNT_HW_CACHE_L1D |
                             (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
                             (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);
                break;
            case PerfEvent::L1iReadMiss:
                attr.type = PERF_TYPE_HW_CACHE;
                attr.config = PERF_COUNT_HW_CACHE_L1I |
                             (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                             (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);
                break;
            case PerfEvent::LLCReadMiss:
                attr.type = PERF_TYPE_HW_CACHE;
                attr.config = PERF_COUNT_HW_CACHE_LL |
                             (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                             (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);
                break;
            case PerfEvent::LLCWriteMiss:
                attr.type = PERF_TYPE_HW_CACHE;
                attr.config = PERF_COUNT_HW_CACHE_LL |
                             (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
                             (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);
                break;
            case PerfEvent::DTlbReadMiss:
                attr.type = PERF_TYPE_HW_CACHE;
                attr.config = PERF_COUNT_HW_CACHE_DTLB |
                             (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                             (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);
                break;
            case PerfEvent::DTlbWriteMiss:
                attr.type = PERF_TYPE_HW_CACHE;
                attr.config = PERF_COUNT_HW_CACHE_DTLB |
                             (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
                             (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);
                break;
            case PerfEvent::ITlbReadMiss:
                attr.type = PERF_TYPE_HW_CACHE;
                attr.config = PERF_COUNT_HW_CACHE_ITLB |
                             (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                             (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);
                break;
            default:
                return false;
        }
        return true;
    }

    std::vector<int> fds_;
    std::vector<PerfEvent> events_;
};

/// RAII wrapper for scoped performance counting
class ScopedPerfCounters {
public:
    explicit ScopedPerfCounters(PerfCounterGroup& group)
        : group_(group) {
        group_.start();
    }

    ~ScopedPerfCounters() {
        group_.stop();
    }

    ScopedPerfCounters(const ScopedPerfCounters&) = delete;
    ScopedPerfCounters& operator=(const ScopedPerfCounters&) = delete;

private:
    PerfCounterGroup& group_;
};

#else // !__linux__

// Stub implementation for non-Linux platforms
class PerfCounterGroup {
public:
    bool add(PerfEvent) { return false; }
    void start() {}
    void stop() {}
    [[nodiscard]] std::vector<PerfResult> read() const { return {}; }
    [[nodiscard]] size_t size() const noexcept { return 0; }
    [[nodiscard]] bool empty() const noexcept { return true; }
};

class ScopedPerfCounters {
public:
    explicit ScopedPerfCounters(PerfCounterGroup&) {}
};

#endif // __linux__

} // namespace nfx::bench
