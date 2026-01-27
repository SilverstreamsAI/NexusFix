#pragma once

/// @file winsock_init.hpp
/// @brief RAII wrapper for Windows Winsock initialization
///
/// On Windows, Winsock must be initialized with WSAStartup() before any
/// socket operations, and cleaned up with WSACleanup() on exit.
/// This header provides automatic initialization via a singleton pattern.

#include "nexusfix/platform/platform.hpp"

#if NFX_PLATFORM_WINDOWS

#include "nexusfix/platform/socket_types.hpp"
#include "nexusfix/types/error.hpp"

#include <atomic>

namespace nfx {

// ============================================================================
// Winsock Initialization (Windows only)
// ============================================================================

/// RAII wrapper for Winsock initialization
/// Thread-safe singleton ensures WSAStartup is called exactly once.
///
/// Usage:
///   // Option 1: Explicit initialization (recommended at program start)
///   if (!WinsockInit::initialize()) {
///       // Handle error
///   }
///
///   // Option 2: Automatic initialization (lazy, on first socket use)
///   // Just create sockets - WinsockInit::ensure() is called internally
///
class WinsockInit {
public:
    /// Initialize Winsock subsystem
    /// Thread-safe, idempotent - safe to call multiple times
    /// @return true if Winsock is ready to use
    [[nodiscard]] static bool initialize() noexcept {
        return instance().init();
    }

    /// Ensure Winsock is initialized (alias for initialize)
    /// @return true if Winsock is ready to use
    [[nodiscard]] static bool ensure() noexcept {
        return initialize();
    }

    /// Check if Winsock has been successfully initialized
    [[nodiscard]] static bool is_initialized() noexcept {
        return instance().initialized_.load(std::memory_order_acquire);
    }

    /// Get last initialization error code (if initialization failed)
    [[nodiscard]] static int last_error() noexcept {
        return instance().error_code_;
    }

    /// Get Winsock version info (valid only after successful init)
    [[nodiscard]] static const WSADATA& wsa_data() noexcept {
        return instance().wsa_data_;
    }

    /// Create TransportError for Winsock init failure
    [[nodiscard]] static TransportError make_init_error() noexcept {
        return TransportError{TransportErrorCode::WinsockInitFailed, last_error()};
    }

private:
    WinsockInit() noexcept
        : initialized_{false}
        , error_code_{0}
        , wsa_data_{} {}

    ~WinsockInit() {
        if (initialized_.load(std::memory_order_acquire)) {
            WSACleanup();
        }
    }

    // Non-copyable, non-movable
    WinsockInit(const WinsockInit&) = delete;
    WinsockInit& operator=(const WinsockInit&) = delete;

    /// Get singleton instance
    static WinsockInit& instance() noexcept {
        static WinsockInit inst;
        return inst;
    }

    /// Perform actual initialization (thread-safe)
    [[nodiscard]] bool init() noexcept {
        // Fast path: already initialized
        if (initialized_.load(std::memory_order_acquire)) {
            return true;
        }

        // Slow path: need to initialize
        // Use compare_exchange to ensure only one thread initializes
        bool expected = false;
        if (!initializing_.compare_exchange_strong(expected, true,
                                                   std::memory_order_acq_rel)) {
            // Another thread is initializing, spin-wait for completion
            while (!initialized_.load(std::memory_order_acquire)) {
                // Yield to avoid busy-wait
                Sleep(0);
            }
            return error_code_ == 0;
        }

        // We won the race - perform initialization
        // Request Winsock 2.2
        int result = WSAStartup(MAKEWORD(2, 2), &wsa_data_);
        if (result != 0) {
            error_code_ = result;
            initializing_.store(false, std::memory_order_release);
            return false;
        }

        // Verify we got Winsock 2.2
        if (LOBYTE(wsa_data_.wVersion) != 2 || HIBYTE(wsa_data_.wVersion) != 2) {
            error_code_ = WSAVERNOTSUPPORTED;
            WSACleanup();
            initializing_.store(false, std::memory_order_release);
            return false;
        }

        error_code_ = 0;
        initialized_.store(true, std::memory_order_release);
        return true;
    }

    std::atomic<bool> initialized_;
    std::atomic<bool> initializing_{false};
    int error_code_;
    WSADATA wsa_data_;
};

// ============================================================================
// Helper Macro for Early Initialization
// ============================================================================

/// Macro to ensure Winsock is initialized at static init time
/// Usage: Place NFX_WINSOCK_INIT in one .cpp file (e.g., main.cpp)
#define NFX_WINSOCK_INIT                                    \
    namespace {                                             \
    struct WinsockAutoInit {                                \
        WinsockAutoInit() { nfx::WinsockInit::initialize(); } \
    } _nfx_winsock_init;                                    \
    }

} // namespace nfx

#else  // !NFX_PLATFORM_WINDOWS

// ============================================================================
// Stub for non-Windows platforms
// ============================================================================

namespace nfx {

/// No-op Winsock initialization for non-Windows platforms
class WinsockInit {
public:
    [[nodiscard]] static bool initialize() noexcept { return true; }
    [[nodiscard]] static bool ensure() noexcept { return true; }
    [[nodiscard]] static bool is_initialized() noexcept { return true; }
    [[nodiscard]] static int last_error() noexcept { return 0; }
};

#define NFX_WINSOCK_INIT

} // namespace nfx

#endif  // NFX_PLATFORM_WINDOWS
