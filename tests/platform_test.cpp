/// @file platform_test.cpp
/// @brief Tests for cross-platform abstraction layer

#include "nexusfix/platform/platform.hpp"
#include "nexusfix/platform/socket_types.hpp"
#include "nexusfix/platform/error_mapping.hpp"
#include "nexusfix/transport/tcp_transport.hpp"
#include "nexusfix/transport/winsock_init.hpp"
#include "nexusfix/transport/winsock_transport.hpp"
#include "nexusfix/transport/transport_factory.hpp"

#include <iostream>
#include <cassert>

using namespace nfx;

void test_platform_detection() {
    std::cout << "Platform: " << platform::name() << "\n";
    std::cout << "Compiler: " << platform::compiler_name() << "\n";
    std::cout << "Architecture: " << platform::arch_name() << "\n";
    std::cout << "Async I/O: " << platform::async_io_backend() << "\n";

    // At least one platform must be detected
    static_assert(NFX_PLATFORM_LINUX || NFX_PLATFORM_WINDOWS || NFX_PLATFORM_MACOS,
                  "No platform detected");

#if NFX_PLATFORM_LINUX
    assert(platform::is_linux());
    assert(platform::is_posix());
    assert(!platform::is_windows());
    assert(!platform::is_macos());
#elif NFX_PLATFORM_WINDOWS
    assert(platform::is_windows());
    assert(!platform::is_linux());
    assert(!platform::is_macos());
    assert(!platform::is_posix());
#elif NFX_PLATFORM_MACOS
    assert(platform::is_macos());
    assert(platform::is_posix());
    assert(!platform::is_windows());
    assert(!platform::is_linux());
#endif

    std::cout << "Platform detection: PASS\n";
}

void test_socket_types() {
    // Test invalid socket constant
    SocketHandle invalid = INVALID_SOCKET_HANDLE;
    assert(!is_valid_socket(invalid));

    // Test socket creation
    SocketHandle sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (is_valid_socket(sock)) {
        assert(is_valid_socket(sock));

        // Test socket options
        assert(set_tcp_nodelay(sock, true));
        assert(set_socket_keepalive(sock, true));
        assert(set_socket_reuseaddr(sock, true));
        assert(set_socket_nonblocking(sock, true));
        assert(set_socket_nonblocking(sock, false));

        close_socket(sock);
    }

    std::cout << "Socket types: PASS\n";
}

void test_error_mapping() {
    // Test POSIX error mapping
#if NFX_PLATFORM_POSIX
    assert(map_socket_error(ECONNREFUSED) == TransportErrorCode::ConnectionRefused);
    assert(map_socket_error(ECONNRESET) == TransportErrorCode::ConnectionReset);
    assert(map_socket_error(ETIMEDOUT) == TransportErrorCode::Timeout);
    assert(map_socket_error(EAGAIN) == TransportErrorCode::WouldBlock);
    assert(map_socket_error(EINPROGRESS) == TransportErrorCode::InProgress);
    assert(map_socket_error(ENETUNREACH) == TransportErrorCode::NetworkUnreachable);
    assert(map_socket_error(0) == TransportErrorCode::None);
#endif

#if NFX_PLATFORM_WINDOWS
    assert(map_socket_error(WSAECONNREFUSED) == TransportErrorCode::ConnectionRefused);
    assert(map_socket_error(WSAECONNRESET) == TransportErrorCode::ConnectionReset);
    assert(map_socket_error(WSAETIMEDOUT) == TransportErrorCode::Timeout);
    assert(map_socket_error(WSAEWOULDBLOCK) == TransportErrorCode::WouldBlock);
    assert(map_socket_error(0) == TransportErrorCode::None);
#endif

    // Test error factory
    auto err = make_transport_error(TransportErrorCode::ConnectionFailed, 42);
    assert(err.code == TransportErrorCode::ConnectionFailed);
    assert(err.system_errno == 42);

    std::cout << "Error mapping: PASS\n";
}

void test_tcp_socket() {
    TcpSocket sock;
    assert(!sock.is_connected());
    assert(sock.state() == ConnectionState::Disconnected);

    // Create socket
    auto result = sock.create();
    assert(result.has_value());
    assert(is_valid_socket(sock.fd()));

    // Test options before connect
    assert(sock.set_nodelay(true));
    assert(sock.set_keepalive(true));

    // Close socket
    sock.close();
    assert(!is_valid_socket(sock.fd()));
    assert(sock.state() == ConnectionState::Disconnected);

    std::cout << "TCP socket: PASS\n";
}

void test_tcp_transport() {
    TcpTransport transport;
    assert(!transport.is_connected());

    // Test options before connect
    assert(transport.set_nodelay(true));
    assert(transport.set_keepalive(true));

    std::cout << "TCP transport: PASS\n";
}

void test_tcp_acceptor() {
    TcpAcceptor acceptor;
    assert(!acceptor.is_listening());

    // Listen on ephemeral port
    auto result = acceptor.listen(0);  // Port 0 = let OS choose
    assert(result.has_value());
    assert(acceptor.is_listening());

    // Close
    acceptor.close();
    assert(!acceptor.is_listening());

    std::cout << "TCP acceptor: PASS\n";
}

void test_new_error_codes() {
    // Verify new error codes exist and have messages
    TransportError err;

    err.code = TransportErrorCode::ConnectionRefused;
    assert(err.message() == "Connection refused");

    err.code = TransportErrorCode::ConnectionReset;
    assert(err.message() == "Connection reset by peer");

    err.code = TransportErrorCode::NetworkUnreachable;
    assert(err.message() == "Network unreachable");

    err.code = TransportErrorCode::WouldBlock;
    assert(err.message() == "Operation would block");

    err.code = TransportErrorCode::WinsockInitFailed;
    assert(err.message() == "Winsock initialization failed");

    std::cout << "New error codes: PASS\n";
}

void test_transport_factory() {
    // Test factory info functions
    std::cout << "Transport factory:\n";
    std::cout << "  Platform: " << TransportFactory::platform_name() << "\n";
    std::cout << "  Async backend: " << TransportFactory::async_backend_name() << "\n";
    std::cout << "  Default transport: " << TransportFactory::default_transport_name() << "\n";
    std::cout << "  Has async I/O: " << (TransportFactory::has_async_io() ? "yes" : "no") << "\n";
    std::cout << "  Has io_uring: " << (TransportFactory::has_io_uring() ? "yes" : "no") << "\n";
    std::cout << "  Has IOCP: " << (TransportFactory::has_iocp() ? "yes" : "no") << "\n";
    std::cout << "  Has kqueue: " << (TransportFactory::has_kqueue() ? "yes" : "no") << "\n";

    // Create default transport
    auto transport = TransportFactory::create();
    assert(transport != nullptr);
    assert(!transport->is_connected());

    // Create simple transport
    auto simple = TransportFactory::create_simple();
    assert(simple != nullptr);

    // Create via convenience function
    auto t1 = make_transport();
    assert(t1 != nullptr);

    auto t2 = make_simple_transport();
    assert(t2 != nullptr);

    auto t3 = make_fast_transport();
    assert(t3 != nullptr);

    // Test platform type aliases
    PlatformSocket sock;
    assert(!sock.is_connected());

    PlatformTransport pt;
    assert(!pt.is_connected());

    std::cout << "Transport factory: PASS\n";
}

void test_winsock_init_stub() {
    // On non-Windows, WinsockInit is a no-op stub
    assert(WinsockInit::initialize());
    assert(WinsockInit::ensure());
    assert(WinsockInit::is_initialized());
    assert(WinsockInit::last_error() == 0);

    std::cout << "Winsock init stub: PASS\n";
}

int main() {
    std::cout << "=== Platform Abstraction Tests ===\n\n";

    test_platform_detection();
    test_socket_types();
    test_error_mapping();
    test_tcp_socket();
    test_tcp_transport();
    test_tcp_acceptor();
    test_new_error_codes();
    test_transport_factory();
    test_winsock_init_stub();

    std::cout << "\n=== All tests passed ===\n";
    return 0;
}
