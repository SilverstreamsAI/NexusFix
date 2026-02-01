// ============================================================================
// TICKET_023: Error Message Mapping Benchmark
// Compare: Switch-based vs Compile-time Lookup Table
// ============================================================================

#include <iostream>
#include <chrono>
#include <random>
#include <array>
#include <cstdint>
#include <string_view>

// Include the new implementation
#include "nexusfix/types/error.hpp"

// ============================================================================
// OLD Implementation (switch-based) - for comparison
// ============================================================================

namespace old_impl {

[[nodiscard]] inline std::string_view parse_error_message(nfx::ParseErrorCode code) noexcept {
    switch (code) {
        case nfx::ParseErrorCode::None:               return "No error";
        case nfx::ParseErrorCode::BufferTooShort:     return "Buffer too short";
        case nfx::ParseErrorCode::InvalidBeginString: return "Invalid BeginString";
        case nfx::ParseErrorCode::InvalidBodyLength:  return "Invalid BodyLength";
        case nfx::ParseErrorCode::InvalidChecksum:    return "Invalid CheckSum";
        case nfx::ParseErrorCode::MissingRequiredField: return "Missing required field";
        case nfx::ParseErrorCode::InvalidFieldFormat: return "Invalid field format";
        case nfx::ParseErrorCode::InvalidTagNumber:   return "Invalid tag number";
        case nfx::ParseErrorCode::DuplicateTag:       return "Duplicate tag";
        case nfx::ParseErrorCode::UnterminatedField:  return "Unterminated field";
        case nfx::ParseErrorCode::InvalidMsgType:     return "Invalid MsgType";
        case nfx::ParseErrorCode::GarbledMessage:     return "Garbled message";
    }
    return "Unknown error";
}

[[nodiscard]] inline std::string_view session_error_message(nfx::SessionErrorCode code) noexcept {
    switch (code) {
        case nfx::SessionErrorCode::None:            return "No error";
        case nfx::SessionErrorCode::NotConnected:    return "Not connected";
        case nfx::SessionErrorCode::AlreadyConnected: return "Already connected";
        case nfx::SessionErrorCode::LogonRejected:   return "Logon rejected";
        case nfx::SessionErrorCode::LogonTimeout:    return "Logon timeout";
        case nfx::SessionErrorCode::HeartbeatTimeout: return "Heartbeat timeout";
        case nfx::SessionErrorCode::SequenceGap:     return "Sequence gap detected";
        case nfx::SessionErrorCode::InvalidState:    return "Invalid session state";
        case nfx::SessionErrorCode::Disconnected:    return "Disconnected";
    }
    return "Unknown error";
}

[[nodiscard]] inline std::string_view transport_error_message(nfx::TransportErrorCode code) noexcept {
    switch (code) {
        case nfx::TransportErrorCode::None:           return "No error";
        case nfx::TransportErrorCode::ConnectionFailed: return "Connection failed";
        case nfx::TransportErrorCode::ConnectionClosed: return "Connection closed";
        case nfx::TransportErrorCode::ConnectionRefused: return "Connection refused";
        case nfx::TransportErrorCode::ConnectionReset: return "Connection reset by peer";
        case nfx::TransportErrorCode::ConnectionAborted: return "Connection aborted";
        case nfx::TransportErrorCode::ReadError:      return "Read error";
        case nfx::TransportErrorCode::WriteError:     return "Write error";
        case nfx::TransportErrorCode::Timeout:        return "Timeout";
        case nfx::TransportErrorCode::AddressResolutionFailed: return "Address resolution failed";
        case nfx::TransportErrorCode::NetworkUnreachable: return "Network unreachable";
        case nfx::TransportErrorCode::HostUnreachable: return "Host unreachable";
        case nfx::TransportErrorCode::SocketError:    return "Socket error";
        case nfx::TransportErrorCode::WouldBlock:     return "Operation would block";
        case nfx::TransportErrorCode::InProgress:     return "Operation in progress";
        case nfx::TransportErrorCode::NotConnected:   return "Socket not connected";
        case nfx::TransportErrorCode::NoBufferSpace:  return "No buffer space available";
        case nfx::TransportErrorCode::WinsockInitFailed: return "Winsock initialization failed";
        case nfx::TransportErrorCode::IocpError:      return "IOCP operation failed";
        case nfx::TransportErrorCode::KqueueError:    return "kqueue operation failed";
    }
    return "Unknown error";
}

[[nodiscard]] inline std::string_view validation_error_message(nfx::ValidationErrorCode code) noexcept {
    switch (code) {
        case nfx::ValidationErrorCode::None:           return "No error";
        case nfx::ValidationErrorCode::InvalidPrice:   return "Invalid price";
        case nfx::ValidationErrorCode::InvalidQuantity: return "Invalid quantity";
        case nfx::ValidationErrorCode::InvalidSide:    return "Invalid side";
        case nfx::ValidationErrorCode::InvalidOrderType: return "Invalid order type";
        case nfx::ValidationErrorCode::InvalidTimeInForce: return "Invalid time in force";
        case nfx::ValidationErrorCode::InvalidSymbol:  return "Invalid symbol";
        case nfx::ValidationErrorCode::PriceOutOfRange: return "Price out of range";
        case nfx::ValidationErrorCode::QuantityOutOfRange: return "Quantity out of range";
    }
    return "Unknown error";
}

} // namespace old_impl

// ============================================================================
// Benchmark utilities
// ============================================================================

inline uint64_t rdtsc() {
    uint64_t lo, hi;
    asm volatile ("lfence; rdtsc; lfence" : "=a"(lo), "=d"(hi));
    return (hi << 32) | lo;
}

template<typename T>
inline void do_not_optimize(T&& value) {
    asm volatile("" : : "r,m"(value) : "memory");
}

// ============================================================================
// Test data
// ============================================================================

constexpr std::array<nfx::ParseErrorCode, 12> ALL_PARSE_ERRORS = {
    nfx::ParseErrorCode::None,
    nfx::ParseErrorCode::BufferTooShort,
    nfx::ParseErrorCode::InvalidBeginString,
    nfx::ParseErrorCode::InvalidBodyLength,
    nfx::ParseErrorCode::InvalidChecksum,
    nfx::ParseErrorCode::MissingRequiredField,
    nfx::ParseErrorCode::InvalidFieldFormat,
    nfx::ParseErrorCode::InvalidTagNumber,
    nfx::ParseErrorCode::DuplicateTag,
    nfx::ParseErrorCode::UnterminatedField,
    nfx::ParseErrorCode::InvalidMsgType,
    nfx::ParseErrorCode::GarbledMessage
};

constexpr std::array<nfx::SessionErrorCode, 9> ALL_SESSION_ERRORS = {
    nfx::SessionErrorCode::None,
    nfx::SessionErrorCode::NotConnected,
    nfx::SessionErrorCode::AlreadyConnected,
    nfx::SessionErrorCode::LogonRejected,
    nfx::SessionErrorCode::LogonTimeout,
    nfx::SessionErrorCode::HeartbeatTimeout,
    nfx::SessionErrorCode::SequenceGap,
    nfx::SessionErrorCode::InvalidState,
    nfx::SessionErrorCode::Disconnected
};

constexpr std::array<nfx::TransportErrorCode, 20> ALL_TRANSPORT_ERRORS = {
    nfx::TransportErrorCode::None,
    nfx::TransportErrorCode::ConnectionFailed,
    nfx::TransportErrorCode::ConnectionClosed,
    nfx::TransportErrorCode::ConnectionRefused,
    nfx::TransportErrorCode::ConnectionReset,
    nfx::TransportErrorCode::ConnectionAborted,
    nfx::TransportErrorCode::ReadError,
    nfx::TransportErrorCode::WriteError,
    nfx::TransportErrorCode::Timeout,
    nfx::TransportErrorCode::AddressResolutionFailed,
    nfx::TransportErrorCode::NetworkUnreachable,
    nfx::TransportErrorCode::HostUnreachable,
    nfx::TransportErrorCode::SocketError,
    nfx::TransportErrorCode::WouldBlock,
    nfx::TransportErrorCode::InProgress,
    nfx::TransportErrorCode::NotConnected,
    nfx::TransportErrorCode::NoBufferSpace,
    nfx::TransportErrorCode::WinsockInitFailed,
    nfx::TransportErrorCode::IocpError,
    nfx::TransportErrorCode::KqueueError
};

constexpr std::array<nfx::ValidationErrorCode, 9> ALL_VALIDATION_ERRORS = {
    nfx::ValidationErrorCode::None,
    nfx::ValidationErrorCode::InvalidPrice,
    nfx::ValidationErrorCode::InvalidQuantity,
    nfx::ValidationErrorCode::InvalidSide,
    nfx::ValidationErrorCode::InvalidOrderType,
    nfx::ValidationErrorCode::InvalidTimeInForce,
    nfx::ValidationErrorCode::InvalidSymbol,
    nfx::ValidationErrorCode::PriceOutOfRange,
    nfx::ValidationErrorCode::QuantityOutOfRange
};

// ============================================================================
// Benchmark
// ============================================================================

int main() {
    std::cout << "============================================================\n";
    std::cout << "TICKET_023: Error Message Mapping Benchmark\n";
    std::cout << "============================================================\n\n";

    constexpr int ITERATIONS = 10'000'000;
    constexpr int WARMUP = 100'000;

    // ========================================================================
    // Benchmark 1: ParseError message()
    // ========================================================================

    std::cout << "--- ParseError (12 codes, " << ITERATIONS << " iterations) ---\n\n";

    // Warmup
    for (int i = 0; i < WARMUP; ++i) {
        for (auto code : ALL_PARSE_ERRORS) {
            do_not_optimize(old_impl::parse_error_message(code));
            do_not_optimize(nfx::parse_error_message(code));
        }
    }

    // OLD: Switch-based
    uint64_t old_parse_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto code : ALL_PARSE_ERRORS) {
            do_not_optimize(old_impl::parse_error_message(code));
        }
    }
    uint64_t old_parse_cycles = rdtsc() - old_parse_start;

    // NEW: Lookup table
    uint64_t new_parse_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto code : ALL_PARSE_ERRORS) {
            do_not_optimize(nfx::parse_error_message(code));
        }
    }
    uint64_t new_parse_cycles = rdtsc() - new_parse_start;

    double parse_ops = static_cast<double>(ITERATIONS) * ALL_PARSE_ERRORS.size();
    double old_parse_cpop = static_cast<double>(old_parse_cycles) / parse_ops;
    double new_parse_cpop = static_cast<double>(new_parse_cycles) / parse_ops;
    double parse_improvement = (old_parse_cpop - new_parse_cpop) / old_parse_cpop * 100;

    std::cout << "  OLD (switch):     " << old_parse_cpop << " cycles/op\n";
    std::cout << "  NEW (lookup):     " << new_parse_cpop << " cycles/op\n";
    std::cout << "  Improvement:      " << parse_improvement << "%\n\n";

    // ========================================================================
    // Benchmark 2: SessionError message()
    // ========================================================================

    std::cout << "--- SessionError (9 codes, " << ITERATIONS << " iterations) ---\n\n";

    // OLD: Switch-based
    uint64_t old_session_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto code : ALL_SESSION_ERRORS) {
            do_not_optimize(old_impl::session_error_message(code));
        }
    }
    uint64_t old_session_cycles = rdtsc() - old_session_start;

    // NEW: Lookup table
    uint64_t new_session_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto code : ALL_SESSION_ERRORS) {
            do_not_optimize(nfx::session_error_message(code));
        }
    }
    uint64_t new_session_cycles = rdtsc() - new_session_start;

    double session_ops = static_cast<double>(ITERATIONS) * ALL_SESSION_ERRORS.size();
    double old_session_cpop = static_cast<double>(old_session_cycles) / session_ops;
    double new_session_cpop = static_cast<double>(new_session_cycles) / session_ops;
    double session_improvement = (old_session_cpop - new_session_cpop) / old_session_cpop * 100;

    std::cout << "  OLD (switch):     " << old_session_cpop << " cycles/op\n";
    std::cout << "  NEW (lookup):     " << new_session_cpop << " cycles/op\n";
    std::cout << "  Improvement:      " << session_improvement << "%\n\n";

    // ========================================================================
    // Benchmark 3: TransportError message() - largest switch (20 cases)
    // ========================================================================

    std::cout << "--- TransportError (20 codes, " << ITERATIONS << " iterations) ---\n\n";

    // OLD: Switch-based
    uint64_t old_transport_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto code : ALL_TRANSPORT_ERRORS) {
            do_not_optimize(old_impl::transport_error_message(code));
        }
    }
    uint64_t old_transport_cycles = rdtsc() - old_transport_start;

    // NEW: Lookup table
    uint64_t new_transport_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto code : ALL_TRANSPORT_ERRORS) {
            do_not_optimize(nfx::transport_error_message(code));
        }
    }
    uint64_t new_transport_cycles = rdtsc() - new_transport_start;

    double transport_ops = static_cast<double>(ITERATIONS) * ALL_TRANSPORT_ERRORS.size();
    double old_transport_cpop = static_cast<double>(old_transport_cycles) / transport_ops;
    double new_transport_cpop = static_cast<double>(new_transport_cycles) / transport_ops;
    double transport_improvement = (old_transport_cpop - new_transport_cpop) / old_transport_cpop * 100;

    std::cout << "  OLD (switch):     " << old_transport_cpop << " cycles/op\n";
    std::cout << "  NEW (lookup):     " << new_transport_cpop << " cycles/op\n";
    std::cout << "  Improvement:      " << transport_improvement << "%\n\n";

    // ========================================================================
    // Benchmark 4: ValidationError message()
    // ========================================================================

    std::cout << "--- ValidationError (9 codes, " << ITERATIONS << " iterations) ---\n\n";

    // OLD: Switch-based
    uint64_t old_validation_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto code : ALL_VALIDATION_ERRORS) {
            do_not_optimize(old_impl::validation_error_message(code));
        }
    }
    uint64_t old_validation_cycles = rdtsc() - old_validation_start;

    // NEW: Lookup table
    uint64_t new_validation_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto code : ALL_VALIDATION_ERRORS) {
            do_not_optimize(nfx::validation_error_message(code));
        }
    }
    uint64_t new_validation_cycles = rdtsc() - new_validation_start;

    double validation_ops = static_cast<double>(ITERATIONS) * ALL_VALIDATION_ERRORS.size();
    double old_validation_cpop = static_cast<double>(old_validation_cycles) / validation_ops;
    double new_validation_cpop = static_cast<double>(new_validation_cycles) / validation_ops;
    double validation_improvement = (old_validation_cpop - new_validation_cpop) / old_validation_cpop * 100;

    std::cout << "  OLD (switch):     " << old_validation_cpop << " cycles/op\n";
    std::cout << "  NEW (lookup):     " << new_validation_cpop << " cycles/op\n";
    std::cout << "  Improvement:      " << validation_improvement << "%\n\n";

    // ========================================================================
    // Benchmark 5: Random access pattern (cache pressure)
    // ========================================================================

    std::cout << "--- Random Access Pattern (TransportError, " << ITERATIONS << " iterations) ---\n\n";

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, ALL_TRANSPORT_ERRORS.size() - 1);
    std::array<nfx::TransportErrorCode, 1024> random_codes;
    for (auto& c : random_codes) {
        c = ALL_TRANSPORT_ERRORS[dist(rng)];
    }

    // OLD: Random access
    uint64_t old_rand_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto code : random_codes) {
            do_not_optimize(old_impl::transport_error_message(code));
        }
    }
    uint64_t old_rand_cycles = rdtsc() - old_rand_start;

    // NEW: Random access
    uint64_t new_rand_start = rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        for (auto code : random_codes) {
            do_not_optimize(nfx::transport_error_message(code));
        }
    }
    uint64_t new_rand_cycles = rdtsc() - new_rand_start;

    double rand_ops = static_cast<double>(ITERATIONS) * random_codes.size();
    double old_rand_cpop = static_cast<double>(old_rand_cycles) / rand_ops;
    double new_rand_cpop = static_cast<double>(new_rand_cycles) / rand_ops;
    double rand_improvement = (old_rand_cpop - new_rand_cpop) / old_rand_cpop * 100;

    std::cout << "  OLD (switch):     " << old_rand_cpop << " cycles/op\n";
    std::cout << "  NEW (lookup):     " << new_rand_cpop << " cycles/op\n";
    std::cout << "  Improvement:      " << rand_improvement << "%\n\n";

    // ========================================================================
    // Summary
    // ========================================================================

    double avg_improvement = (parse_improvement + session_improvement +
                              transport_improvement + validation_improvement) / 4.0;

    std::cout << "============================================================\n";
    std::cout << "SUMMARY\n";
    std::cout << "============================================================\n\n";

    std::cout << "| Error Type       | OLD (cycles) | NEW (cycles) | Improvement |\n";
    std::cout << "|------------------|--------------|--------------|-------------|\n";
    std::cout << "| ParseError       | " << old_parse_cpop << "       | " << new_parse_cpop << "       | " << parse_improvement << "% |\n";
    std::cout << "| SessionError     | " << old_session_cpop << "       | " << new_session_cpop << "       | " << session_improvement << "% |\n";
    std::cout << "| TransportError   | " << old_transport_cpop << "       | " << new_transport_cpop << "       | " << transport_improvement << "% |\n";
    std::cout << "| ValidationError  | " << old_validation_cpop << "       | " << new_validation_cpop << "       | " << validation_improvement << "% |\n";
    std::cout << "| Random Access    | " << old_rand_cpop << "       | " << new_rand_cpop << "       | " << rand_improvement << "% |\n";
    std::cout << "|------------------|--------------|--------------|-------------|\n";
    std::cout << "| Average          |              |              | " << avg_improvement << "% |\n";

    std::cout << "\nTotal switch cases eliminated: 50 (12 + 9 + 20 + 9)\n";

    return 0;
}
