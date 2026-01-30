// SPDX-License-Identifier: MIT
// Copyright (c) 2025 SilverstreamsAI

#pragma once

// ============================================================================
// NexusFix SBE (Simple Binary Encoding) Module
// ============================================================================
//
// High-performance binary encoding for internal IPC communication.
// Target: ~5ns decode latency (40x faster than FIX text parsing).
//
// Message Types:
//   - NewOrderSingle (templateId=1, 64 bytes)
//   - ExecutionReport (templateId=8, 144 bytes)
//
// Usage (Decode):
//   auto msg = sbe::ExecutionReportCodec::wrapForDecode(buffer, length);
//   if (msg.isValid()) {
//       auto symbol = msg.symbol();  // ~1-2ns, direct offset read
//       auto price = msg.price();    // Returns FixedPrice
//   }
//
// Usage (Encode):
//   alignas(8) char buffer[sbe::NewOrderSingleCodec::TOTAL_SIZE];
//   auto msg = sbe::NewOrderSingleCodec::wrapForEncode(buffer, sizeof(buffer))
//       .encodeHeader()
//       .clOrdId("ORDER123")
//       .symbol("AAPL")
//       .side(Side::Buy)
//       .price(FixedPrice::from_double(150.50));
//
// Usage (Dispatch):
//   sbe::dispatch(buffer, length, [](auto& codec) {
//       // codec is NewOrderSingleCodec or ExecutionReportCodec
//   });

#include "nexusfix/sbe/message_header.hpp"
#include "nexusfix/sbe/types/sbe_types.hpp"
#include "nexusfix/sbe/types/composite_types.hpp"
#include "nexusfix/sbe/codecs/new_order_single.hpp"
#include "nexusfix/sbe/codecs/execution_report.hpp"

namespace nfx::sbe {

// ============================================================================
// Message Dispatch by Template ID
// ============================================================================

// Dispatch result for unknown message types
struct UnknownMessage {
    SbeUint16 templateId;
    const char* buffer;
    std::size_t length;
};

// Dispatch to appropriate codec based on template ID
// Handler signature: void(auto& codec) where codec is one of:
//   - NewOrderSingleCodec
//   - ExecutionReportCodec
//   - UnknownMessage
template <typename Handler>
NFX_HOT NFX_FORCE_INLINE void dispatch(
    const char* buffer, std::size_t length, Handler&& handler) noexcept {
    if (length < MessageHeader::SIZE) {
        UnknownMessage unknown{0, buffer, length};
        handler(unknown);
        return;
    }

    auto header = MessageHeader::wrapForDecode(buffer, length);
    if (!header.isValid()) {
        UnknownMessage unknown{0, buffer, length};
        handler(unknown);
        return;
    }

    const SbeUint16 templateId = header.templateId();

    switch (templateId) {
        case MessageHeader::TemplateId::NewOrderSingle: {
            auto codec = NewOrderSingleCodec::wrapForDecode(buffer, length);
            handler(codec);
            break;
        }
        case MessageHeader::TemplateId::ExecutionReport: {
            auto codec = ExecutionReportCodec::wrapForDecode(buffer, length);
            handler(codec);
            break;
        }
        default: {
            UnknownMessage unknown{templateId, buffer, length};
            handler(unknown);
            break;
        }
    }
}

// Overload for std::span input
template <typename Handler>
NFX_FORCE_INLINE void dispatch(
    std::span<const char> buffer, Handler&& handler) noexcept {
    dispatch(buffer.data(), buffer.size(), std::forward<Handler>(handler));
}

// ============================================================================
// Codec Type Traits
// ============================================================================

template <typename T>
struct is_sbe_codec : std::false_type {};

template <>
struct is_sbe_codec<NewOrderSingleCodec> : std::true_type {};

template <>
struct is_sbe_codec<ExecutionReportCodec> : std::true_type {};

template <typename T>
inline constexpr bool is_sbe_codec_v = is_sbe_codec<T>::value;

// Concept for SBE codecs
template <typename T>
concept SbeCodec = is_sbe_codec_v<T>;

// ============================================================================
// Buffer Size Helpers
// ============================================================================

// Get required buffer size for a message type
template <SbeCodec T>
[[nodiscard]] constexpr std::size_t required_buffer_size() noexcept {
    return T::TOTAL_SIZE;
}

// Maximum message size across all supported types
inline constexpr std::size_t MAX_MESSAGE_SIZE =
    std::max(NewOrderSingleCodec::TOTAL_SIZE,
             ExecutionReportCodec::TOTAL_SIZE);

}  // namespace nfx::sbe
