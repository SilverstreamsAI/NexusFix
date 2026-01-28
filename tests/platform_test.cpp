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
#include <stdexcept>

// Test assertion that works in Release mode (unlike cassert)
#define TEST_ASSERT(cond) \
    do { if (!(cond)) { throw std::runtime_error("Test failed: " #cond); } } while(0)

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
    TEST_ASSERT(platform::is_linux());
    TEST_ASSERT(platform::is_posix());
    TEST_ASSERT(!platform::is_windows());
    TEST_ASSERT(!platform::is_macos());
#elif NFX_PLATFORM_WINDOWS
    TEST_ASSERT(platform::is_windows());
    TEST_ASSERT(!platform::is_linux());
    TEST_ASSERT(!platform::is_macos());
    TEST_ASSERT(!platform::is_posix());
#elif NFX_PLATFORM_MACOS
    TEST_ASSERT(platform::is_macos());
    TEST_ASSERT(platform::is_posix());
    TEST_ASSERT(!platform::is_windows());
    TEST_ASSERT(!platform::is_linux());
#endif

    std::cout << "Platform detection: PASS\n";
}

void test_socket_types() {
    // Test invalid socket constant
    TEST_ASSERT(!is_valid_socket(INVALID_SOCKET_HANDLE));

    // Test socket creation
    SocketHandle sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (is_valid_socket(sock)) {
        TEST_ASSERT(is_valid_socket(sock));

        // Test socket options
        TEST_ASSERT(set_tcp_nodelay(sock, true));
        TEST_ASSERT(set_socket_keepalive(sock, true));
        TEST_ASSERT(set_socket_reuseaddr(sock, true));
        TEST_ASSERT(set_socket_nonblocking(sock, true));
        TEST_ASSERT(set_socket_nonblocking(sock, false));

        close_socket(sock);
    }

    std::cout << "Socket types: PASS\n";
}

void test_error_mapping() {
    // Test POSIX error mapping
#if NFX_PLATFORM_POSIX
    TEST_ASSERT(map_socket_error(ECONNREFUSED) == TransportErrorCode::ConnectionRefused);
    TEST_ASSERT(map_socket_error(ECONNRESET) == TransportErrorCode::ConnectionReset);
    TEST_ASSERT(map_socket_error(ETIMEDOUT) == TransportErrorCode::Timeout);
    TEST_ASSERT(map_socket_error(EAGAIN) == TransportErrorCode::WouldBlock);
    TEST_ASSERT(map_socket_error(EINPROGRESS) == TransportErrorCode::InProgress);
    TEST_ASSERT(map_socket_error(ENETUNREACH) == TransportErrorCode::NetworkUnreachable);
    TEST_ASSERT(map_socket_error(0) == TransportErrorCode::None);
#endif

#if NFX_PLATFORM_WINDOWS
    TEST_ASSERT(map_socket_error(WSAECONNREFUSED) == TransportErrorCode::ConnectionRefused);
    TEST_ASSERT(map_socket_error(WSAECONNRESET) == TransportErrorCode::ConnectionReset);
    TEST_ASSERT(map_socket_error(WSAETIMEDOUT) == TransportErrorCode::Timeout);
    TEST_ASSERT(map_socket_error(WSAEWOULDBLOCK) == TransportErrorCode::WouldBlock);
    TEST_ASSERT(map_socket_error(0) == TransportErrorCode::None);
#endif

    // Test error factory
    auto err = make_transport_error(TransportErrorCode::ConnectionFailed, 42);
    TEST_ASSERT(err.code == TransportErrorCode::ConnectionFailed);
    TEST_ASSERT(err.system_errno == 42);

    std::cout << "Error mapping: PASS\n";
}

void test_tcp_socket() {
    TcpSocket sock;
    TEST_ASSERT(!sock.is_connected());
    TEST_ASSERT(sock.state() == ConnectionState::Disconnected);

    // Create socket
    auto result = sock.create();
    TEST_ASSERT(result.has_value());
    TEST_ASSERT(is_valid_socket(sock.fd()));

    // Test options before connect
    TEST_ASSERT(sock.set_nodelay(true));
    TEST_ASSERT(sock.set_keepalive(true));

    // Close socket
    sock.close();
    TEST_ASSERT(!is_valid_socket(sock.fd()));
    TEST_ASSERT(sock.state() == ConnectionState::Disconnected);

    std::cout << "TCP socket: PASS\n";
}

void test_tcp_transport() {
    TcpTransport transport;
    TEST_ASSERT(!transport.is_connected());

    // Test options before connect
    TEST_ASSERT(transport.set_nodelay(true));
    TEST_ASSERT(transport.set_keepalive(true));

    std::cout << "TCP transport: PASS\n";
}

void test_tcp_acceptor() {
    TcpAcceptor acceptor;
    TEST_ASSERT(!acceptor.is_listening());

    // Listen on ephemeral port
    auto result = acceptor.listen(0);  // Port 0 = let OS choose
    TEST_ASSERT(result.has_value());
    TEST_ASSERT(acceptor.is_listening());

    // Close
    acceptor.close();
    TEST_ASSERT(!acceptor.is_listening());

    std::cout << "TCP acceptor: PASS\n";
}

void test_new_error_codes() {
    // Verify new error codes exist and have messages
    TransportError err;

    err.code = TransportErrorCode::ConnectionRefused;
    TEST_ASSERT(err.message() == "Connection refused");

    err.code = TransportErrorCode::ConnectionReset;
    TEST_ASSERT(err.message() == "Connection reset by peer");

    err.code = TransportErrorCode::NetworkUnreachable;
    TEST_ASSERT(err.message() == "Network unreachable");

    err.code = TransportErrorCode::WouldBlock;
    TEST_ASSERT(err.message() == "Operation would block");

    err.code = TransportErrorCode::WinsockInitFailed;
    TEST_ASSERT(err.message() == "Winsock initialization failed");

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
    TEST_ASSERT(transport != nullptr);
    TEST_ASSERT(!transport->is_connected());

    // Create simple transport
    auto simple = TransportFactory::create_simple();
    TEST_ASSERT(simple != nullptr);

    // Create via convenience function
    auto t1 = make_transport();
    TEST_ASSERT(t1 != nullptr);

    auto t2 = make_simple_transport();
    TEST_ASSERT(t2 != nullptr);

    auto t3 = make_fast_transport();
    TEST_ASSERT(t3 != nullptr);

    // Test platform type aliases
    PlatformSocket sock;
    TEST_ASSERT(!sock.is_connected());

    PlatformTransport pt;
    TEST_ASSERT(!pt.is_connected());

    std::cout << "Transport factory: PASS\n";
}

void test_winsock_init_stub() {
    // On non-Windows, WinsockInit is a no-op stub
    TEST_ASSERT(WinsockInit::initialize());
    TEST_ASSERT(WinsockInit::ensure());
    TEST_ASSERT(WinsockInit::is_initialized());
    TEST_ASSERT(WinsockInit::last_error() == 0);

    std::cout << "Winsock init stub: PASS\n";
}

int main() {
    std::cout << "=== Platform Abstraction Tests ===\n\n";

    // IMPORTANT: On Windows, Winsock must be initialized before any socket API calls
#if NFX_PLATFORM_WINDOWS
    std::cout << "Initializing Winsock...\n";
    if (!WinsockInit::ensure()) {
        std::cerr << "FATAL: Winsock initialization failed, error=" << WinsockInit::last_error() << "\n";
        return 1;
    }
    std::cout << "Winsock initialized.\n\n";
#endif

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
