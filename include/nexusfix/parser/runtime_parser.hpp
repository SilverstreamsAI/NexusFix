#pragma once

// Suppress MSVC warning C4324: structure was padded due to alignment specifier
// This is expected behavior for cache-line aligned structures
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324)
#endif

#include <span>
#include <cstdint>

#include "nexusfix/platform/platform.hpp"
#include "nexusfix/types/tag.hpp"
#include "nexusfix/types/error.hpp"
#include "nexusfix/interfaces/i_message.hpp"
#include "nexusfix/parser/field_view.hpp"
#include "nexusfix/parser/simd_scanner.hpp"
#include "nexusfix/parser/consteval_parser.hpp"

namespace nfx {

// Use global cache line size
inline constexpr size_t PARSER_CACHE_LINE_SIZE = CACHE_LINE_SIZE;

// ============================================================================
// Parsed Message (zero-copy reference to original buffer)
// ============================================================================

/// Zero-copy parsed FIX message
/// Aligned to cache line boundary for optimal memory access
class alignas(PARSER_CACHE_LINE_SIZE) ParsedMessage {
public:
    static constexpr size_t MAX_FIELDS = 128;

    constexpr ParsedMessage() noexcept
        : raw_{}, header_{}, field_count_{0} {}

    /// Parse from buffer (zero-copy)
    [[nodiscard]] NFX_HOT
    static ParseResult<ParsedMessage> parse(
        std::span<const char> data) noexcept
    {
        ParsedMessage msg;
        msg.raw_ = data;

        // Parse header
        auto header_result = parse_header(data);
        if (!header_result.ok()) [[unlikely]] {
            return std::unexpected{header_result.error};
        }
        msg.header_ = header_result.header;

        // Parse all fields using SIMD-accelerated scanning
        auto soh_positions = simd::scan_soh(data);
        const char* __restrict ptr = data.data();

        size_t field_start = 0;
        for (size_t i = 0; i < soh_positions.count && msg.field_count_ < MAX_FIELDS; ++i) [[likely]] {
            size_t field_end = soh_positions[i];

            // Find '=' separator
            size_t eq_pos = simd::find_equals(data, field_start);
            if (eq_pos >= field_end) [[unlikely]] {
                return std::unexpected{ParseError{
                    ParseErrorCode::InvalidFieldFormat, 0, field_start}};
            }

            // Parse tag number
            int tag = 0;
            for (size_t j = field_start; j < eq_pos; ++j) [[likely]] {
                char c = ptr[j];
                if (c < '0' || c > '9') [[unlikely]] {
                    return std::unexpected{ParseError{
                        ParseErrorCode::InvalidTagNumber, 0, j}};
                }
                tag = tag * 10 + (c - '0');
            }

            // Create field view (zero-copy)
            size_t value_start = eq_pos + 1;
            size_t value_len = field_end - value_start;

            msg.fields_[msg.field_count_++] = FieldView{
                tag,
                std::span<const char>{ptr + value_start, value_len}
            };

            field_start = field_end + 1;  // Skip SOH
        }

        // Validate checksum
        auto checksum_error = validate_checksum(data);
        if (checksum_error.code != ParseErrorCode::None) [[unlikely]] {
            return std::unexpected{checksum_error};
        }

        return msg;
    }

    // ========================================================================
    // Accessors
    // ========================================================================

    /// Get raw message buffer
    [[nodiscard]] constexpr std::span<const char> raw() const noexcept {
        return raw_;
    }

    /// Get parsed header
    [[nodiscard]] constexpr const MessageHeader& header() const noexcept {
        return header_;
    }

    /// Get message type
    [[nodiscard]] constexpr char msg_type() const noexcept {
        return header_.msg_type;
    }

    /// Get sequence number
    [[nodiscard]] constexpr uint32_t msg_seq_num() const noexcept {
        return header_.msg_seq_num;
    }

    /// Get sender comp ID
    [[nodiscard]] constexpr std::string_view sender_comp_id() const noexcept {
        return header_.sender_comp_id;
    }

    /// Get target comp ID
    [[nodiscard]] constexpr std::string_view target_comp_id() const noexcept {
        return header_.target_comp_id;
    }

    /// Get sending time
    [[nodiscard]] constexpr std::string_view sending_time() const noexcept {
        return header_.sending_time;
    }

    /// Get field count
    [[nodiscard]] constexpr size_t field_count() const noexcept {
        return field_count_;
    }

    /// Get field by index
    [[nodiscard]] constexpr FieldView field_at(size_t index) const noexcept {
        return index < field_count_ ? fields_[index] : FieldView{};
    }

    /// Get field by tag (O(n) linear search)
    [[nodiscard]] constexpr FieldView get_field(int tag) const noexcept {
        for (size_t i = 0; i < field_count_; ++i) {
            if (fields_[i].tag == tag) {
                return fields_[i];
            }
        }
        return FieldView{};
    }

    /// Check if field exists
    [[nodiscard]] constexpr bool has_field(int tag) const noexcept {
        for (size_t i = 0; i < field_count_; ++i) {
            if (fields_[i].tag == tag) {
                return true;
            }
        }
        return false;
    }

    /// Get string field value
    [[nodiscard]] constexpr std::string_view get_string(int tag) const noexcept {
        return get_field(tag).as_string();
    }

    /// Get int field value
    [[nodiscard]] constexpr std::optional<int64_t> get_int(int tag) const noexcept {
        return get_field(tag).as_int();
    }

    /// Get char field value
    [[nodiscard]] constexpr char get_char(int tag) const noexcept {
        return get_field(tag).as_char();
    }

    /// Get price field value
    [[nodiscard]] constexpr FixedPrice get_price(int tag) const noexcept {
        return get_field(tag).as_price();
    }

    /// Get quantity field value
    [[nodiscard]] constexpr Qty get_qty(int tag) const noexcept {
        return get_field(tag).as_qty();
    }

    // ========================================================================
    // Iteration
    // ========================================================================

    /// Iterator for range-based for loops
    [[nodiscard]] constexpr const FieldView* begin() const noexcept {
        return fields_.data();
    }

    [[nodiscard]] constexpr const FieldView* end() const noexcept {
        return fields_.data() + field_count_;
    }

private:
    std::span<const char> raw_;
    MessageHeader header_;
    std::array<FieldView, MAX_FIELDS> fields_;
    size_t field_count_;
};

// ============================================================================
// Stream Parser (for handling partial messages)
// ============================================================================

/// Parser for FIX message streams (handles buffering and framing)
class StreamParser {
public:
    static constexpr size_t DEFAULT_BUFFER_SIZE = 65536;

    StreamParser() noexcept = default;

    /// Feed data into parser
    /// Returns number of bytes consumed
    size_t feed(std::span<const char> data) noexcept {
        size_t consumed = 0;

        while (consumed < data.size()) {
            // Try to find complete message
            auto remaining = data.subspan(consumed);
            auto boundary = simd::find_message_boundary(remaining);

            if (!boundary.complete) {
                break;  // Need more data
            }

            // Store message boundary for retrieval
            if (pending_count_ < MAX_PENDING) {
                pending_messages_[pending_count_++] = {
                    consumed + boundary.start,
                    consumed + boundary.end
                };
            }

            consumed = boundary.end;
        }

        return consumed;
    }

    /// Check if complete messages are available
    [[nodiscard]] bool has_message() const noexcept {
        return pending_count_ > 0;
    }

    /// Get next message boundary
    [[nodiscard]] std::pair<size_t, size_t> next_message() noexcept {
        if (pending_count_ == 0) {
            return {0, 0};
        }

        auto result = pending_messages_[0];

        // Shift remaining messages
        for (size_t i = 1; i < pending_count_; ++i) {
            pending_messages_[i - 1] = pending_messages_[i];
        }
        --pending_count_;

        return result;
    }

    /// Clear parser state
    void reset() noexcept {
        pending_count_ = 0;
    }

private:
    static constexpr size_t MAX_PENDING = 16;

    std::array<std::pair<size_t, size_t>, MAX_PENDING> pending_messages_;
    size_t pending_count_{0};
};

// ============================================================================
// Optimized Tag Lookup Parser
// ============================================================================

/// Parser with O(1) tag lookup using precomputed offset table
/// Aligned to cache line boundary for optimal memory access
class alignas(PARSER_CACHE_LINE_SIZE) IndexedParser {
public:
    static constexpr size_t MAX_TAG = 512;

    /// Parse and index all fields for O(1) lookup
    [[nodiscard]] NFX_HOT
    static ParseResult<IndexedParser> parse(
        std::span<const char> data) noexcept
    {
        IndexedParser parser;
        parser.raw_ = data;

        // Parse header
        auto header_result = parse_header(data);
        if (!header_result.ok()) [[unlikely]] {
            return std::unexpected{header_result.error};
        }
        parser.header_ = header_result.header;

        // Index all fields
        FieldIterator iter{data};
        while (iter.has_next()) [[likely]] {
            FieldView field = iter.next();
            if (!field.is_valid()) [[unlikely]] break;

            // Store in lookup table for O(1) access
            if (field.tag > 0 && static_cast<size_t>(field.tag) < MAX_TAG) [[likely]] {
                parser.field_table_.set(field.tag, field.value);
            }
        }

        // Validate checksum
        auto checksum_error = validate_checksum(data);
        if (checksum_error.code != ParseErrorCode::None) [[unlikely]] {
            return std::unexpected{checksum_error};
        }

        return parser;
    }

    // ========================================================================
    // O(1) Field Access
    // ========================================================================

    /// Get field by tag (O(1) lookup)
    [[nodiscard]] NFX_HOT FieldView get_field(int tag) const noexcept {
        return field_table_.get(tag);
    }

    /// Check if field exists (O(1))
    [[nodiscard]] NFX_HOT bool has_field(int tag) const noexcept {
        return field_table_.has(tag);
    }

    [[nodiscard]] NFX_HOT std::string_view get_string(int tag) const noexcept {
        return field_table_.get_string(tag);
    }

    [[nodiscard]] NFX_HOT std::optional<int64_t> get_int(int tag) const noexcept {
        return field_table_.get_int(tag);
    }

    [[nodiscard]] NFX_HOT char get_char(int tag) const noexcept {
        return field_table_.get_char(tag);
    }

    // ========================================================================
    // Header Access
    // ========================================================================

    [[nodiscard]] const MessageHeader& header() const noexcept {
        return header_;
    }

    [[nodiscard]] std::span<const char> raw() const noexcept {
        return raw_;
    }

    [[nodiscard]] char msg_type() const noexcept {
        return header_.msg_type;
    }

    [[nodiscard]] uint32_t msg_seq_num() const noexcept {
        return header_.msg_seq_num;
    }

    [[nodiscard]] std::string_view sender_comp_id() const noexcept {
        return header_.sender_comp_id;
    }

    [[nodiscard]] std::string_view target_comp_id() const noexcept {
        return header_.target_comp_id;
    }

    [[nodiscard]] std::string_view sending_time() const noexcept {
        return header_.sending_time;
    }

    // ========================================================================
    // Version Detection
    // ========================================================================

    /// Get BeginString value
    [[nodiscard]] std::string_view begin_string() const noexcept {
        return header_.begin_string;
    }

    /// Check if this is a FIXT 1.1 message (FIX 5.0+ transport)
    [[nodiscard]] bool is_fixt11() const noexcept {
        return header_.is_fixt11();
    }

    /// Check if this is a FIX 4.x message
    [[nodiscard]] bool is_fix4() const noexcept {
        return header_.is_fix4();
    }

    /// Check if this is a FIX 4.4 message
    [[nodiscard]] bool is_fix44() const noexcept {
        return header_.is_fix44();
    }

    /// Check if this is an admin/session message
    [[nodiscard]] bool is_admin_message() const noexcept {
        return header_.is_admin_message();
    }

    /// Check if this is an application message
    [[nodiscard]] bool is_app_message() const noexcept {
        return header_.is_app_message();
    }

private:
    IndexedParser() noexcept = default;

    std::span<const char> raw_;
    MessageHeader header_;
    FieldTable<MAX_TAG> field_table_;
};

// ============================================================================
// Convenience Functions
// ============================================================================

/// Parse FIX message from buffer
[[nodiscard]] NFX_HOT
inline ParseResult<ParsedMessage> parse_message(
    std::span<const char> data) noexcept
{
    return ParsedMessage::parse(data);
}

/// Parse with O(1) field lookup
[[nodiscard]] NFX_HOT
inline ParseResult<IndexedParser> parse_indexed(
    std::span<const char> data) noexcept
{
    return IndexedParser::parse(data);
}

// ============================================================================
// Static Assertions for Parser Layout
// ============================================================================

// Verify parsers are cache-line aligned
static_assert(alignof(ParsedMessage) >= PARSER_CACHE_LINE_SIZE,
    "ParsedMessage must be cache-line aligned for optimal memory access");

static_assert(alignof(IndexedParser) >= PARSER_CACHE_LINE_SIZE,
    "IndexedParser must be cache-line aligned for optimal memory access");

} // namespace nfx

#ifdef _MSC_VER
#pragma warning(pop)
#endif
