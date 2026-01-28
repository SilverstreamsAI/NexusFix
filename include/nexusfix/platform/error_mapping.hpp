#pragma once

/// @file error_mapping.hpp
/// @brief Maps OS-specific socket errors to TransportErrorCode

#include "nexusfix/platform/platform.hpp"
#include "nexusfix/platform/socket_types.hpp"
#include "nexusfix/types/error.hpp"

#include <cstring>

namespace nfx {

// ============================================================================
// OS Error to TransportErrorCode Mapping
// ============================================================================

/// Map OS-specific socket error code to TransportErrorCode
/// @param os_error Platform-specific error code (errno on POSIX, WSAGetLastError on Windows)
/// @return Corresponding TransportErrorCode
[[nodiscard]] inline TransportErrorCode map_socket_error(int os_error) noexcept {
#if NFX_PLATFORM_WINDOWS
    // Windows Winsock error codes
    switch (os_error) {
        case 0:                 return TransportErrorCode::None;

        // Connection errors
        case WSAECONNREFUSED:   return TransportErrorCode::ConnectionRefused;
        case WSAECONNRESET:     return TransportErrorCode::ConnectionReset;
        case WSAECONNABORTED:   return TransportErrorCode::ConnectionAborted;
        case WSAENOTCONN:       return TransportErrorCode::NotConnected;
        case WSAESHUTDOWN:      return TransportErrorCode::ConnectionClosed;
        case WSAEDISCON:        return TransportErrorCode::ConnectionClosed;

        // Network/host errors
        case WSAENETUNREACH:    return TransportErrorCode::NetworkUnreachable;
        case WSAEHOSTUNREACH:   return TransportErrorCode::HostUnreachable;
        case WSAENETDOWN:       return TransportErrorCode::NetworkUnreachable;
        case WSAEHOSTDOWN:      return TransportErrorCode::HostUnreachable;

        // Timeout
        case WSAETIMEDOUT:      return TransportErrorCode::Timeout;

        // Non-blocking / async
        case WSAEWOULDBLOCK:    return TransportErrorCode::WouldBlock;
        case WSAEINPROGRESS:    return TransportErrorCode::InProgress;
        case WSAEALREADY:       return TransportErrorCode::InProgress;

        // Buffer space
        case WSAENOBUFS:        return TransportErrorCode::NoBufferSpace;

        // Address resolution
        case WSAHOST_NOT_FOUND: return TransportErrorCode::AddressResolutionFailed;
        case WSATRY_AGAIN:      return TransportErrorCode::AddressResolutionFailed;
        case WSANO_RECOVERY:    return TransportErrorCode::AddressResolutionFailed;
        case WSANO_DATA:        return TransportErrorCode::AddressResolutionFailed;

        // Connection establishment
        case WSAEADDRNOTAVAIL:  return TransportErrorCode::ConnectionFailed;
        case WSAEAFNOSUPPORT:   return TransportErrorCode::ConnectionFailed;

        // General socket errors
        case WSAEBADF:          return TransportErrorCode::SocketError;
        case WSAENOTSOCK:       return TransportErrorCode::SocketError;
        case WSAEINVAL:         return TransportErrorCode::SocketError;
        case WSAEMFILE:         return TransportErrorCode::SocketError;
        case WSAEACCES:         return TransportErrorCode::SocketError;
        case WSAEFAULT:         return TransportErrorCode::SocketError;
        case WSANOTINITIALISED: return TransportErrorCode::WinsockInitFailed;
        case WSASYSNOTREADY:    return TransportErrorCode::WinsockInitFailed;
        case WSAVERNOTSUPPORTED: return TransportErrorCode::WinsockInitFailed;

        default:                return TransportErrorCode::SocketError;
    }
#else
    // POSIX errno values
    switch (os_error) {
        case 0:                 return TransportErrorCode::None;

        // Connection errors
        case ECONNREFUSED:      return TransportErrorCode::ConnectionRefused;
        case ECONNRESET:        return TransportErrorCode::ConnectionReset;
        case ECONNABORTED:      return TransportErrorCode::ConnectionAborted;
        case ENOTCONN:          return TransportErrorCode::NotConnected;
        case EPIPE:             return TransportErrorCode::ConnectionClosed;
        case ESHUTDOWN:         return TransportErrorCode::ConnectionClosed;

        // Network/host errors
        case ENETUNREACH:       return TransportErrorCode::NetworkUnreachable;
        case EHOSTUNREACH:      return TransportErrorCode::HostUnreachable;
        case ENETDOWN:          return TransportErrorCode::NetworkUnreachable;
#ifdef EHOSTDOWN
        case EHOSTDOWN:         return TransportErrorCode::HostUnreachable;
#endif

        // Timeout
        case ETIMEDOUT:         return TransportErrorCode::Timeout;

        // Non-blocking / async
        case EAGAIN:            return TransportErrorCode::WouldBlock;
#if EAGAIN != EWOULDBLOCK
        case EWOULDBLOCK:       return TransportErrorCode::WouldBlock;
#endif
        case EINPROGRESS:       return TransportErrorCode::InProgress;
        case EALREADY:          return TransportErrorCode::InProgress;

        // Buffer space
        case ENOBUFS:           return TransportErrorCode::NoBufferSpace;
        case ENOMEM:            return TransportErrorCode::NoBufferSpace;

        // Address resolution (getaddrinfo returns EAI_* codes, not errno)
        // These are rare but possible in some contexts
        case EADDRNOTAVAIL:     return TransportErrorCode::ConnectionFailed;
        case EAFNOSUPPORT:      return TransportErrorCode::ConnectionFailed;

        // General socket errors
        case EBADF:             return TransportErrorCode::SocketError;
        case ENOTSOCK:          return TransportErrorCode::SocketError;
        case EINVAL:            return TransportErrorCode::SocketError;
        case EMFILE:            return TransportErrorCode::SocketError;
        case ENFILE:            return TransportErrorCode::SocketError;
        case EACCES:            return TransportErrorCode::SocketError;
        case EFAULT:            return TransportErrorCode::SocketError;
        case EPROTONOSUPPORT:   return TransportErrorCode::SocketError;

        // Interrupted (should typically retry)
        case EINTR:             return TransportErrorCode::SocketError;

        default:                return TransportErrorCode::SocketError;
    }
#endif
}

// ============================================================================
// getaddrinfo Error Mapping
// ============================================================================

/// Map getaddrinfo error code to TransportErrorCode
/// @param gai_error Return value from getaddrinfo()
/// @return Corresponding TransportErrorCode
[[nodiscard]] inline TransportErrorCode map_gai_error(int gai_error) noexcept {
#if NFX_PLATFORM_WINDOWS
    // Windows getaddrinfo uses WSA error codes
    return map_socket_error(gai_error);
#else
    // POSIX getaddrinfo uses EAI_* error codes
    switch (gai_error) {
        case 0:                 return TransportErrorCode::None;
        case EAI_AGAIN:         return TransportErrorCode::AddressResolutionFailed;
        case EAI_BADFLAGS:      return TransportErrorCode::AddressResolutionFailed;
        case EAI_FAIL:          return TransportErrorCode::AddressResolutionFailed;
        case EAI_FAMILY:        return TransportErrorCode::AddressResolutionFailed;
        case EAI_MEMORY:        return TransportErrorCode::NoBufferSpace;
        case EAI_NONAME:        return TransportErrorCode::AddressResolutionFailed;
        case EAI_SERVICE:       return TransportErrorCode::AddressResolutionFailed;
        case EAI_SOCKTYPE:      return TransportErrorCode::AddressResolutionFailed;
#ifdef EAI_SYSTEM
        case EAI_SYSTEM:        return map_socket_error(errno);
#endif
#ifdef EAI_OVERFLOW
        case EAI_OVERFLOW:      return TransportErrorCode::AddressResolutionFailed;
#endif
#ifdef EAI_NODATA
        case EAI_NODATA:        return TransportErrorCode::AddressResolutionFailed;
#endif
#ifdef EAI_ADDRFAMILY
        case EAI_ADDRFAMILY:    return TransportErrorCode::AddressResolutionFailed;
#endif
        default:                return TransportErrorCode::AddressResolutionFailed;
    }
#endif
}

// ============================================================================
// TransportError Factory Functions
// ============================================================================

/// Create TransportError from current OS socket error
[[nodiscard]] inline TransportError make_socket_error() noexcept {
    int os_error = get_last_socket_error();
    return TransportError{map_socket_error(os_error), os_error};
}

/// Create TransportError from specific OS error code
[[nodiscard]] inline TransportError make_socket_error(int os_error) noexcept {
    return TransportError{map_socket_error(os_error), os_error};
}

/// Create TransportError from getaddrinfo result
[[nodiscard]] inline TransportError make_gai_error(int gai_error) noexcept {
    return TransportError{map_gai_error(gai_error), gai_error};
}

/// Create TransportError with explicit code and OS error
[[nodiscard]] inline TransportError make_transport_error(
    TransportErrorCode code,
    int os_error = 0) noexcept
{
    return TransportError{code, os_error};
}

// ============================================================================
// Error Description
// ============================================================================

/// Get human-readable description of OS socket error
/// @param os_error Platform-specific error code
/// @return Error description (platform-specific format)
[[nodiscard]] inline const char* socket_error_string(int os_error) noexcept {
#if NFX_PLATFORM_WINDOWS
    // Windows: Use FormatMessage
    static thread_local char buffer[256] = {0};
    DWORD len = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        static_cast<DWORD>(os_error),
        MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
        buffer,
        sizeof(buffer) - 1,
        nullptr
    );
    if (len == 0) {
        buffer[0] = '\0';
        return "Unknown error";
    }
    buffer[len] = '\0';  // Ensure null-termination
    return buffer;
#else
    // POSIX: Use strerror_r for thread safety
    static thread_local char buffer[256] = {0};
#if NFX_PLATFORM_MACOS || ((_POSIX_C_SOURCE >= 200112L) && !defined(_GNU_SOURCE))
    // XSI-compliant strerror_r (macOS always uses this, Linux uses it when _GNU_SOURCE is not defined)
    if (strerror_r(os_error, buffer, sizeof(buffer)) == 0) {
        return buffer;
    }
    return "Unknown error";
#else
    // GNU strerror_r returns char*
    return strerror_r(os_error, buffer, sizeof(buffer));
#endif
#endif
}

} // namespace nfx
