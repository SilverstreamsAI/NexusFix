#pragma once

#include <span>
#include <string_view>
#include <array>
#include <cstdint>

#include "nexusfix/types/tag.hpp"
#include "nexusfix/types/field_types.hpp"
#include "nexusfix/types/error.hpp"
#include "nexusfix/interfaces/i_message.hpp"
#include "nexusfix/parser/field_view.hpp"

namespace nfx {

// ============================================================================
// FIX Message Header Builder
// ============================================================================

/// Builder for constructing FIX message headers
class HeaderBuilder {
public:
    static constexpr size_t MAX_HEADER_SIZE = 256;

    constexpr HeaderBuilder() noexcept
        : buffer_{}, pos_{0} {}

    /// Set BeginString (tag 8)
    HeaderBuilder& begin_string(std::string_view value) noexcept {
        append_field(tag::BeginString::value, value);
        return *this;
    }

    /// Set BodyLength (tag 9) - placeholder, will be updated
    HeaderBuilder& body_length_placeholder() noexcept {
        body_length_pos_ = pos_;
        append_raw("9=000000");
        append_soh();
        return *this;
    }

    /// Set MsgType (tag 35)
    HeaderBuilder& msg_type(char type) noexcept {
        append_field(tag::MsgType::value, std::string_view{&type, 1});
        return *this;
    }

    /// Set SenderCompID (tag 49)
    HeaderBuilder& sender_comp_id(std::string_view value) noexcept {
        append_field(tag::SenderCompID::value, value);
        return *this;
    }

    /// Set TargetCompID (tag 56)
    HeaderBuilder& target_comp_id(std::string_view value) noexcept {
        append_field(tag::TargetCompID::value, value);
        return *this;
    }

    /// Set MsgSeqNum (tag 34)
    HeaderBuilder& msg_seq_num(uint32_t seq) noexcept {
        append_field(tag::MsgSeqNum::value, seq);
        return *this;
    }

    /// Set SendingTime (tag 52)
    HeaderBuilder& sending_time(std::string_view value) noexcept {
        append_field(tag::SendingTime::value, value);
        return *this;
    }

    /// Set PossDupFlag (tag 43)
    HeaderBuilder& poss_dup_flag(bool value) noexcept {
        append_field(tag::PossDupFlag::value, value ? "Y" : "N");
        return *this;
    }

    /// Set OrigSendingTime (tag 122) - required when PossDupFlag=Y
    HeaderBuilder& orig_sending_time(std::string_view value) noexcept {
        append_field(tag::OrigSendingTime::value, value);
        return *this;
    }

    /// Get current buffer content
    [[nodiscard]] std::span<const char> data() const noexcept {
        return std::span<const char>{buffer_.data(), pos_};
    }

    /// Get current size
    [[nodiscard]] size_t size() const noexcept { return pos_; }

    /// Get position where body length value starts
    [[nodiscard]] size_t body_length_pos() const noexcept { return body_length_pos_; }

    /// Update body length value in buffer
    void update_body_length(size_t body_len) noexcept {
        if (body_length_pos_ == 0) return;

        // Format as 6-digit number
        size_t pos = body_length_pos_ + 2;  // Skip "9="
        for (int i = 5; i >= 0; --i) {
            buffer_[pos + i] = '0' + (body_len % 10);
            body_len /= 10;
        }
    }

    /// Reset builder
    void reset() noexcept {
        pos_ = 0;
        body_length_pos_ = 0;
    }

private:
    void append_raw(std::string_view sv) noexcept {
        for (char c : sv) {
            if (pos_ < MAX_HEADER_SIZE) {
                buffer_[pos_++] = c;
            }
        }
    }

    void append_soh() noexcept {
        if (pos_ < MAX_HEADER_SIZE) {
            buffer_[pos_++] = fix::SOH;
        }
    }

    void append_field(int tag, std::string_view value) noexcept {
        // Append tag
        char tag_buf[16];
        int tag_len = 0;
        int t = tag;
        do {
            tag_buf[tag_len++] = '0' + (t % 10);
            t /= 10;
        } while (t > 0);

        for (int i = tag_len - 1; i >= 0; --i) {
            if (pos_ < MAX_HEADER_SIZE) {
                buffer_[pos_++] = tag_buf[i];
            }
        }

        // Append '='
        if (pos_ < MAX_HEADER_SIZE) buffer_[pos_++] = '=';

        // Append value
        append_raw(value);

        // Append SOH
        append_soh();
    }

    void append_field(int tag, uint32_t value) noexcept {
        char buf[16];
        int len = 0;
        uint32_t v = value;
        do {
            buf[len++] = '0' + (v % 10);
            v /= 10;
        } while (v > 0);

        // Reverse
        for (int i = 0; i < len / 2; ++i) {
            char tmp = buf[i];
            buf[i] = buf[len - 1 - i];
            buf[len - 1 - i] = tmp;
        }

        append_field(tag, std::string_view{buf, static_cast<size_t>(len)});
    }

    std::array<char, MAX_HEADER_SIZE> buffer_;
    size_t pos_;
    size_t body_length_pos_{0};
};

// ============================================================================
// Standard FIX 4.4 Header
// ============================================================================

/// Parsed FIX header with direct field access
struct FixHeader {
    std::string_view begin_string;    // Tag 8
    int body_length;                   // Tag 9
    char msg_type;                     // Tag 35
    std::string_view sender_comp_id;  // Tag 49
    std::string_view target_comp_id;  // Tag 56
    uint32_t msg_seq_num;             // Tag 34
    std::string_view sending_time;    // Tag 52
    bool poss_dup_flag;               // Tag 43
    bool poss_resend;                 // Tag 97
    std::string_view orig_sending_time; // Tag 122

    constexpr FixHeader() noexcept
        : begin_string{}
        , body_length{0}
        , msg_type{'\0'}
        , sender_comp_id{}
        , target_comp_id{}
        , msg_seq_num{0}
        , sending_time{}
        , poss_dup_flag{false}
        , poss_resend{false}
        , orig_sending_time{} {}

    /// Parse header from field table
    template <size_t MaxTag>
    static FixHeader from_fields(const FieldTable<MaxTag>& fields) noexcept {
        FixHeader hdr;
        hdr.begin_string = fields.get_string(tag::BeginString::value);
        if (auto v = fields.get_int(tag::BodyLength::value)) {
            hdr.body_length = static_cast<int>(*v);
        }
        hdr.msg_type = fields.get_char(tag::MsgType::value);
        hdr.sender_comp_id = fields.get_string(tag::SenderCompID::value);
        hdr.target_comp_id = fields.get_string(tag::TargetCompID::value);
        if (auto v = fields.get_int(tag::MsgSeqNum::value)) {
            hdr.msg_seq_num = static_cast<uint32_t>(*v);
        }
        hdr.sending_time = fields.get_string(tag::SendingTime::value);
        hdr.poss_dup_flag = fields.get_char(tag::PossDupFlag::value) == 'Y';
        hdr.poss_resend = fields.get_char(tag::PossResend::value) == 'Y';
        hdr.orig_sending_time = fields.get_string(tag::OrigSendingTime::value);
        return hdr;
    }

    /// Validate required header fields
    [[nodiscard]] constexpr ParseError validate() const noexcept {
        if (begin_string.empty()) {
            return ParseError{ParseErrorCode::MissingRequiredField, tag::BeginString::value};
        }
        if (body_length <= 0) {
            return ParseError{ParseErrorCode::MissingRequiredField, tag::BodyLength::value};
        }
        if (msg_type == '\0') {
            return ParseError{ParseErrorCode::MissingRequiredField, tag::MsgType::value};
        }
        if (sender_comp_id.empty()) {
            return ParseError{ParseErrorCode::MissingRequiredField, tag::SenderCompID::value};
        }
        if (target_comp_id.empty()) {
            return ParseError{ParseErrorCode::MissingRequiredField, tag::TargetCompID::value};
        }
        if (msg_seq_num == 0) {
            return ParseError{ParseErrorCode::MissingRequiredField, tag::MsgSeqNum::value};
        }
        if (sending_time.empty()) {
            return ParseError{ParseErrorCode::MissingRequiredField, tag::SendingTime::value};
        }
        // OrigSendingTime required when PossDupFlag=Y
        if (poss_dup_flag && orig_sending_time.empty()) {
            return ParseError{ParseErrorCode::MissingRequiredField, tag::OrigSendingTime::value};
        }
        return ParseError{};
    }
};

} // namespace nfx
