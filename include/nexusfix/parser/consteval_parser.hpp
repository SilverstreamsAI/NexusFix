#pragma once

#include <span>
#include <array>
#include <cstdint>
#include <type_traits>

#include "nexusfix/platform/platform.hpp"
#include "nexusfix/types/tag.hpp"
#include "nexusfix/types/error.hpp"
#include "nexusfix/parser/field_view.hpp"

namespace nfx {

// ============================================================================
// Compile-time Field Specification
// ============================================================================

/// Field requirement level
enum class FieldRequirement : uint8_t {
    Required,
    Optional,
    Conditional
};

/// Compile-time field specification
template <int Tag, FieldRequirement Req = FieldRequirement::Required>
struct FieldSpec {
    static constexpr int tag = Tag;
    static constexpr FieldRequirement requirement = Req;
    static constexpr bool is_required = (Req == FieldRequirement::Required);
};

// ============================================================================
// Message Schema (compile-time field layout)
// ============================================================================

/// Compile-time message schema definition
template <typename... Fields>
struct MessageSchema {
    static constexpr size_t field_count = sizeof...(Fields);

    /// Check if schema contains a specific tag
    template <int Tag>
    static consteval bool has_tag() {
        return ((Fields::tag == Tag) || ...);
    }

    /// Get index of tag in schema (or -1 if not found)
    template <int Tag>
    static consteval int tag_index() {
        int index = 0;
        int result = -1;
        ((Fields::tag == Tag ? (result = index, true) : (++index, false)) || ...);
        return result;
    }

    /// Check if tag is required
    template <int Tag>
    static consteval bool is_required() {
        bool result = false;
        ((Fields::tag == Tag && Fields::is_required ? (result = true) : false) || ...);
        return result;
    }

    /// Get array of all tag numbers
    static consteval std::array<int, field_count> tags() {
        return {Fields::tag...};
    }

    /// Get array of requirement flags
    static consteval std::array<bool, field_count> required_flags() {
        return {Fields::is_required...};
    }
};

// ============================================================================
// Standard FIX Header Schema
// ============================================================================

using HeaderSchema = MessageSchema<
    FieldSpec<tag::BeginString::value>,      // 8  - Required
    FieldSpec<tag::BodyLength::value>,       // 9  - Required
    FieldSpec<tag::MsgType::value>,          // 35 - Required
    FieldSpec<tag::SenderCompID::value>,     // 49 - Required
    FieldSpec<tag::TargetCompID::value>,     // 56 - Required
    FieldSpec<tag::MsgSeqNum::value>,        // 34 - Required
    FieldSpec<tag::SendingTime::value>,      // 52 - Required
    FieldSpec<tag::PossDupFlag::value, FieldRequirement::Optional>,    // 43
    FieldSpec<tag::PossResend::value, FieldRequirement::Optional>,     // 97
    FieldSpec<tag::OrigSendingTime::value, FieldRequirement::Optional> // 122
>;

using TrailerSchema = MessageSchema<
    FieldSpec<tag::CheckSum::value>  // 10 - Required
>;

// ============================================================================
// Compile-time Tag to Offset Mapping
// ============================================================================

/// Maps tags to array indices for O(1) lookup
template <size_t MaxTag = 512>
class TagOffsetMap {
public:
    constexpr TagOffsetMap() noexcept {
        for (auto& offset : offsets_) {
            offset = INVALID_OFFSET;
        }
    }

    /// Set offset for a tag
    constexpr void set(int tag, uint16_t offset) noexcept {
        if (tag > 0 && static_cast<size_t>(tag) < MaxTag) {
            offsets_[tag] = offset;
        }
    }

    /// Get offset for a tag (INVALID_OFFSET if not found)
    [[nodiscard]] constexpr uint16_t get(int tag) const noexcept {
        if (tag > 0 && static_cast<size_t>(tag) < MaxTag) {
            return offsets_[tag];
        }
        return INVALID_OFFSET;
    }

    /// Check if tag has valid offset
    [[nodiscard]] constexpr bool has(int tag) const noexcept {
        return get(tag) != INVALID_OFFSET;
    }

    static constexpr uint16_t INVALID_OFFSET = 0xFFFF;

private:
    std::array<uint16_t, MaxTag> offsets_;
};

// ============================================================================
// Compile-time Field Extractor
// ============================================================================

/// Result of compile-time field extraction
template <size_t MaxFields>
struct FieldExtractionResult {
    std::array<FieldView, MaxFields> fields;
    size_t count;
    ParseError error;

    constexpr FieldExtractionResult() noexcept
        : fields{}, count{0}, error{} {}

    [[nodiscard]] constexpr bool ok() const noexcept {
        return error.code == ParseErrorCode::None;
    }

    [[nodiscard]] constexpr FieldView get(int tag) const noexcept {
        for (size_t i = 0; i < count; ++i) {
            if (fields[i].tag == tag) {
                return fields[i];
            }
        }
        return FieldView{};
    }
};

/// Extract fields from buffer (constexpr-capable)
template <size_t MaxFields = 64>
[[nodiscard]] constexpr FieldExtractionResult<MaxFields> extract_fields(
    std::span<const char> data) noexcept
{
    FieldExtractionResult<MaxFields> result;
    FieldIterator iter{data};

    while (iter.has_next() && result.count < MaxFields) {
        FieldView field = iter.next();
        if (!field.is_valid()) {
            result.error = ParseError{ParseErrorCode::InvalidFieldFormat, 0, iter.position()};
            break;
        }
        result.fields[result.count++] = field;
    }

    return result;
}

// ============================================================================
// Schema Validator
// ============================================================================

/// Validate message against schema at compile time
template <typename Schema>
class SchemaValidator {
public:
    /// Validate extracted fields against schema
    template <size_t N>
    [[nodiscard]] static constexpr ParseError validate(
        const FieldExtractionResult<N>& fields) noexcept
    {
        constexpr auto tags = Schema::tags();
        constexpr auto required = Schema::required_flags();

        for (size_t i = 0; i < Schema::field_count; ++i) {
            if (required[i]) {
                bool found = false;
                for (size_t j = 0; j < fields.count; ++j) {
                    if (fields.fields[j].tag == tags[i]) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    return ParseError{ParseErrorCode::MissingRequiredField, tags[i]};
                }
            }
        }

        return ParseError{};  // No error
    }

    /// Check if specific tag is present
    template <size_t N>
    [[nodiscard]] static constexpr bool has_field(
        const FieldExtractionResult<N>& fields,
        int tag) noexcept
    {
        for (size_t i = 0; i < fields.count; ++i) {
            if (fields.fields[i].tag == tag) {
                return true;
            }
        }
        return false;
    }
};

// ============================================================================
// Consteval Header Parser
// ============================================================================

/// Parse and validate FIX header at compile time
struct HeaderParseResult {
    MessageHeader header;
    size_t body_start;  // Offset where body begins
    ParseError error;

    constexpr HeaderParseResult() noexcept
        : header{}, body_start{0}, error{} {}

    [[nodiscard]] constexpr bool ok() const noexcept {
        return error.code == ParseErrorCode::None;
    }
};

/// Parse FIX message header (constexpr-capable)
[[nodiscard]] NFX_HOT
constexpr HeaderParseResult parse_header(
    std::span<const char> data) noexcept
{
    HeaderParseResult result;

    if (data.size() < fix::MIN_MESSAGE_SIZE) [[unlikely]] {
        result.error = ParseError{ParseErrorCode::BufferTooShort};
        return result;
    }

    FieldIterator iter{data};
    int fields_parsed = 0;

    // Parse required header fields in order
    while (iter.has_next() && fields_parsed < 7) [[likely]] {
        FieldView field = iter.next();
        if (!field.is_valid()) [[unlikely]] {
            result.error = ParseError{ParseErrorCode::InvalidFieldFormat, 0, iter.position()};
            return result;
        }

        switch (field.tag) {
            case tag::BeginString::value:
                result.header.begin_string = field.as_string();
                ++fields_parsed;
                break;

            case tag::BodyLength::value:
                if (auto val = field.as_int()) [[likely]] {
                    result.header.body_length = static_cast<int>(*val);
                } else {
                    result.error = ParseError{ParseErrorCode::InvalidBodyLength, 9};
                    return result;
                }
                ++fields_parsed;
                break;

            case tag::MsgType::value:
                result.header.msg_type = field.as_char();
                ++fields_parsed;
                break;

            case tag::SenderCompID::value:
                result.header.sender_comp_id = field.as_string();
                ++fields_parsed;
                break;

            case tag::TargetCompID::value:
                result.header.target_comp_id = field.as_string();
                ++fields_parsed;
                break;

            case tag::MsgSeqNum::value:
                if (auto val = field.as_uint()) [[likely]] {
                    result.header.msg_seq_num = static_cast<uint32_t>(*val);
                } else {
                    result.error = ParseError{ParseErrorCode::InvalidFieldFormat, 34};
                    return result;
                }
                ++fields_parsed;
                break;

            case tag::SendingTime::value:
                result.header.sending_time = field.as_string();
                ++fields_parsed;
                break;

            case tag::PossDupFlag::value:
                result.header.poss_dup_flag = field.as_bool();
                break;

            case tag::PossResend::value:
                result.header.poss_resend = field.as_bool();
                break;

            case tag::OrigSendingTime::value:
                result.header.orig_sending_time = field.as_string();
                break;

            default:
                // Non-header field encountered, body starts here
                result.body_start = iter.position() - field.value.size() - 2;  // Back up
                break;
        }

        if (result.body_start > 0) [[unlikely]] break;
    }

    // Validate required header fields
    if (result.header.begin_string.empty()) [[unlikely]] {
        result.error = ParseError{ParseErrorCode::MissingRequiredField, tag::BeginString::value};
    } else if (result.header.body_length == 0) [[unlikely]] {
        result.error = ParseError{ParseErrorCode::MissingRequiredField, tag::BodyLength::value};
    } else if (result.header.msg_type == '\0') [[unlikely]] {
        result.error = ParseError{ParseErrorCode::MissingRequiredField, tag::MsgType::value};
    } else if (result.header.sender_comp_id.empty()) [[unlikely]] {
        result.error = ParseError{ParseErrorCode::MissingRequiredField, tag::SenderCompID::value};
    } else if (result.header.target_comp_id.empty()) [[unlikely]] {
        result.error = ParseError{ParseErrorCode::MissingRequiredField, tag::TargetCompID::value};
    } else if (result.header.msg_seq_num == 0) [[unlikely]] {
        result.error = ParseError{ParseErrorCode::MissingRequiredField, tag::MsgSeqNum::value};
    }

    if (result.body_start == 0) [[likely]] {
        result.body_start = iter.position();
    }

    return result;
}

// ============================================================================
// Checksum Validation
// ============================================================================

/// Validate FIX checksum
[[nodiscard]] NFX_HOT
constexpr ParseError validate_checksum(
    std::span<const char> data) noexcept
{
    // Find "10=" near the end
    if (data.size() < 7) [[unlikely]] {  // Minimum: "10=000|"
        return ParseError{ParseErrorCode::BufferTooShort};
    }

    const char* __restrict ptr = data.data();

    // Search backwards for "10="
    size_t checksum_pos = data.size();
    for (size_t i = data.size() - 7; i > 0; --i) [[likely]] {
        if (ptr[i] == fix::SOH &&
            ptr[i + 1] == '1' &&
            ptr[i + 2] == '0' &&
            ptr[i + 3] == '=') [[unlikely]] {
            checksum_pos = i + 4;
            break;
        }
    }

    if (checksum_pos >= data.size() - 3) [[unlikely]] {
        return ParseError{ParseErrorCode::MissingRequiredField, tag::CheckSum::value};
    }

    // Parse expected checksum (branch-free 3-digit parsing)
    int expected = (ptr[checksum_pos] - '0') * 100 +
                   (ptr[checksum_pos + 1] - '0') * 10 +
                   (ptr[checksum_pos + 2] - '0');

    // Calculate actual checksum (everything before "10=" including the SOH delimiter)
    // Per FIX spec: checksum includes all bytes up to and including the SOH before 10=
    uint8_t actual = fix::calculate_checksum(data.subspan(0, checksum_pos - 3));

    if (static_cast<int>(actual) != expected) [[unlikely]] {
        return ParseError{ParseErrorCode::InvalidChecksum, tag::CheckSum::value};
    }

    return ParseError{};  // Valid
}

// ============================================================================
// Static Assertions for Parser Types
// ============================================================================

// HeaderParseResult should be reasonably compact
static_assert(sizeof(HeaderParseResult) <= 256,
    "HeaderParseResult should fit in L1 cache");

} // namespace nfx
