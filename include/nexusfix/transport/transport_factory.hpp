#pragma once

/// @file transport_factory.hpp
/// @brief Factory for creating platform-appropriate transport implementations
///
/// Provides a unified interface for creating transports that automatically
/// selects the best implementation for the current platform:
/// - Linux: TcpTransport (POSIX) or IoUringTransport (if available)
/// - Windows: WinsockTransport or IocpTransport (future)
/// - macOS: TcpTransport (POSIX) or KqueueTransport (future)

#include "nexusfix/platform/platform.hpp"
#include "nexusfix/transport/socket.hpp"

#include <memory>

// Include platform-appropriate headers
#if NFX_PLATFORM_WINDOWS
    #include "nexusfix/transport/winsock_transport.hpp"
#else
    #include "nexusfix/transport/tcp_transport.hpp"
#endif

// Include async transport if available
#if NFX_PLATFORM_LINUX && NFX_ASYNC_IO_IOURING
    #include "nexusfix/transport/io_uring_transport.hpp"
#endif

namespace nfx {

// ============================================================================
// Transport Preference
// ============================================================================

/// Transport implementation preference
enum class TransportPreference : uint8_t {
    /// Platform default (best available for current platform)
    Default,

    /// High-performance async I/O (io_uring/IOCP/kqueue)
    HighPerf,

    /// Simple blocking TCP (POSIX sockets/Winsock)
    Simple,

    /// Explicit transport selection
    TcpPosix,       // POSIX TCP (Linux/macOS)
    IoUring,        // Linux io_uring
    Winsock,        // Windows Winsock2
    Iocp,           // Windows IOCP (future)
    Kqueue          // macOS kqueue (future)
};

// ============================================================================
// Platform-specific Type Aliases
// ============================================================================

#if NFX_PLATFORM_WINDOWS
    /// Default socket type for current platform
    using PlatformSocket = WinsockSocket;

    /// Default transport type for current platform
    using PlatformTransport = WinsockTransport;

    /// Default acceptor type for current platform
    using PlatformAcceptor = WinsockAcceptor;
#else
    /// Default socket type for current platform
    using PlatformSocket = TcpSocket;

    /// Default transport type for current platform
    using PlatformTransport = TcpTransport;

    /// Default acceptor type for current platform
    using PlatformAcceptor = TcpAcceptor;
#endif

// ============================================================================
// Transport Factory
// ============================================================================

/// Factory for creating platform-appropriate transports
class TransportFactory {
public:
    /// Create transport with platform-optimal implementation
    /// @param pref Transport preference (default: platform best)
    /// @return Unique pointer to transport, or nullptr on failure
    [[nodiscard]] static std::unique_ptr<ITransport> create(
        TransportPreference pref = TransportPreference::Default) noexcept
    {
        switch (pref) {
            case TransportPreference::Simple:
            case TransportPreference::TcpPosix:
            case TransportPreference::Winsock:
                return create_simple();

            case TransportPreference::IoUring:
                return create_io_uring();

            case TransportPreference::Iocp:
                return create_iocp();

            case TransportPreference::Kqueue:
                return create_kqueue();

            case TransportPreference::HighPerf:
            case TransportPreference::Default:
            default:
                return create_best();
        }
    }

    /// Create simple blocking transport for current platform
    [[nodiscard]] static std::unique_ptr<ITransport> create_simple() noexcept {
#if NFX_PLATFORM_WINDOWS
        return std::make_unique<WinsockTransport>();
#else
        return std::make_unique<TcpTransport>();
#endif
    }

    /// Create io_uring transport (Linux only)
    /// Returns simple transport on other platforms or if io_uring unavailable
    [[nodiscard]] static std::unique_ptr<ITransport> create_io_uring() noexcept {
#if NFX_PLATFORM_LINUX && NFX_ASYNC_IO_IOURING
        // Create or get shared io_uring context
        static IoUringContext ctx;
        if (!ctx.is_initialized()) {
            if (auto result = ctx.init(); !result) {
                // io_uring init failed, fall back to simple transport
                return create_simple();
            }
        }
        return std::make_unique<IoUringTransport>(ctx);
#else
        // io_uring not available, fall back to simple transport
        return create_simple();
#endif
    }

    /// Create IOCP transport (Windows only)
    /// Returns simple transport on other platforms or if IOCP unavailable
    [[nodiscard]] static std::unique_ptr<ITransport> create_iocp() noexcept {
#if NFX_PLATFORM_WINDOWS && NFX_ASYNC_IO_IOCP
        // TODO: Implement IOCP transport in Phase 3
        // For now, fall back to Winsock
        return create_simple();
#else
        return create_simple();
#endif
    }

    /// Create kqueue transport (macOS only)
    /// Returns simple transport on other platforms or if kqueue unavailable
    [[nodiscard]] static std::unique_ptr<ITransport> create_kqueue() noexcept {
#if NFX_PLATFORM_MACOS && NFX_ASYNC_IO_KQUEUE
        // TODO: Implement kqueue transport in Phase 4
        // For now, fall back to POSIX TCP
        return create_simple();
#else
        return create_simple();
#endif
    }

    /// Create best available transport for current platform
    [[nodiscard]] static std::unique_ptr<ITransport> create_best() noexcept {
#if NFX_PLATFORM_LINUX && NFX_ASYNC_IO_IOURING
        return create_io_uring();
#elif NFX_PLATFORM_WINDOWS && NFX_ASYNC_IO_IOCP
        return create_iocp();
#elif NFX_PLATFORM_MACOS && NFX_ASYNC_IO_KQUEUE
        return create_kqueue();
#else
        return create_simple();
#endif
    }

    // ========================================================================
    // Platform Information
    // ========================================================================

    /// Get platform name
    [[nodiscard]] static constexpr const char* platform_name() noexcept {
        return NFX_PLATFORM_NAME;
    }

    /// Get async I/O backend name
    [[nodiscard]] static constexpr const char* async_backend_name() noexcept {
        return NFX_ASYNC_IO_BACKEND_NAME;
    }

    /// Check if high-performance async I/O is available
    [[nodiscard]] static constexpr bool has_async_io() noexcept {
        return NFX_HAS_ASYNC_IO;
    }

    /// Check if io_uring is available
    [[nodiscard]] static constexpr bool has_io_uring() noexcept {
        return NFX_ASYNC_IO_IOURING;
    }

    /// Check if IOCP is available
    [[nodiscard]] static constexpr bool has_iocp() noexcept {
        return NFX_ASYNC_IO_IOCP;
    }

    /// Check if kqueue is available
    [[nodiscard]] static constexpr bool has_kqueue() noexcept {
        return NFX_ASYNC_IO_KQUEUE;
    }

    /// Get description of what create() will return
    [[nodiscard]] static constexpr const char* default_transport_name() noexcept {
#if NFX_PLATFORM_LINUX && NFX_ASYNC_IO_IOURING
        return "IoUringTransport";
#elif NFX_PLATFORM_WINDOWS
        return "WinsockTransport";
#elif NFX_PLATFORM_MACOS
        return "TcpTransport (POSIX)";
#else
        return "TcpTransport (POSIX)";
#endif
    }
};

// ============================================================================
// Convenience Functions
// ============================================================================

/// Create default transport for current platform
[[nodiscard]] inline std::unique_ptr<ITransport> make_transport() noexcept {
    return TransportFactory::create();
}

/// Create simple blocking transport
[[nodiscard]] inline std::unique_ptr<ITransport> make_simple_transport() noexcept {
    return TransportFactory::create_simple();
}

/// Create high-performance transport (if available)
[[nodiscard]] inline std::unique_ptr<ITransport> make_fast_transport() noexcept {
    return TransportFactory::create(TransportPreference::HighPerf);
}

} // namespace nfx
