#pragma once

/// @file socket_types.hpp
/// @brief Cross-platform socket type aliases and helper functions

#include "nexusfix/platform/platform.hpp"

#include <cstdint>
#include <cstddef>

// ============================================================================
// Platform-specific Headers
// ============================================================================

#if NFX_PLATFORM_WINDOWS
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <mswsock.h>
    // Link with Winsock library
    #pragma comment(lib, "ws2_32.lib")
    #pragma comment(lib, "mswsock.lib")
#else  // POSIX (Linux, macOS)
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <poll.h>
    #include <cerrno>
#endif

namespace nfx {

// ============================================================================
// Socket Handle Type
// ============================================================================

#if NFX_PLATFORM_WINDOWS
    /// Socket handle type (SOCKET on Windows, int on POSIX)
    using SocketHandle = SOCKET;

    /// Invalid socket handle constant
    constexpr SocketHandle INVALID_SOCKET_HANDLE = INVALID_SOCKET;

    /// Socket error return value
    constexpr int SOCKET_ERROR_VALUE = SOCKET_ERROR;
#else
    /// Socket handle type (SOCKET on Windows, int on POSIX)
    using SocketHandle = int;

    /// Invalid socket handle constant
    constexpr SocketHandle INVALID_SOCKET_HANDLE = -1;

    /// Socket error return value
    constexpr int SOCKET_ERROR_VALUE = -1;
#endif

// ============================================================================
// Size Types for Socket Operations
// ============================================================================

#if NFX_PLATFORM_WINDOWS
    /// Socket address length type
    using SocketLength = int;

    /// I/O operation size type (return from send/recv)
    using IoSize = int;

    /// Signed size for socket operations
    using SSocketSize = int;
#else
    /// Socket address length type
    using SocketLength = socklen_t;

    /// I/O operation size type (return from send/recv)
    using IoSize = ssize_t;

    /// Signed size for socket operations
    using SSocketSize = ssize_t;
#endif

// ============================================================================
// Socket Option Types
// ============================================================================

#if NFX_PLATFORM_WINDOWS
    /// Type for setsockopt value parameter
    using SockOptValue = const char*;

    /// Type for getsockopt value parameter
    using SockOptValueMut = char*;
#else
    /// Type for setsockopt value parameter
    using SockOptValue = const void*;

    /// Type for getsockopt value parameter
    using SockOptValueMut = void*;
#endif

// ============================================================================
// Helper Functions
// ============================================================================

/// Get last socket error code (WSAGetLastError on Windows, errno on POSIX)
[[nodiscard]] inline int get_last_socket_error() noexcept {
#if NFX_PLATFORM_WINDOWS
    return WSAGetLastError();
#else
    return errno;
#endif
}

/// Set last socket error code
inline void set_last_socket_error(int error) noexcept {
#if NFX_PLATFORM_WINDOWS
    WSASetLastError(error);
#else
    errno = error;
#endif
}

/// Close a socket handle
inline void close_socket(SocketHandle socket) noexcept {
    if (socket != INVALID_SOCKET_HANDLE) {
#if NFX_PLATFORM_WINDOWS
        ::closesocket(socket);
#else
        ::close(socket);
#endif
    }
}

/// Check if socket handle is valid
[[nodiscard]] inline bool is_valid_socket(SocketHandle socket) noexcept {
    return socket != INVALID_SOCKET_HANDLE;
}

/// Cast pointer for setsockopt (handles char* vs void* difference)
template <typename T>
[[nodiscard]] inline SockOptValue sockopt_ptr(const T* value) noexcept {
#if NFX_PLATFORM_WINDOWS
    return reinterpret_cast<const char*>(value);
#else
    return static_cast<const void*>(value);
#endif
}

/// Cast pointer for getsockopt (handles char* vs void* difference)
template <typename T>
[[nodiscard]] inline SockOptValueMut sockopt_ptr_mut(T* value) noexcept {
#if NFX_PLATFORM_WINDOWS
    return reinterpret_cast<char*>(value);
#else
    return static_cast<void*>(value);
#endif
}

// ============================================================================
// MSG_NOSIGNAL Compatibility
// ============================================================================

// MSG_NOSIGNAL prevents SIGPIPE on send() to closed socket (Linux only)
// Windows and macOS handle this differently
#if NFX_PLATFORM_LINUX
    constexpr int MSG_NOSIGNAL_COMPAT = MSG_NOSIGNAL;
#else
    constexpr int MSG_NOSIGNAL_COMPAT = 0;
#endif

// ============================================================================
// Non-blocking Mode
// ============================================================================

/// Set socket to non-blocking mode
/// @return true on success
[[nodiscard]] inline bool set_socket_nonblocking(SocketHandle socket, bool nonblocking) noexcept {
#if NFX_PLATFORM_WINDOWS
    u_long mode = nonblocking ? 1 : 0;
    return ::ioctlsocket(socket, FIONBIO, &mode) == 0;
#else
    int flags = ::fcntl(socket, F_GETFL, 0);
    if (flags == -1) return false;

    if (nonblocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }

    return ::fcntl(socket, F_SETFL, flags) == 0;
#endif
}

// ============================================================================
// Socket Options Helpers
// ============================================================================

/// Set TCP_NODELAY option (disable Nagle's algorithm)
[[nodiscard]] inline bool set_tcp_nodelay(SocketHandle socket, bool enable) noexcept {
    int flag = enable ? 1 : 0;
    return ::setsockopt(socket, IPPROTO_TCP, TCP_NODELAY,
                        sockopt_ptr(&flag), sizeof(flag)) == 0;
}

/// Set SO_KEEPALIVE option
[[nodiscard]] inline bool set_socket_keepalive(SocketHandle socket, bool enable) noexcept {
    int flag = enable ? 1 : 0;
    return ::setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE,
                        sockopt_ptr(&flag), sizeof(flag)) == 0;
}

/// Set SO_REUSEADDR option
[[nodiscard]] inline bool set_socket_reuseaddr(SocketHandle socket, bool enable) noexcept {
    int flag = enable ? 1 : 0;
    return ::setsockopt(socket, SOL_SOCKET, SO_REUSEADDR,
                        sockopt_ptr(&flag), sizeof(flag)) == 0;
}

/// Set socket receive buffer size
[[nodiscard]] inline bool set_socket_recv_buffer(SocketHandle socket, int size) noexcept {
    return ::setsockopt(socket, SOL_SOCKET, SO_RCVBUF,
                        sockopt_ptr(&size), sizeof(size)) == 0;
}

/// Set socket send buffer size
[[nodiscard]] inline bool set_socket_send_buffer(SocketHandle socket, int size) noexcept {
    return ::setsockopt(socket, SOL_SOCKET, SO_SNDBUF,
                        sockopt_ptr(&size), sizeof(size)) == 0;
}

/// Set socket receive timeout
[[nodiscard]] inline bool set_socket_recv_timeout(SocketHandle socket, int milliseconds) noexcept {
#if NFX_PLATFORM_WINDOWS
    DWORD timeout = static_cast<DWORD>(milliseconds);
    return ::setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO,
                        sockopt_ptr(&timeout), sizeof(timeout)) == 0;
#else
    struct timeval tv;
    tv.tv_sec = milliseconds / 1000;
    tv.tv_usec = (milliseconds % 1000) * 1000;
    return ::setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO,
                        sockopt_ptr(&tv), sizeof(tv)) == 0;
#endif
}

/// Set socket send timeout
[[nodiscard]] inline bool set_socket_send_timeout(SocketHandle socket, int milliseconds) noexcept {
#if NFX_PLATFORM_WINDOWS
    DWORD timeout = static_cast<DWORD>(milliseconds);
    return ::setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO,
                        sockopt_ptr(&timeout), sizeof(timeout)) == 0;
#else
    struct timeval tv;
    tv.tv_sec = milliseconds / 1000;
    tv.tv_usec = (milliseconds % 1000) * 1000;
    return ::setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO,
                        sockopt_ptr(&tv), sizeof(tv)) == 0;
#endif
}

// ============================================================================
// Error Code Checks
// ============================================================================

/// Check if error indicates operation would block (non-blocking socket)
[[nodiscard]] inline bool is_would_block_error(int error) noexcept {
#if NFX_PLATFORM_WINDOWS
    return error == WSAEWOULDBLOCK;
#else
    return error == EAGAIN || error == EWOULDBLOCK;
#endif
}

/// Check if error indicates operation in progress (non-blocking connect)
[[nodiscard]] inline bool is_in_progress_error(int error) noexcept {
#if NFX_PLATFORM_WINDOWS
    return error == WSAEWOULDBLOCK || error == WSAEINPROGRESS;
#else
    return error == EINPROGRESS;
#endif
}

/// Check if error indicates connection was reset by peer
[[nodiscard]] inline bool is_connection_reset_error(int error) noexcept {
#if NFX_PLATFORM_WINDOWS
    return error == WSAECONNRESET || error == WSAECONNABORTED;
#else
    return error == ECONNRESET || error == ECONNABORTED || error == EPIPE;
#endif
}

} // namespace nfx
