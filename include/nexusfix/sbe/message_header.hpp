// SPDX-License-Identifier: MIT
// Copyright (c) 2025 SilverstreamsAI

#pragma once

#include <cstddef>
#include <span>

#include "nexusfix/platform/platform.hpp"
#include "nexusfix/sbe/types/sbe_types.hpp"

namespace nfx::sbe {

// ============================================================================
// SBE Message Header (8 bytes)
// ============================================================================
// Standard SBE header layout:
//   Offset 0: blockLength (uint16) - Size of the message body in bytes
//   Offset 2: templateId  (uint16) - Message type identifier
//   Offset 4: schemaId    (uint16) - Schema identifier
//   Offset 6: version     (uint16) - Schema version
//
// All fields are little-endian encoded.

class MessageHeader {
public:
    static constexpr std::size_t SIZE = 8;

    // NexusFix schema constants
    static constexpr SbeUint16 SCHEMA_ID = 1;        // NexusFix schema
    static constexpr SbeUint16 SCHEMA_VERSION = 1;   // Version 1.0

    // Template IDs for NexusFix messages
    struct TemplateId {
        static constexpr SbeUint16 NewOrderSingle = 1;
        static constexpr SbeUint16 ExecutionReport = 8;
    };

    // Field offsets within the header
    struct Offset {
        static constexpr std::size_t BlockLength = 0;
        static constexpr std::size_t TemplateId = 2;
        static constexpr std::size_t SchemaId = 4;
        static constexpr std::size_t Version = 6;
    };

    // ========================================================================
    // Flyweight Decode API
    // ========================================================================

    // Wrap existing buffer for decoding (zero-copy)
    [[nodiscard]] NFX_FORCE_INLINE static MessageHeader wrapForDecode(
        const char* buffer, std::size_t length) noexcept {
        return MessageHeader{buffer, length, false};
    }

    // Check if header is valid
    [[nodiscard]] NFX_FORCE_INLINE bool isValid() const noexcept {
        return buffer_ != nullptr && length_ >= SIZE;
    }

    // Read block length (size of message body)
    [[nodiscard]] NFX_HOT NFX_FORCE_INLINE SbeUint16 blockLength() const noexcept {
        return read_uint16(buffer_ + Offset::BlockLength);
    }

    // Read template ID (message type)
    [[nodiscard]] NFX_HOT NFX_FORCE_INLINE SbeUint16 templateId() const noexcept {
        return read_uint16(buffer_ + Offset::TemplateId);
    }

    // Read schema ID
    [[nodiscard]] NFX_FORCE_INLINE SbeUint16 schemaId() const noexcept {
        return read_uint16(buffer_ + Offset::SchemaId);
    }

    // Read schema version
    [[nodiscard]] NFX_FORCE_INLINE SbeUint16 version() const noexcept {
        return read_uint16(buffer_ + Offset::Version);
    }

    // Get total message size (header + body)
    [[nodiscard]] NFX_FORCE_INLINE std::size_t messageSize() const noexcept {
        return SIZE + blockLength();
    }

    // Get pointer to message body (after header)
    [[nodiscard]] NFX_FORCE_INLINE const char* body() const noexcept {
        return buffer_ + SIZE;
    }

    // Get remaining buffer length after header
    [[nodiscard]] NFX_FORCE_INLINE std::size_t bodyLength() const noexcept {
        return length_ > SIZE ? length_ - SIZE : 0;
    }

    // Validate header schema matches expected
    [[nodiscard]] NFX_FORCE_INLINE bool validateSchema() const noexcept {
        return schemaId() == SCHEMA_ID && version() == SCHEMA_VERSION;
    }

    // ========================================================================
    // Flyweight Encode API
    // ========================================================================

    // Wrap buffer for encoding
    [[nodiscard]] NFX_FORCE_INLINE static MessageHeader wrapForEncode(
        char* buffer, std::size_t length) noexcept {
        return MessageHeader{buffer, length, true};
    }

    // Encode header fields (returns *this for chaining)
    NFX_FORCE_INLINE MessageHeader& encodeHeader(
        SbeUint16 blockLength, SbeUint16 templateId) noexcept {
        write_uint16(mutableBuffer() + Offset::BlockLength, blockLength);
        write_uint16(mutableBuffer() + Offset::TemplateId, templateId);
        write_uint16(mutableBuffer() + Offset::SchemaId, SCHEMA_ID);
        write_uint16(mutableBuffer() + Offset::Version, SCHEMA_VERSION);
        return *this;
    }

    // Get mutable pointer to body for encoding
    [[nodiscard]] NFX_FORCE_INLINE char* mutableBody() noexcept {
        return mutableBuffer() + SIZE;
    }

    // Get mutable buffer pointer
    [[nodiscard]] NFX_FORCE_INLINE char* mutableBuffer() noexcept {
        return const_cast<char*>(buffer_);
    }

    // Get raw buffer pointer
    [[nodiscard]] NFX_FORCE_INLINE const char* buffer() const noexcept {
        return buffer_;
    }

    // Get buffer length
    [[nodiscard]] NFX_FORCE_INLINE std::size_t bufferLength() const noexcept {
        return length_;
    }

private:
    explicit MessageHeader(const char* buffer, std::size_t length,
                          bool /*forEncode*/) noexcept
        : buffer_{buffer}, length_{length} {}

    const char* buffer_{nullptr};
    std::size_t length_{0};
};

// Static assertions for header layout
static_assert(MessageHeader::SIZE == 8, "SBE header must be 8 bytes");
static_assert(MessageHeader::Offset::BlockLength == 0);
static_assert(MessageHeader::Offset::TemplateId == 2);
static_assert(MessageHeader::Offset::SchemaId == 4);
static_assert(MessageHeader::Offset::Version == 6);

}  // namespace nfx::sbe
