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
// NewOrderSingleCodec: SBE Flyweight Codec for NewOrderSingle (35=D)
// ============================================================================
//
// Message Layout (64 bytes total: 8 header + 56 body):
//
// Header (8 bytes):
//   Offset 0-7: MessageHeader (blockLength=56, templateId=1)
//
// Body (56 bytes):
//   Offset  0-19: clOrdId      FixedString<20>  Client Order ID
//   Offset 20-27: symbol       FixedString<8>   Instrument symbol
//   Offset    28: side         char             Side enum ('1'=Buy, '2'=Sell)
//   Offset    29: ordType      char             Order type enum
//   Offset 30-31: padding      (2 bytes)        Alignment padding
//   Offset 32-39: price        int64            Price (8 decimals)
//   Offset 40-47: orderQty     int64            Quantity (4 decimals)
//   Offset 48-55: transactTime int64            Timestamp (nanoseconds)
//
// All int64 fields are 8-byte aligned for optimal memory access.

class NewOrderSingleCodec {
public:
    // Message constants
    static constexpr SbeUint16 TEMPLATE_ID = MessageHeader::TemplateId::NewOrderSingle;
    static constexpr std::size_t BLOCK_LENGTH = 56;
    static constexpr std::size_t TOTAL_SIZE = MessageHeader::SIZE + BLOCK_LENGTH;

    // Field offsets within the body (after header)
    struct Offset {
        static constexpr std::size_t ClOrdId = 0;
        static constexpr std::size_t Symbol = 20;
        static constexpr std::size_t Side = 28;
        static constexpr std::size_t OrdType = 29;
        static constexpr std::size_t Padding = 30;
        static constexpr std::size_t Price = 32;
        static constexpr std::size_t OrderQty = 40;
        static constexpr std::size_t TransactTime = 48;
    };

    // Field sizes
    struct Size {
        static constexpr std::size_t ClOrdId = 20;
        static constexpr std::size_t Symbol = 8;
        static constexpr std::size_t Side = 1;
        static constexpr std::size_t OrdType = 1;
        static constexpr std::size_t Padding = 2;
        static constexpr std::size_t Price = 8;
        static constexpr std::size_t OrderQty = 8;
        static constexpr std::size_t TransactTime = 8;
    };

    // ========================================================================
    // Decode API (Zero-Copy Flyweight)
    // ========================================================================

    // Wrap existing buffer for decoding
    [[nodiscard]] NFX_FORCE_INLINE static NewOrderSingleCodec wrapForDecode(
        const char* buffer, std::size_t length) noexcept {
        return NewOrderSingleCodec{buffer, length, false};
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
    [[nodiscard]] NFX_HOT NFX_FORCE_INLINE std::string_view clOrdId() const noexcept {
        return FixedString20::decode(body() + Offset::ClOrdId);
    }

    [[nodiscard]] NFX_HOT NFX_FORCE_INLINE std::string_view symbol() const noexcept {
        return FixedString8::decode(body() + Offset::Symbol);
    }

    [[nodiscard]] NFX_HOT NFX_FORCE_INLINE Side side() const noexcept {
        return SbeSide::decode(body() + Offset::Side);
    }

    [[nodiscard]] NFX_HOT NFX_FORCE_INLINE OrdType ordType() const noexcept {
        return SbeOrdType::decode(body() + Offset::OrdType);
    }

    [[nodiscard]] NFX_HOT NFX_FORCE_INLINE FixedPrice price() const noexcept {
        return DecimalPrice::decode(body() + Offset::Price);
    }

    [[nodiscard]] NFX_HOT NFX_FORCE_INLINE Qty orderQty() const noexcept {
        return DecimalQty::decode(body() + Offset::OrderQty);
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
    [[nodiscard]] NFX_FORCE_INLINE static NewOrderSingleCodec wrapForEncode(
        char* buffer, std::size_t length) noexcept {
        return NewOrderSingleCodec{buffer, length, true};
    }

    // Encode header (call first)
    NFX_FORCE_INLINE NewOrderSingleCodec& encodeHeader() noexcept {
        auto header = MessageHeader::wrapForEncode(mutableBuffer(), length_);
        header.encodeHeader(BLOCK_LENGTH, TEMPLATE_ID);
        // Clear body to ensure clean padding
        std::memset(mutableBody(), 0, BLOCK_LENGTH);
        return *this;
    }

    // Field encoders (fluent interface, return *this)
    NFX_FORCE_INLINE NewOrderSingleCodec& clOrdId(std::string_view value) noexcept {
        FixedString20::encode(mutableBody() + Offset::ClOrdId, value);
        return *this;
    }

    NFX_FORCE_INLINE NewOrderSingleCodec& symbol(std::string_view value) noexcept {
        FixedString8::encode(mutableBody() + Offset::Symbol, value);
        return *this;
    }

    NFX_FORCE_INLINE NewOrderSingleCodec& side(Side value) noexcept {
        SbeSide::encode(mutableBody() + Offset::Side, value);
        return *this;
    }

    NFX_FORCE_INLINE NewOrderSingleCodec& ordType(OrdType value) noexcept {
        SbeOrdType::encode(mutableBody() + Offset::OrdType, value);
        return *this;
    }

    NFX_FORCE_INLINE NewOrderSingleCodec& price(FixedPrice value) noexcept {
        DecimalPrice::encode(mutableBody() + Offset::Price, value);
        return *this;
    }

    NFX_FORCE_INLINE NewOrderSingleCodec& orderQty(Qty value) noexcept {
        DecimalQty::encode(mutableBody() + Offset::OrderQty, value);
        return *this;
    }

    NFX_FORCE_INLINE NewOrderSingleCodec& transactTime(Timestamp value) noexcept {
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
    explicit NewOrderSingleCodec(const char* buffer, std::size_t length,
                                  bool /*forEncode*/) noexcept
        : buffer_{buffer}, length_{length} {}

    const char* buffer_{nullptr};
    std::size_t length_{0};
};

// ============================================================================
// Static Assertions: Verify Layout
// ============================================================================

static_assert(NewOrderSingleCodec::TOTAL_SIZE == 64,
              "NewOrderSingle must be 64 bytes (8 header + 56 body)");

static_assert(NewOrderSingleCodec::Offset::ClOrdId == 0);
static_assert(NewOrderSingleCodec::Offset::Symbol == 20);
static_assert(NewOrderSingleCodec::Offset::Side == 28);
static_assert(NewOrderSingleCodec::Offset::OrdType == 29);
static_assert(NewOrderSingleCodec::Offset::Padding == 30);
static_assert(NewOrderSingleCodec::Offset::Price == 32);
static_assert(NewOrderSingleCodec::Offset::OrderQty == 40);
static_assert(NewOrderSingleCodec::Offset::TransactTime == 48);

// Verify 8-byte alignment for int64 fields
static_assert(NewOrderSingleCodec::Offset::Price % 8 == 0,
              "Price must be 8-byte aligned");
static_assert(NewOrderSingleCodec::Offset::OrderQty % 8 == 0,
              "OrderQty must be 8-byte aligned");
static_assert(NewOrderSingleCodec::Offset::TransactTime % 8 == 0,
              "TransactTime must be 8-byte aligned");

// Verify body size
static_assert(NewOrderSingleCodec::Offset::TransactTime +
              NewOrderSingleCodec::Size::TransactTime ==
              NewOrderSingleCodec::BLOCK_LENGTH,
              "Body layout must match BLOCK_LENGTH");

}  // namespace nfx::sbe
