/*
    NexusFIX Sequence Lock (Seqlock)

    Modern C++ #78: Seqlock (Sequence Lock)

    A reader-writer synchronization primitive optimized for:
    - Single writer, multiple readers
    - Readers never block (retry on conflict)
    - Writers are lock-free

    Use cases:
    - Real-time market data publishing
    - Frequently-read, rarely-written shared state
    - Low-latency data distribution

    Properties:
    - Readers: Wait-free (with retry)
    - Writers: Lock-free
    - No reader starvation
    - Writer priority (readers retry during write)

    Cache-line alignment prevents false sharing.
*/

#pragma once

#include <atomic>
#include <cstdint>
#include <new>
#include <type_traits>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#define NFX_SEQLOCK_PAUSE() _mm_pause()
#elif defined(__aarch64__)
#define NFX_SEQLOCK_PAUSE() __asm__ __volatile__("yield" ::: "memory")
#else
#define NFX_SEQLOCK_PAUSE() std::atomic_thread_fence(std::memory_order_seq_cst)
#endif

namespace nfx::memory {

// ============================================================================
// Seqlock Configuration
// ============================================================================

/// Cache line size for alignment
inline constexpr std::size_t CACHE_LINE_SIZE =
#ifdef __cpp_lib_hardware_interference_size
    std::hardware_destructive_interference_size;
#else
    64;
#endif

// ============================================================================
// Seqlock Implementation
// ============================================================================

/// Sequence lock for single-writer, multiple-reader synchronization
/// @tparam T Data type (must be trivially copyable)
template<typename T>
    requires std::is_trivially_copyable_v<T>
class Seqlock {
public:
    Seqlock() noexcept = default;

    explicit Seqlock(const T& initial) noexcept
        : data_{initial} {}

    Seqlock(const Seqlock&) = delete;
    Seqlock& operator=(const Seqlock&) = delete;

    // ========================================================================
    // Writer API (single writer only)
    // ========================================================================

    /// Write new value (single writer only)
    /// Thread-safety: Must be called by single writer thread
    void write(const T& value) noexcept {
        // Increment to odd (write in progress)
        uint64_t seq = sequence_.load(std::memory_order_relaxed);
        sequence_.store(seq + 1, std::memory_order_release);

        // Write data
        data_ = value;

        // Increment to even (write complete)
        sequence_.store(seq + 2, std::memory_order_release);
    }

    /// Begin write operation (for multi-field updates)
    /// Must be paired with end_write()
    void begin_write() noexcept {
        uint64_t seq = sequence_.load(std::memory_order_relaxed);
        sequence_.store(seq + 1, std::memory_order_release);
    }

    /// End write operation
    void end_write() noexcept {
        uint64_t seq = sequence_.load(std::memory_order_relaxed);
        sequence_.store(seq + 1, std::memory_order_release);
    }

    /// Get reference to data for writing (use between begin_write/end_write)
    [[nodiscard]] T& data() noexcept {
        return data_;
    }

    // ========================================================================
    // Reader API (multiple readers)
    // ========================================================================

    /// Read value (retries on write conflict)
    /// Thread-safety: Safe for multiple concurrent readers
    [[nodiscard]] T read() const noexcept {
        T result;
        uint64_t seq1, seq2;

        do {
            // Wait for writer to finish (odd sequence = write in progress)
            seq1 = sequence_.load(std::memory_order_acquire);
            while (seq1 & 1) [[unlikely]] {
                NFX_SEQLOCK_PAUSE();
                seq1 = sequence_.load(std::memory_order_acquire);
            }

            // Copy data
            result = data_;

            // Verify no write occurred during read
            std::atomic_thread_fence(std::memory_order_acquire);
            seq2 = sequence_.load(std::memory_order_relaxed);

        } while (seq1 != seq2);

        return result;
    }

    /// Try to read value once (no retry)
    /// @return true if read was successful, false if write was in progress
    [[nodiscard]] bool try_read(T& out) const noexcept {
        uint64_t seq1 = sequence_.load(std::memory_order_acquire);

        // Fail if write in progress
        if (seq1 & 1) [[unlikely]] {
            return false;
        }

        // Copy data
        out = data_;

        // Verify no write occurred
        std::atomic_thread_fence(std::memory_order_acquire);
        uint64_t seq2 = sequence_.load(std::memory_order_relaxed);

        return seq1 == seq2;
    }

    /// Read with callback (avoids copy for large T)
    /// @param callback Function called with const T& while holding read lock
    template<typename Func>
        requires std::invocable<Func, const T&>
    [[nodiscard]] auto read_with(Func&& callback) const
        -> std::invoke_result_t<Func, const T&>
    {
        using Result = std::invoke_result_t<Func, const T&>;

        uint64_t seq1, seq2;
        Result result;

        do {
            seq1 = sequence_.load(std::memory_order_acquire);
            while (seq1 & 1) [[unlikely]] {
                NFX_SEQLOCK_PAUSE();
                seq1 = sequence_.load(std::memory_order_acquire);
            }

            result = callback(data_);

            std::atomic_thread_fence(std::memory_order_acquire);
            seq2 = sequence_.load(std::memory_order_relaxed);

        } while (seq1 != seq2);

        return result;
    }

    // ========================================================================
    // Status API
    // ========================================================================

    /// Get current sequence number
    [[nodiscard]] uint64_t sequence() const noexcept {
        return sequence_.load(std::memory_order_relaxed);
    }

    /// Check if write is in progress
    [[nodiscard]] bool is_writing() const noexcept {
        return sequence_.load(std::memory_order_relaxed) & 1;
    }

private:
    // Cache-line aligned sequence counter
    alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> sequence_{0};

    // Cache-line aligned data
    alignas(CACHE_LINE_SIZE) T data_{};
};

// ============================================================================
// Seqlock Guard (RAII Write Scope)
// ============================================================================

/// RAII guard for seqlock write operations
template<typename T>
class SeqlockWriteGuard {
public:
    explicit SeqlockWriteGuard(Seqlock<T>& lock) noexcept
        : lock_{lock} {
        lock_.begin_write();
    }

    ~SeqlockWriteGuard() {
        lock_.end_write();
    }

    SeqlockWriteGuard(const SeqlockWriteGuard&) = delete;
    SeqlockWriteGuard& operator=(const SeqlockWriteGuard&) = delete;

    /// Get reference to data for modification
    [[nodiscard]] T& data() noexcept {
        return lock_.data();
    }

    [[nodiscard]] T* operator->() noexcept {
        return &lock_.data();
    }

private:
    Seqlock<T>& lock_;
};

// ============================================================================
// Versioned Value (Seqlock + Version Tracking)
// ============================================================================

/// Value with version tracking for change detection
template<typename T>
    requires std::is_trivially_copyable_v<T>
class VersionedValue {
public:
    struct Snapshot {
        T value;
        uint64_t version;
    };

    VersionedValue() noexcept = default;

    explicit VersionedValue(const T& initial) noexcept
        : seqlock_{initial} {}

    /// Write new value
    void write(const T& value) noexcept {
        seqlock_.write(value);
    }

    /// Read value with version
    [[nodiscard]] Snapshot read() const noexcept {
        uint64_t seq1, seq2;
        T result;

        do {
            seq1 = seqlock_.sequence();
            while (seq1 & 1) [[unlikely]] {
                NFX_SEQLOCK_PAUSE();
                seq1 = seqlock_.sequence();
            }

            result = seqlock_.read_with([](const T& data) { return data; });

            seq2 = seqlock_.sequence();
        } while (seq1 != seq2);

        return {result, seq2 / 2};  // Version is half the sequence
    }

    /// Check if value changed since given version
    [[nodiscard]] bool changed_since(uint64_t version) const noexcept {
        return (seqlock_.sequence() / 2) > version;
    }

    /// Read only if changed since given version
    /// @return true if value was updated, false if no change
    [[nodiscard]] bool read_if_changed(T& out, uint64_t& version) const noexcept {
        auto snapshot = read();
        if (snapshot.version > version) {
            out = snapshot.value;
            version = snapshot.version;
            return true;
        }
        return false;
    }

private:
    Seqlock<T> seqlock_;
};

} // namespace nfx::memory

#undef NFX_SEQLOCK_PAUSE
