#pragma once

#include <span>
#include <array>
#include <cstdint>
#include <cstdio>

#include "nexusfix/types/tag.hpp"
#include "nexusfix/types/error.hpp"
#include "nexusfix/interfaces/i_message.hpp"

namespace nfx {

// ============================================================================
// FIX Message Trailer
// ============================================================================

/// FIX trailer with checksum calculation
struct FixTrailer {
    std::array<char, 3> check_sum;  // Tag 10 - 3 digit checksum

    constexpr FixTrailer() noexcept : check_sum{'0', '0', '0'} {}

    explicit constexpr FixTrailer(uint8_t sum) noexcept
        : check_sum{
            static_cast<char>('0' + (sum / 100)),
            static_cast<char>('0' + ((sum / 10) % 10)),
            static_cast<char>('0' + (sum % 10))
        } {}

    /// Get checksum as string view
    [[nodiscard]] constexpr std::string_view as_string() const noexcept {
        return std::string_view{check_sum.data(), 3};
    }

    /// Get checksum as integer
    [[nodiscard]] constexpr int as_int() const noexcept {
        return (check_sum[0] - '0') * 100 +
               (check_sum[1] - '0') * 10 +
               (check_sum[2] - '0');
    }
};

// ============================================================================
// Trailer Builder
// ============================================================================

/// Builder for constructing FIX message trailers
class TrailerBuilder {
public:
    static constexpr size_t TRAILER_SIZE = 7;  // "10=XXX\x01"

    constexpr TrailerBuilder() noexcept : buffer_{} {}

    /// Build trailer from message body (calculates checksum)
    [[nodiscard]] std::span<const char> build(std::span<const char> message_without_trailer) noexcept {
        uint8_t checksum = fix::calculate_checksum(message_without_trailer);
        return build(checksum);
    }

    /// Build trailer with pre-calculated checksum
    [[nodiscard]] std::span<const char> build(uint8_t checksum) noexcept {
        buffer_[0] = '1';
        buffer_[1] = '0';
        buffer_[2] = '=';
        buffer_[3] = '0' + (checksum / 100);
        buffer_[4] = '0' + ((checksum / 10) % 10);
        buffer_[5] = '0' + (checksum % 10);
        buffer_[6] = fix::SOH;

        return std::span<const char>{buffer_.data(), TRAILER_SIZE};
    }

    /// Get built trailer
    [[nodiscard]] std::span<const char> data() const noexcept {
        return std::span<const char>{buffer_.data(), TRAILER_SIZE};
    }

private:
    std::array<char, TRAILER_SIZE> buffer_;
};

// ============================================================================
// Checksum Utilities
// ============================================================================

namespace checksum {

/// Calculate FIX checksum for a message
[[nodiscard]] inline constexpr uint8_t calculate(std::span<const char> data) noexcept {
    return fix::calculate_checksum(data);
}

/// Validate checksum of a complete message
[[nodiscard]] inline ParseError validate(std::span<const char> message) noexcept {
    // Message must end with "10=XXX\x01"
    if (message.size() < 8) {
        return ParseError{ParseErrorCode::BufferTooShort};
    }

    // Find the checksum field
    size_t trailer_start = message.size();
    for (size_t i = message.size() - 8; i < message.size() - 6; ++i) {
        if (message[i] == fix::SOH &&
            message[i + 1] == '1' &&
            message[i + 2] == '0' &&
            message[i + 3] == '=') {
            trailer_start = i + 1;
            break;
        }
    }

    if (trailer_start >= message.size()) {
        return ParseError{ParseErrorCode::MissingRequiredField, tag::CheckSum::value};
    }

    // Parse expected checksum
    size_t checksum_value_pos = trailer_start + 3;  // After "10="
    if (checksum_value_pos + 3 > message.size()) {
        return ParseError{ParseErrorCode::InvalidChecksum};
    }

    int expected = 0;
    for (int i = 0; i < 3; ++i) {
        char c = message[checksum_value_pos + i];
        if (c < '0' || c > '9') {
            return ParseError{ParseErrorCode::InvalidChecksum};
        }
        expected = expected * 10 + (c - '0');
    }

    // Calculate actual checksum (everything before "10=")
    // We need to include the SOH before "10="
    size_t body_end = trailer_start - 1;  // Position of SOH before 10=
    if (body_end == 0 || message[body_end] != fix::SOH) {
        // If no SOH before, include up to trailer_start
        body_end = trailer_start;
    }

    uint8_t actual = calculate(message.subspan(0, trailer_start));

    if (static_cast<int>(actual) != expected) {
        return ParseError{ParseErrorCode::InvalidChecksum, tag::CheckSum::value};
    }

    return ParseError{};
}

/// Format checksum value as 3-digit string
[[nodiscard]] inline constexpr std::array<char, 3> format(uint8_t checksum) noexcept {
    return fix::format_checksum(checksum);
}

} // namespace checksum

// ============================================================================
// Complete Message Builder
// ============================================================================

/// Assembles complete FIX message with header, body, and trailer
class MessageAssembler {
public:
    static constexpr size_t MAX_MESSAGE_SIZE = 4096;

    constexpr MessageAssembler() noexcept : buffer_{}, pos_{0} {}

    /// Start building a new message
    MessageAssembler& start(std::string_view begin_string = fix::FIX_4_4) noexcept {
        pos_ = 0;
        append_field(tag::BeginString::value, begin_string);
        body_length_pos_ = pos_;
        append_raw("9=000000");  // Placeholder
        append_soh();
        body_start_ = pos_;
        return *this;
    }

    /// Start building a FIXT 1.1 message (for FIX 5.0+ sessions)
    MessageAssembler& start_fixt11() noexcept {
        return start(fix::FIXT_1_1);
    }

    /// Append field to message body
    MessageAssembler& field(int tag_num, std::string_view value) noexcept {
        append_field(tag_num, value);
        return *this;
    }

    /// Append integer field
    MessageAssembler& field(int tag_num, int64_t value) noexcept {
        char buf[32];
        int len = 0;
        bool negative = value < 0;
        if (negative) value = -value;

        do {
            buf[len++] = '0' + (value % 10);
            value /= 10;
        } while (value > 0);

        if (negative) buf[len++] = '-';

        // Reverse
        for (int i = 0; i < len / 2; ++i) {
            char tmp = buf[i];
            buf[i] = buf[len - 1 - i];
            buf[len - 1 - i] = tmp;
        }

        return field(tag_num, std::string_view{buf, static_cast<size_t>(len)});
    }

    /// Append char field
    MessageAssembler& field(int tag_num, char value) noexcept {
        return field(tag_num, std::string_view{&value, 1});
    }

    /// Append price field
    MessageAssembler& field(int tag_num, FixedPrice price) noexcept {
        // Format price to string
        char buf[32];
        double d = price.to_double();
        int len = std::snprintf(buf, sizeof(buf), "%.8g", d);
        return field(tag_num, std::string_view{buf, static_cast<size_t>(len)});
    }

    /// Finalize message (updates body length and adds checksum)
    [[nodiscard]] std::span<const char> finish() noexcept {
        // Calculate body length (from after 9=XXXXXX\x01 to before 10=)
        size_t body_length = pos_ - body_start_;

        // Update body length field
        size_t len_pos = body_length_pos_ + 2;  // After "9="
        for (int i = 5; i >= 0; --i) {
            buffer_[len_pos + i] = '0' + (body_length % 10);
            body_length /= 10;
        }

        // Calculate checksum (everything before trailer)
        uint8_t checksum = fix::calculate_checksum(
            std::span<const char>{buffer_.data(), pos_});

        // Append trailer
        append_field(tag::CheckSum::value, checksum::format(checksum));

        return std::span<const char>{buffer_.data(), pos_};
    }

    /// Get current message content (before finish)
    [[nodiscard]] std::span<const char> data() const noexcept {
        return std::span<const char>{buffer_.data(), pos_};
    }

    /// Reset for new message
    void reset() noexcept {
        pos_ = 0;
        body_length_pos_ = 0;
        body_start_ = 0;
    }

private:
    void append_raw(std::string_view sv) noexcept {
        for (char c : sv) {
            if (pos_ < MAX_MESSAGE_SIZE) {
                buffer_[pos_++] = c;
            }
        }
    }

    void append_soh() noexcept {
        if (pos_ < MAX_MESSAGE_SIZE) {
            buffer_[pos_++] = fix::SOH;
        }
    }

    void append_field(int tag, std::string_view value) noexcept {
        char tag_buf[16];
        int tag_len = 0;
        int t = tag;
        do {
            tag_buf[tag_len++] = '0' + (t % 10);
            t /= 10;
        } while (t > 0);

        for (int i = tag_len - 1; i >= 0; --i) {
            if (pos_ < MAX_MESSAGE_SIZE) {
                buffer_[pos_++] = tag_buf[i];
            }
        }

        if (pos_ < MAX_MESSAGE_SIZE) buffer_[pos_++] = '=';
        append_raw(value);
        append_soh();
    }

    void append_field(int tag, std::array<char, 3> value) noexcept {
        append_field(tag, std::string_view{value.data(), 3});
    }

    std::array<char, MAX_MESSAGE_SIZE> buffer_;
    size_t pos_;
    size_t body_length_pos_{0};
    size_t body_start_{0};
};

} // namespace nfx
