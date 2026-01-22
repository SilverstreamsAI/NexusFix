#pragma once

/// @file nexusfix.hpp
/// @brief Main header for NexusFIX high-performance FIX protocol engine

// Types
#include "nexusfix/types/tag.hpp"
#include "nexusfix/types/field_types.hpp"
#include "nexusfix/types/error.hpp"

// Memory
#include "nexusfix/memory/buffer_pool.hpp"

// Interfaces
#include "nexusfix/interfaces/i_message.hpp"

// Parser
#include "nexusfix/parser/field_view.hpp"
#include "nexusfix/parser/simd_scanner.hpp"
#include "nexusfix/parser/consteval_parser.hpp"
#include "nexusfix/parser/runtime_parser.hpp"

// Messages
#include "nexusfix/messages/common/header.hpp"
#include "nexusfix/messages/common/trailer.hpp"
#include "nexusfix/messages/fix44/logon.hpp"
#include "nexusfix/messages/fix44/heartbeat.hpp"
#include "nexusfix/messages/fix44/execution_report.hpp"
#include "nexusfix/messages/fix44/new_order_single.hpp"

// Session
#include "nexusfix/session/state.hpp"
#include "nexusfix/session/sequence.hpp"
#include "nexusfix/session/coroutine.hpp"
#include "nexusfix/session/session_manager.hpp"

// Transport
#include "nexusfix/transport/socket.hpp"
#include "nexusfix/transport/tcp_transport.hpp"
#include "nexusfix/transport/io_uring_transport.hpp"

namespace nfx {

/// Library version
inline constexpr struct {
    int major = 0;
    int minor = 1;
    int patch = 0;

    [[nodiscard]] constexpr const char* string() const noexcept {
        return "0.1.0";
    }
} VERSION;

} // namespace nfx
