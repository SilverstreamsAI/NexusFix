// SPDX-License-Identifier: MIT
// Copyright (c) 2025 SilverstreamsAI

#pragma once

#include <cstddef>
#include <cstring>
#include <span>
#include <string_view>

#include "nexusfix/platform/platform.hpp"
#include "nexusfix/sbe/message_header.hpp"
#include "nexusfix/sbe/types/composite_types.hpp"
#include "nexusfix/sbe/types/sbe_types.hpp"

namespace nfx::sbe {

// ============================================================================
// ExecutionReportCodec: SBE Flyweight Codec for ExecutionReport (35=8)
// ============================================================================
//
// Message Layout (144 bytes total: 8 header + 136 body):
//
// Header (8 bytes):
//   Offset 0-7: MessageHeader (blockLength=136, templateId=8)
//
// Body (136 bytes):
//   Offset   0-19: orderId      FixedString<20>  Exchange Order ID
//   Offset  20-39: execId       FixedString<20>  Execution ID
//   Offset  40-59: clOrdId      FixedString<20>  Client Order ID
//   Offset  60-67: symbol       FixedString<8>   Instrument symbol
//   Offset     68: side         char             Side enum
//   Offset     69: execType     char             Execution type enum
//   Offset     70: ordStatus    char             Order status enum
//   Offset     71: padding      (1 byte)         Alignment padding
//   Offset  72-79: price        int64            Order price (8 decimals)
//   Offset  80-87: orderQty     int64            Order quantity (4 decimals)
//   Offset  88-95: lastPx       int64            Last fill price (8 decimals)
//   Offset  96-103: lastQty     int64            Last fill qty (4 decimals)
//   Offset 104-111: leavesQty   int64            Remaining qty (4 decimals)
//   Offset 112-119: cumQty      int64            Cumulative qty (4 decimals)
//   Offset 120-127: avgPx       int64            Average price (8 decimals)
//   Offset 128-135: transactTime int64           Timestamp (nanoseconds)
//
// All int64 fields are 8-byte aligned for optimal memory access.

class ExecutionReportCodec {
public:
    // Message constants
    static constexpr SbeUint16 TEMPLATE_ID = MessageHeader::TemplateId::ExecutionReport;
    static constexpr std::size_t BLOCK_LENGTH = 136;
    static constexpr std::size_t TOTAL_SIZE = MessageHeader::SIZE + BLOCK_LENGTH;

    // Field offsets within the body (after header)
    struct Offset {
        static constexpr std::size_t OrderId = 0;
        static constexpr std::size_t ExecId = 20;
        static constexpr std::size_t ClOrdId = 40;
        static constexpr std::size_t Symbol = 60;
        static constexpr std::size_t Side = 68;
        static constexpr std::size_t ExecType = 69;
        static constexpr std::size_t OrdStatus = 70;
        static constexpr std::size_t Padding = 71;
        static constexpr std::size_t Price = 72;
        static constexpr std::size_t OrderQty = 80;
        static constexpr std::size_t LastPx = 88;
        static constexpr std::size_t LastQty = 96;
        static constexpr std::size_t LeavesQty = 104;
        static constexpr std::size_t CumQty = 112;
        static constexpr std::size_t AvgPx = 120;
        static constexpr std::size_t TransactTime = 128;
    };

    // Field sizes
    struct Size {
        static constexpr std::size_t OrderId = 20;
        static constexpr std::size_t ExecId = 20;
        static constexpr std::size_t ClOrdId = 20;
        static constexpr std::size_t Symbol = 8;
        static constexpr std::size_t Side = 1;
        static constexpr std::size_t ExecType = 1;
        static constexpr std::size_t OrdStatus = 1;
        static constexpr std::size_t Padding = 1;
        static constexpr std::size_t Price = 8;
        static constexpr std::size_t OrderQty = 8;
        static constexpr std::size_t LastPx = 8;
        static constexpr std::size_t LastQty = 8;
        static constexpr std::size_t LeavesQty = 8;
        static constexpr std::size_t CumQty = 8;
        static constexpr std::size_t AvgPx = 8;
        static constexpr std::size_t TransactTime = 8;
    };

    // ========================================================================
    // Decode API (Zero-Copy Flyweight)
    // ========================================================================

    // Wrap existing buffer for decoding
    [[nodiscard]] NFX_FORCE_INLINE static ExecutionReportCodec wrapForDecode(
        const char* buffer, std::size_t length) noexcept {
        return ExecutionReportCodec{buffer, length, false};
    }

    // Check if buffer is valid for this message type
    [[nodiscard]] NFX_FORCE_INLINE bool isValid() const noexcept {
        if (buffer_ == nullptr || length_ < TOTAL_SIZE) {
            return false;
        }
        auto header = MessageHeader::wrapForDecode(buffer_, length_);
        return header.isValid() &&
               header.templateId() == TEMPLATE_ID &&
               header.blockLength() == BLOCK_LENGTH;
    }

    // Field accessors (hot path, zero-copy)
    [[nodiscard]] NFX_HOT NFX_FORCE_INLINE std::string_view orderId() const noexcept {
        return FixedString20::decode(body() + Offset::OrderId);
    }

    [[nodiscard]] NFX_HOT NFX_FORCE_INLINE std::string_view execId() const noexcept {
        return FixedString20::decode(body() + Offset::ExecId);
    }

    [[nodiscard]] NFX_HOT NFX_FORCE_INLINE std::string_view clOrdId() const noexcept {
        return FixedString20::decode(body() + Offset::ClOrdId);
    }

    [[nodiscard]] NFX_HOT NFX_FORCE_INLINE std::string_view symbol() const noexcept {
        return FixedString8::decode(body() + Offset::Symbol);
    }

    [[nodiscard]] NFX_HOT NFX_FORCE_INLINE Side side() const noexcept {
        return SbeSide::decode(body() + Offset::Side);
    }

    [[nodiscard]] NFX_HOT NFX_FORCE_INLINE ExecType execType() const noexcept {
        return SbeExecType::decode(body() + Offset::ExecType);
    }

    [[nodiscard]] NFX_HOT NFX_FORCE_INLINE OrdStatus ordStatus() const noexcept {
        return SbeOrdStatus::decode(body() + Offset::OrdStatus);
    }

    [[nodiscard]] NFX_HOT NFX_FORCE_INLINE FixedPrice price() const noexcept {
        return DecimalPrice::decode(body() + Offset::Price);
    }

    [[nodiscard]] NFX_HOT NFX_FORCE_INLINE Qty orderQty() const noexcept {
        return DecimalQty::decode(body() + Offset::OrderQty);
    }

    [[nodiscard]] NFX_HOT NFX_FORCE_INLINE FixedPrice lastPx() const noexcept {
        return DecimalPrice::decode(body() + Offset::LastPx);
    }

    [[nodiscard]] NFX_HOT NFX_FORCE_INLINE Qty lastQty() const noexcept {
        return DecimalQty::decode(body() + Offset::LastQty);
    }

    [[nodiscard]] NFX_HOT NFX_FORCE_INLINE Qty leavesQty() const noexcept {
        return DecimalQty::decode(body() + Offset::LeavesQty);
    }

    [[nodiscard]] NFX_HOT NFX_FORCE_INLINE Qty cumQty() const noexcept {
        return DecimalQty::decode(body() + Offset::CumQty);
    }

    [[nodiscard]] NFX_HOT NFX_FORCE_INLINE FixedPrice avgPx() const noexcept {
        return DecimalPrice::decode(body() + Offset::AvgPx);
    }

    [[nodiscard]] NFX_HOT NFX_FORCE_INLINE Timestamp transactTime() const noexcept {
        return SbeTimestamp::decode(body() + Offset::TransactTime);
    }

    // Get pointer to message body
    [[nodiscard]] NFX_FORCE_INLINE const char* body() const noexcept {
        return buffer_ + MessageHeader::SIZE;
    }

    // Get header
    [[nodiscard]] NFX_FORCE_INLINE MessageHeader header() const noexcept {
        return MessageHeader::wrapForDecode(buffer_, length_);
    }

    // ========================================================================
    // Encode API (Fluent Builder)
    // ========================================================================

    // Wrap buffer for encoding
    [[nodiscard]] NFX_FORCE_INLINE static ExecutionReportCodec wrapForEncode(
        char* buffer, std::size_t length) noexcept {
        return ExecutionReportCodec{buffer, length, true};
    }

    // Encode header (call first)
    NFX_FORCE_INLINE ExecutionReportCodec& encodeHeader() noexcept {
        auto header = MessageHeader::wrapForEncode(mutableBuffer(), length_);
        header.encodeHeader(BLOCK_LENGTH, TEMPLATE_ID);
        // Clear body to ensure clean padding
        std::memset(mutableBody(), 0, BLOCK_LENGTH);
        return *this;
    }

    // Field encoders (fluent interface, return *this)
    NFX_FORCE_INLINE ExecutionReportCodec& orderId(std::string_view value) noexcept {
        FixedString20::encode(mutableBody() + Offset::OrderId, value);
        return *this;
    }

    NFX_FORCE_INLINE ExecutionReportCodec& execId(std::string_view value) noexcept {
        FixedString20::encode(mutableBody() + Offset::ExecId, value);
        return *this;
    }

    NFX_FORCE_INLINE ExecutionReportCodec& clOrdId(std::string_view value) noexcept {
        FixedString20::encode(mutableBody() + Offset::ClOrdId, value);
        return *this;
    }

    NFX_FORCE_INLINE ExecutionReportCodec& symbol(std::string_view value) noexcept {
        FixedString8::encode(mutableBody() + Offset::Symbol, value);
        return *this;
    }

    NFX_FORCE_INLINE ExecutionReportCodec& side(Side value) noexcept {
        SbeSide::encode(mutableBody() + Offset::Side, value);
        return *this;
    }

    NFX_FORCE_INLINE ExecutionReportCodec& execType(ExecType value) noexcept {
        SbeExecType::encode(mutableBody() + Offset::ExecType, value);
        return *this;
    }

    NFX_FORCE_INLINE ExecutionReportCodec& ordStatus(OrdStatus value) noexcept {
        SbeOrdStatus::encode(mutableBody() + Offset::OrdStatus, value);
        return *this;
    }

    NFX_FORCE_INLINE ExecutionReportCodec& price(FixedPrice value) noexcept {
        DecimalPrice::encode(mutableBody() + Offset::Price, value);
        return *this;
    }

    NFX_FORCE_INLINE ExecutionReportCodec& orderQty(Qty value) noexcept {
        DecimalQty::encode(mutableBody() + Offset::OrderQty, value);
        return *this;
    }

    NFX_FORCE_INLINE ExecutionReportCodec& lastPx(FixedPrice value) noexcept {
        DecimalPrice::encode(mutableBody() + Offset::LastPx, value);
        return *this;
    }

    NFX_FORCE_INLINE ExecutionReportCodec& lastQty(Qty value) noexcept {
        DecimalQty::encode(mutableBody() + Offset::LastQty, value);
        return *this;
    }

    NFX_FORCE_INLINE ExecutionReportCodec& leavesQty(Qty value) noexcept {
        DecimalQty::encode(mutableBody() + Offset::LeavesQty, value);
        return *this;
    }

    NFX_FORCE_INLINE ExecutionReportCodec& cumQty(Qty value) noexcept {
        DecimalQty::encode(mutableBody() + Offset::CumQty, value);
        return *this;
    }

    NFX_FORCE_INLINE ExecutionReportCodec& avgPx(FixedPrice value) noexcept {
        DecimalPrice::encode(mutableBody() + Offset::AvgPx, value);
        return *this;
    }

    NFX_FORCE_INLINE ExecutionReportCodec& transactTime(Timestamp value) noexcept {
        SbeTimestamp::encode(mutableBody() + Offset::TransactTime, value);
        return *this;
    }

    // Get encoded message as span
    [[nodiscard]] NFX_FORCE_INLINE std::span<const char> encoded() const noexcept {
        return std::span<const char>{buffer_, TOTAL_SIZE};
    }

    // Get total encoded size
    [[nodiscard]] static constexpr std::size_t encodedSize() noexcept {
        return TOTAL_SIZE;
    }

    // Get mutable body pointer
    [[nodiscard]] NFX_FORCE_INLINE char* mutableBody() noexcept {
        return mutableBuffer() + MessageHeader::SIZE;
    }

    // Get mutable buffer pointer
    [[nodiscard]] NFX_FORCE_INLINE char* mutableBuffer() noexcept {
        return const_cast<char*>(buffer_);
    }

private:
    explicit ExecutionReportCodec(const char* buffer, std::size_t length,
                                   bool /*forEncode*/) noexcept
        : buffer_{buffer}, length_{length} {}

    const char* buffer_{nullptr};
    std::size_t length_{0};
};

// ============================================================================
// Static Assertions: Verify Layout
// ============================================================================

static_assert(ExecutionReportCodec::TOTAL_SIZE == 144,
              "ExecutionReport must be 144 bytes (8 header + 136 body)");

static_assert(ExecutionReportCodec::Offset::OrderId == 0);
static_assert(ExecutionReportCodec::Offset::ExecId == 20);
static_assert(ExecutionReportCodec::Offset::ClOrdId == 40);
static_assert(ExecutionReportCodec::Offset::Symbol == 60);
static_assert(ExecutionReportCodec::Offset::Side == 68);
static_assert(ExecutionReportCodec::Offset::ExecType == 69);
static_assert(ExecutionReportCodec::Offset::OrdStatus == 70);
static_assert(ExecutionReportCodec::Offset::Padding == 71);
static_assert(ExecutionReportCodec::Offset::Price == 72);
static_assert(ExecutionReportCodec::Offset::OrderQty == 80);
static_assert(ExecutionReportCodec::Offset::LastPx == 88);
static_assert(ExecutionReportCodec::Offset::LastQty == 96);
static_assert(ExecutionReportCodec::Offset::LeavesQty == 104);
static_assert(ExecutionReportCodec::Offset::CumQty == 112);
static_assert(ExecutionReportCodec::Offset::AvgPx == 120);
static_assert(ExecutionReportCodec::Offset::TransactTime == 128);

// Verify 8-byte alignment for int64 fields
static_assert(ExecutionReportCodec::Offset::Price % 8 == 0,
              "Price must be 8-byte aligned");
static_assert(ExecutionReportCodec::Offset::OrderQty % 8 == 0,
              "OrderQty must be 8-byte aligned");
static_assert(ExecutionReportCodec::Offset::LastPx % 8 == 0,
              "LastPx must be 8-byte aligned");
static_assert(ExecutionReportCodec::Offset::LastQty % 8 == 0,
              "LastQty must be 8-byte aligned");
static_assert(ExecutionReportCodec::Offset::LeavesQty % 8 == 0,
              "LeavesQty must be 8-byte aligned");
static_assert(ExecutionReportCodec::Offset::CumQty % 8 == 0,
              "CumQty must be 8-byte aligned");
static_assert(ExecutionReportCodec::Offset::AvgPx % 8 == 0,
              "AvgPx must be 8-byte aligned");
static_assert(ExecutionReportCodec::Offset::TransactTime % 8 == 0,
              "TransactTime must be 8-byte aligned");

// Verify body size
static_assert(ExecutionReportCodec::Offset::TransactTime +
              ExecutionReportCodec::Size::TransactTime ==
              ExecutionReportCodec::BLOCK_LENGTH,
              "Body layout must match BLOCK_LENGTH");

}  // namespace nfx::sbe
