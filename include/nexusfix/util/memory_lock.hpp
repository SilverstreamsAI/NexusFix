/*
    NexusFIX Memory Locking Utilities

    Modern C++ #61: mlockall Memory Locking

    Lock process memory into RAM to prevent page faults during trading.
    Page fault penalty: ~10-100us (vs ~100ns for cache miss)

    Capabilities required:
    - Linux: CAP_IPC_LOCK or root
    - Windows: SeLockMemoryPrivilege

    Use cases:
    - Lock trading application memory before market open
    - Prevent latency spikes from page faults
    - Ensure deterministic execution times

    Thread-safety: All functions are thread-safe.
*/

#pragma once

#include <nexusfix/types/error.hpp>
#include <cstddef>

#ifdef _WIN32
    #include <windows.h>
    #include <memoryapi.h>
#else
    #include <sys/mman.h>
    #include <unistd.h>
    #include <sys/resource.h>
    #include <cerrno>
#endif

namespace nfx::util {

// ============================================================================
// Memory Lock Error
// ============================================================================

enum class MemoryLockErrorCode : uint8_t {
    None = 0,
    InsufficientPrivileges,
    InsufficientMemory,
    NotSupported,
    SystemError
};

struct MemoryLockError {
    MemoryLockErrorCode code;
    int system_errno;

    constexpr MemoryLockError() noexcept
        : code{MemoryLockErrorCode::None}, system_errno{0} {}

    constexpr MemoryLockError(MemoryLockErrorCode c) noexcept
        : code{c}, system_errno{0} {}

    constexpr MemoryLockError(MemoryLockErrorCode c, int err) noexcept
        : code{c}, system_errno{err} {}

    [[nodiscard]] constexpr bool ok() const noexcept {
        return code == MemoryLockErrorCode::None;
    }

    [[nodiscard]] constexpr std::string_view message() const noexcept {
        switch (code) {
            case MemoryLockErrorCode::None:
                return "No error";
            case MemoryLockErrorCode::InsufficientPrivileges:
                return "Insufficient privileges (need CAP_IPC_LOCK or root)";
            case MemoryLockErrorCode::InsufficientMemory:
                return "Insufficient memory (check RLIMIT_MEMLOCK)";
            case MemoryLockErrorCode::NotSupported:
                return "Memory locking not supported on this platform";
            case MemoryLockErrorCode::SystemError:
                return "System error";
        }
        return "Unknown error";
    }
};

template<typename T>
using MemoryLockResult = std::expected<T, MemoryLockError>;

// ============================================================================
// Memory Lock Functions
// ============================================================================

#ifdef _WIN32

/// Lock all current and future memory pages (Windows)
[[nodiscard]] inline MemoryLockResult<void> lock_all_memory() noexcept {
    // Windows doesn't have mlockall equivalent
    // Must lock specific memory ranges with VirtualLock
    return std::unexpected(MemoryLockError{
        MemoryLockErrorCode::NotSupported
    });
}

/// Unlock all memory pages (Windows)
inline void unlock_all_memory() noexcept {
    // No-op on Windows
}

/// Lock specific memory range (Windows)
[[nodiscard]] inline MemoryLockResult<void> lock_memory(
    void* addr, std::size_t length) noexcept
{
    if (!VirtualLock(addr, length)) {
        DWORD err = GetLastError();
        if (err == ERROR_PRIVILEGE_NOT_HELD) {
            return std::unexpected(MemoryLockError{
                MemoryLockErrorCode::InsufficientPrivileges,
                static_cast<int>(err)
            });
        }
        if (err == ERROR_NOT_ENOUGH_MEMORY || err == ERROR_WORKING_SET_QUOTA) {
            return std::unexpected(MemoryLockError{
                MemoryLockErrorCode::InsufficientMemory,
                static_cast<int>(err)
            });
        }
        return std::unexpected(MemoryLockError{
            MemoryLockErrorCode::SystemError,
            static_cast<int>(err)
        });
    }
    return {};
}

/// Unlock specific memory range (Windows)
inline void unlock_memory(void* addr, std::size_t length) noexcept {
    VirtualUnlock(addr, length);
}

#else  // POSIX (Linux, macOS)

/// Lock all current and future memory pages (Linux/macOS)
[[nodiscard]] inline MemoryLockResult<void> lock_all_memory() noexcept {
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
        int err = errno;
        if (err == EPERM) {
            return std::unexpected(MemoryLockError{
                MemoryLockErrorCode::InsufficientPrivileges, err
            });
        }
        if (err == ENOMEM) {
            return std::unexpected(MemoryLockError{
                MemoryLockErrorCode::InsufficientMemory, err
            });
        }
        return std::unexpected(MemoryLockError{
            MemoryLockErrorCode::SystemError, err
        });
    }
    return {};
}

/// Unlock all memory pages (Linux/macOS)
inline void unlock_all_memory() noexcept {
    munlockall();
}

/// Lock specific memory range (Linux/macOS)
[[nodiscard]] inline MemoryLockResult<void> lock_memory(
    void* addr, std::size_t length) noexcept
{
    if (mlock(addr, length) != 0) {
        int err = errno;
        if (err == EPERM) {
            return std::unexpected(MemoryLockError{
                MemoryLockErrorCode::InsufficientPrivileges, err
            });
        }
        if (err == ENOMEM) {
            return std::unexpected(MemoryLockError{
                MemoryLockErrorCode::InsufficientMemory, err
            });
        }
        return std::unexpected(MemoryLockError{
            MemoryLockErrorCode::SystemError, err
        });
    }
    return {};
}

/// Unlock specific memory range (Linux/macOS)
inline void unlock_memory(void* addr, std::size_t length) noexcept {
    munlock(addr, length);
}

/// Get current memory lock limit (RLIMIT_MEMLOCK)
[[nodiscard]] inline std::size_t get_memlock_limit() noexcept {
    struct rlimit rlim;
    if (getrlimit(RLIMIT_MEMLOCK, &rlim) == 0) {
        return rlim.rlim_cur;
    }
    return 0;
}

/// Set memory lock limit (requires privileges)
[[nodiscard]] inline MemoryLockResult<void> set_memlock_limit(
    std::size_t soft_limit, std::size_t hard_limit) noexcept
{
    struct rlimit rlim;
    rlim.rlim_cur = soft_limit;
    rlim.rlim_max = hard_limit;

    if (setrlimit(RLIMIT_MEMLOCK, &rlim) != 0) {
        int err = errno;
        if (err == EPERM) {
            return std::unexpected(MemoryLockError{
                MemoryLockErrorCode::InsufficientPrivileges, err
            });
        }
        return std::unexpected(MemoryLockError{
            MemoryLockErrorCode::SystemError, err
        });
    }
    return {};
}

#endif  // _WIN32

// ============================================================================
// RAII Wrappers
// ============================================================================

/// RAII wrapper that locks all memory on construction
class ScopedMemoryLock {
public:
    ScopedMemoryLock() noexcept {
        auto result = lock_all_memory();
        locked_ = result.has_value();
    }

    ~ScopedMemoryLock() {
        if (locked_) {
            unlock_all_memory();
        }
    }

    ScopedMemoryLock(const ScopedMemoryLock&) = delete;
    ScopedMemoryLock& operator=(const ScopedMemoryLock&) = delete;
    ScopedMemoryLock(ScopedMemoryLock&&) = delete;
    ScopedMemoryLock& operator=(ScopedMemoryLock&&) = delete;

    [[nodiscard]] bool is_locked() const noexcept { return locked_; }

private:
    bool locked_{false};
};

/// RAII wrapper for locking specific memory range
class ScopedRangeLock {
public:
    ScopedRangeLock(void* addr, std::size_t length) noexcept
        : addr_{addr}, length_{length} {
        auto result = lock_memory(addr, length);
        locked_ = result.has_value();
    }

    ~ScopedRangeLock() {
        if (locked_) {
            unlock_memory(addr_, length_);
        }
    }

    ScopedRangeLock(const ScopedRangeLock&) = delete;
    ScopedRangeLock& operator=(const ScopedRangeLock&) = delete;
    ScopedRangeLock(ScopedRangeLock&&) = delete;
    ScopedRangeLock& operator=(ScopedRangeLock&&) = delete;

    [[nodiscard]] bool is_locked() const noexcept { return locked_; }

private:
    void* addr_;
    std::size_t length_;
    bool locked_{false};
};

// ============================================================================
// Memory Prefault
// ============================================================================

/// Touch all pages in range to trigger page faults before critical section
/// This brings pages into memory and can be done before trading starts
inline void prefault_memory(void* addr, std::size_t length) noexcept {
    volatile char* p = static_cast<volatile char*>(addr);
    volatile char dummy = 0;

    // Touch each page (typically 4KB)
    constexpr std::size_t page_size = 4096;
    for (std::size_t i = 0; i < length; i += page_size) {
        dummy = p[i];  // Read to fault page in
    }
    // Touch last byte in case length doesn't align
    if (length > 0) {
        dummy = p[length - 1];
    }
    (void)dummy;  // Suppress unused warning
}

/// Touch all pages for write (forces copy-on-write pages to be copied)
inline void prefault_memory_write(void* addr, std::size_t length) noexcept {
    volatile char* p = static_cast<volatile char*>(addr);

    constexpr std::size_t page_size = 4096;
    for (std::size_t i = 0; i < length; i += page_size) {
        p[i] = p[i];  // Read-modify-write to trigger write fault
    }
    if (length > 0) {
        p[length - 1] = p[length - 1];
    }
}

// ============================================================================
// Memory Advice (Linux-specific)
// ============================================================================

#ifndef _WIN32

/// Advise kernel about memory usage patterns
enum class MemoryAdvice : int {
    Normal = MADV_NORMAL,       // No special treatment
    Random = MADV_RANDOM,       // Expect random access pattern
    Sequential = MADV_SEQUENTIAL, // Expect sequential access
    WillNeed = MADV_WILLNEED,   // Expect access soon (prefetch)
    DontNeed = MADV_DONTNEED,   // Don't expect access soon
#ifdef MADV_HUGEPAGE
    Hugepage = MADV_HUGEPAGE,   // Prefer huge pages
#endif
#ifdef MADV_NOHUGEPAGE
    NoHugepage = MADV_NOHUGEPAGE, // Don't use huge pages
#endif
};

/// Give advice to kernel about memory region
[[nodiscard]] inline MemoryLockResult<void> advise_memory(
    void* addr, std::size_t length, MemoryAdvice advice) noexcept
{
    if (madvise(addr, length, static_cast<int>(advice)) != 0) {
        return std::unexpected(MemoryLockError{
            MemoryLockErrorCode::SystemError, errno
        });
    }
    return {};
}

/// Prefetch memory region (MADV_WILLNEED)
inline void prefetch_region(void* addr, std::size_t length) noexcept {
    madvise(addr, length, MADV_WILLNEED);
}

#endif  // !_WIN32

} // namespace nfx::util
