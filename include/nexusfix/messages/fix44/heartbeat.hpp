#pragma once

#include <span>
#include <string_view>

#include "nexusfix/types/tag.hpp"
#include "nexusfix/types/error.hpp"
#include "nexusfix/interfaces/i_message.hpp"
#include "nexusfix/parser/runtime_parser.hpp"
#include "nexusfix/messages/common/header.hpp"
#include "nexusfix/messages/common/trailer.hpp"

namespace nfx::fix44 {

// ============================================================================
// Heartbeat Message (MsgType = 0)
// ============================================================================

/// FIX 4.4 Heartbeat message (35=0)
/// Sent to monitor connection status
struct Heartbeat {
    static constexpr char MSG_TYPE = msg_type::Heartbeat;

    FixHeader header;
    std::string_view test_req_id;  // Tag 112 - Conditional (required if responding to TestRequest)
    std::span<const char> raw_data;

    constexpr Heartbeat() noexcept : header{}, test_req_id{}, raw_data{} {}

    [[nodiscard]] constexpr std::span<const char> raw() const noexcept {
        return raw_data;
    }

    [[nodiscard]] constexpr uint32_t msg_seq_num() const noexcept {
        return header.msg_seq_num;
    }

    [[nodiscard]] constexpr std::string_view sender_comp_id() const noexcept {
        return header.sender_comp_id;
    }

    [[nodiscard]] constexpr std::string_view target_comp_id() const noexcept {
        return header.target_comp_id;
    }

    [[nodiscard]] constexpr std::string_view sending_time() const noexcept {
        return header.sending_time;
    }

    [[nodiscard]] static ParseResult<Heartbeat> from_buffer(
        std::span<const char> buffer) noexcept
    {
        // Use lvalue to call and_then with reference parameter (avoids 12KB copy)
        auto parsed = IndexedParser::parse(buffer);
        return parsed.and_then([buffer](IndexedParser& p) -> ParseResult<Heartbeat> {
            if (p.msg_type() != MSG_TYPE) [[unlikely]] {
                return std::unexpected{ParseError{ParseErrorCode::InvalidMsgType}};
            }

            Heartbeat msg;
            msg.raw_data = buffer;
            msg.header.begin_string = p.get_string(tag::BeginString::value);
            msg.header.msg_type = p.msg_type();
            msg.header.sender_comp_id = p.sender_comp_id();
            msg.header.target_comp_id = p.target_comp_id();
            msg.header.msg_seq_num = p.msg_seq_num();
            msg.header.sending_time = p.sending_time();
            msg.test_req_id = p.get_string(tag::TestReqID::value);

            return msg;
        });
    }

    class Builder {
    public:
        Builder& sender_comp_id(std::string_view v) noexcept {
            sender_comp_id_ = v;
            return *this;
        }

        Builder& target_comp_id(std::string_view v) noexcept {
            target_comp_id_ = v;
            return *this;
        }

        Builder& msg_seq_num(uint32_t v) noexcept {
            msg_seq_num_ = v;
            return *this;
        }

        Builder& sending_time(std::string_view v) noexcept {
            sending_time_ = v;
            return *this;
        }

        Builder& test_req_id(std::string_view v) noexcept {
            test_req_id_ = v;
            return *this;
        }

        [[nodiscard]] std::span<const char> build(MessageAssembler& asm_) const noexcept {
            asm_.start()
                .field(tag::MsgType::value, MSG_TYPE)
                .field(tag::SenderCompID::value, sender_comp_id_)
                .field(tag::TargetCompID::value, target_comp_id_)
                .field(tag::MsgSeqNum::value, static_cast<int64_t>(msg_seq_num_))
                .field(tag::SendingTime::value, sending_time_);

            if (!test_req_id_.empty()) {
                asm_.field(tag::TestReqID::value, test_req_id_);
            }

            return asm_.finish();
        }

    private:
        std::string_view sender_comp_id_;
        std::string_view target_comp_id_;
        uint32_t msg_seq_num_{1};
        std::string_view sending_time_;
        std::string_view test_req_id_;
    };
};

// ============================================================================
// TestRequest Message (MsgType = 1)
// ============================================================================

/// FIX 4.4 TestRequest message (35=1)
/// Requests counterparty to send a Heartbeat
struct TestRequest {
    static constexpr char MSG_TYPE = msg_type::TestRequest;

    FixHeader header;
    std::string_view test_req_id;  // Tag 112 - Required
    std::span<const char> raw_data;

    constexpr TestRequest() noexcept : header{}, test_req_id{}, raw_data{} {}

    [[nodiscard]] constexpr std::span<const char> raw() const noexcept {
        return raw_data;
    }

    [[nodiscard]] constexpr uint32_t msg_seq_num() const noexcept {
        return header.msg_seq_num;
    }

    [[nodiscard]] constexpr std::string_view sender_comp_id() const noexcept {
        return header.sender_comp_id;
    }

    [[nodiscard]] constexpr std::string_view target_comp_id() const noexcept {
        return header.target_comp_id;
    }

    [[nodiscard]] constexpr std::string_view sending_time() const noexcept {
        return header.sending_time;
    }

    [[nodiscard]] static ParseResult<TestRequest> from_buffer(
        std::span<const char> buffer) noexcept
    {
        auto parsed = IndexedParser::parse(buffer);
        return parsed.and_then([buffer](IndexedParser& p) -> ParseResult<TestRequest> {
            if (p.msg_type() != MSG_TYPE) [[unlikely]] {
                return std::unexpected{ParseError{ParseErrorCode::InvalidMsgType}};
            }

            TestRequest msg;
            msg.raw_data = buffer;
            msg.header.begin_string = p.get_string(tag::BeginString::value);
            msg.header.msg_type = p.msg_type();
            msg.header.sender_comp_id = p.sender_comp_id();
            msg.header.target_comp_id = p.target_comp_id();
            msg.header.msg_seq_num = p.msg_seq_num();
            msg.header.sending_time = p.sending_time();
            msg.test_req_id = p.get_string(tag::TestReqID::value);

            if (msg.test_req_id.empty()) [[unlikely]] {
                return std::unexpected{ParseError{ParseErrorCode::MissingRequiredField, tag::TestReqID::value}};
            }

            return msg;
        });
    }

    class Builder {
    public:
        Builder& sender_comp_id(std::string_view v) noexcept {
            sender_comp_id_ = v;
            return *this;
        }

        Builder& target_comp_id(std::string_view v) noexcept {
            target_comp_id_ = v;
            return *this;
        }

        Builder& msg_seq_num(uint32_t v) noexcept {
            msg_seq_num_ = v;
            return *this;
        }

        Builder& sending_time(std::string_view v) noexcept {
            sending_time_ = v;
            return *this;
        }

        Builder& test_req_id(std::string_view v) noexcept {
            test_req_id_ = v;
            return *this;
        }

        [[nodiscard]] std::span<const char> build(MessageAssembler& asm_) const noexcept {
            asm_.start()
                .field(tag::MsgType::value, MSG_TYPE)
                .field(tag::SenderCompID::value, sender_comp_id_)
                .field(tag::TargetCompID::value, target_comp_id_)
                .field(tag::MsgSeqNum::value, static_cast<int64_t>(msg_seq_num_))
                .field(tag::SendingTime::value, sending_time_)
                .field(tag::TestReqID::value, test_req_id_);

            return asm_.finish();
        }

    private:
        std::string_view sender_comp_id_;
        std::string_view target_comp_id_;
        uint32_t msg_seq_num_{1};
        std::string_view sending_time_;
        std::string_view test_req_id_;
    };
};

// ============================================================================
// ResendRequest Message (MsgType = 2)
// ============================================================================

/// FIX 4.4 ResendRequest message (35=2)
struct ResendRequest {
    static constexpr char MSG_TYPE = msg_type::ResendRequest;

    FixHeader header;
    uint32_t begin_seq_no;  // Tag 7 - Required
    uint32_t end_seq_no;    // Tag 16 - Required (0 = infinity)
    std::span<const char> raw_data;

    constexpr ResendRequest() noexcept
        : header{}, begin_seq_no{0}, end_seq_no{0}, raw_data{} {}

    [[nodiscard]] constexpr std::span<const char> raw() const noexcept {
        return raw_data;
    }

    [[nodiscard]] constexpr uint32_t msg_seq_num() const noexcept {
        return header.msg_seq_num;
    }

    [[nodiscard]] constexpr std::string_view sender_comp_id() const noexcept {
        return header.sender_comp_id;
    }

    [[nodiscard]] constexpr std::string_view target_comp_id() const noexcept {
        return header.target_comp_id;
    }

    [[nodiscard]] constexpr std::string_view sending_time() const noexcept {
        return header.sending_time;
    }

    [[nodiscard]] static ParseResult<ResendRequest> from_buffer(
        std::span<const char> buffer) noexcept
    {
        auto parsed = IndexedParser::parse(buffer);
        return parsed.and_then([buffer](IndexedParser& p) -> ParseResult<ResendRequest> {
            if (p.msg_type() != MSG_TYPE) [[unlikely]] {
                return std::unexpected{ParseError{ParseErrorCode::InvalidMsgType}};
            }

            ResendRequest msg;
            msg.raw_data = buffer;
            msg.header.begin_string = p.get_string(tag::BeginString::value);
            msg.header.msg_type = p.msg_type();
            msg.header.sender_comp_id = p.sender_comp_id();
            msg.header.target_comp_id = p.target_comp_id();
            msg.header.msg_seq_num = p.msg_seq_num();
            msg.header.sending_time = p.sending_time();

            // Use require_field for required integer fields
            auto begin_result = require_field(to_uint32(p.get_int(7)), 7);  // BeginSeqNo
            if (!begin_result) [[unlikely]] return std::unexpected{begin_result.error()};
            msg.begin_seq_no = *begin_result;

            auto end_result = require_field(to_uint32(p.get_int(16)), 16);  // EndSeqNo
            if (!end_result) [[unlikely]] return std::unexpected{end_result.error()};
            msg.end_seq_no = *end_result;

            return msg;
        });
    }

    class Builder {
    public:
        Builder& sender_comp_id(std::string_view v) noexcept {
            sender_comp_id_ = v;
            return *this;
        }

        Builder& target_comp_id(std::string_view v) noexcept {
            target_comp_id_ = v;
            return *this;
        }

        Builder& msg_seq_num(uint32_t v) noexcept {
            msg_seq_num_ = v;
            return *this;
        }

        Builder& sending_time(std::string_view v) noexcept {
            sending_time_ = v;
            return *this;
        }

        Builder& begin_seq_no(uint32_t v) noexcept {
            begin_seq_no_ = v;
            return *this;
        }

        Builder& end_seq_no(uint32_t v) noexcept {
            end_seq_no_ = v;
            return *this;
        }

        [[nodiscard]] std::span<const char> build(MessageAssembler& asm_) const noexcept {
            asm_.start()
                .field(tag::MsgType::value, MSG_TYPE)
                .field(tag::SenderCompID::value, sender_comp_id_)
                .field(tag::TargetCompID::value, target_comp_id_)
                .field(tag::MsgSeqNum::value, static_cast<int64_t>(msg_seq_num_))
                .field(tag::SendingTime::value, sending_time_)
                .field(7, static_cast<int64_t>(begin_seq_no_))   // BeginSeqNo
                .field(16, static_cast<int64_t>(end_seq_no_));   // EndSeqNo

            return asm_.finish();
        }

    private:
        std::string_view sender_comp_id_;
        std::string_view target_comp_id_;
        uint32_t msg_seq_num_{1};
        std::string_view sending_time_;
        uint32_t begin_seq_no_{1};
        uint32_t end_seq_no_{0};
    };
};

// ============================================================================
// SequenceReset Message (MsgType = 4)
// ============================================================================

/// FIX 4.4 SequenceReset message (35=4)
struct SequenceReset {
    static constexpr char MSG_TYPE = msg_type::SequenceReset;

    FixHeader header;
    uint32_t new_seq_no;    // Tag 36 - Required
    bool gap_fill_flag;     // Tag 123 - Optional
    std::span<const char> raw_data;

    constexpr SequenceReset() noexcept
        : header{}, new_seq_no{0}, gap_fill_flag{false}, raw_data{} {}

    [[nodiscard]] constexpr std::span<const char> raw() const noexcept {
        return raw_data;
    }

    [[nodiscard]] constexpr uint32_t msg_seq_num() const noexcept {
        return header.msg_seq_num;
    }

    [[nodiscard]] constexpr std::string_view sender_comp_id() const noexcept {
        return header.sender_comp_id;
    }

    [[nodiscard]] constexpr std::string_view target_comp_id() const noexcept {
        return header.target_comp_id;
    }

    [[nodiscard]] constexpr std::string_view sending_time() const noexcept {
        return header.sending_time;
    }

    [[nodiscard]] static ParseResult<SequenceReset> from_buffer(
        std::span<const char> buffer) noexcept
    {
        auto parsed = IndexedParser::parse(buffer);
        return parsed.and_then([buffer](IndexedParser& p) -> ParseResult<SequenceReset> {
            if (p.msg_type() != MSG_TYPE) [[unlikely]] {
                return std::unexpected{ParseError{ParseErrorCode::InvalidMsgType}};
            }

            SequenceReset msg;
            msg.raw_data = buffer;
            msg.header.begin_string = p.get_string(tag::BeginString::value);
            msg.header.msg_type = p.msg_type();
            msg.header.sender_comp_id = p.sender_comp_id();
            msg.header.target_comp_id = p.target_comp_id();
            msg.header.msg_seq_num = p.msg_seq_num();
            msg.header.sending_time = p.sending_time();

            // Use require_field for required NewSeqNo
            auto seq_result = require_field(to_uint32(p.get_int(36)), 36);  // NewSeqNo
            if (!seq_result) [[unlikely]] return std::unexpected{seq_result.error()};
            msg.new_seq_no = *seq_result;

            msg.gap_fill_flag = p.get_char(123) == 'Y';  // GapFillFlag

            return msg;
        });
    }

    class Builder {
    public:
        Builder& sender_comp_id(std::string_view v) noexcept {
            sender_comp_id_ = v;
            return *this;
        }

        Builder& target_comp_id(std::string_view v) noexcept {
            target_comp_id_ = v;
            return *this;
        }

        Builder& msg_seq_num(uint32_t v) noexcept {
            msg_seq_num_ = v;
            return *this;
        }

        Builder& sending_time(std::string_view v) noexcept {
            sending_time_ = v;
            return *this;
        }

        Builder& new_seq_no(uint32_t v) noexcept {
            new_seq_no_ = v;
            return *this;
        }

        Builder& gap_fill_flag(bool v) noexcept {
            gap_fill_flag_ = v;
            return *this;
        }

        [[nodiscard]] std::span<const char> build(MessageAssembler& asm_) const noexcept {
            asm_.start()
                .field(tag::MsgType::value, MSG_TYPE)
                .field(tag::SenderCompID::value, sender_comp_id_)
                .field(tag::TargetCompID::value, target_comp_id_)
                .field(tag::MsgSeqNum::value, static_cast<int64_t>(msg_seq_num_))
                .field(tag::SendingTime::value, sending_time_)
                .field(36, static_cast<int64_t>(new_seq_no_));  // NewSeqNo

            if (gap_fill_flag_) {
                asm_.field(123, 'Y');  // GapFillFlag
            }

            return asm_.finish();
        }

    private:
        std::string_view sender_comp_id_;
        std::string_view target_comp_id_;
        uint32_t msg_seq_num_{1};
        std::string_view sending_time_;
        uint32_t new_seq_no_{1};
        bool gap_fill_flag_{false};
    };
};

// ============================================================================
// Reject Message (MsgType = 3)
// ============================================================================

/// FIX 4.4 Reject message (35=3)
struct Reject {
    static constexpr char MSG_TYPE = msg_type::Reject;

    FixHeader header;
    uint32_t ref_seq_num;            // Tag 45 - Required
    int ref_tag_id;                  // Tag 371 - Optional
    int session_reject_reason;       // Tag 373 - Optional
    std::string_view text;           // Tag 58 - Optional
    std::span<const char> raw_data;

    constexpr Reject() noexcept
        : header{}
        , ref_seq_num{0}
        , ref_tag_id{0}
        , session_reject_reason{0}
        , text{}
        , raw_data{} {}

    [[nodiscard]] constexpr std::span<const char> raw() const noexcept {
        return raw_data;
    }

    [[nodiscard]] constexpr uint32_t msg_seq_num() const noexcept {
        return header.msg_seq_num;
    }

    [[nodiscard]] constexpr std::string_view sender_comp_id() const noexcept {
        return header.sender_comp_id;
    }

    [[nodiscard]] constexpr std::string_view target_comp_id() const noexcept {
        return header.target_comp_id;
    }

    [[nodiscard]] constexpr std::string_view sending_time() const noexcept {
        return header.sending_time;
    }

    [[nodiscard]] static ParseResult<Reject> from_buffer(
        std::span<const char> buffer) noexcept
    {
        auto parsed = IndexedParser::parse(buffer);
        return parsed.and_then([buffer](IndexedParser& p) -> ParseResult<Reject> {
            if (p.msg_type() != MSG_TYPE) [[unlikely]] {
                return std::unexpected{ParseError{ParseErrorCode::InvalidMsgType}};
            }

            Reject msg;
            msg.raw_data = buffer;
            msg.header.begin_string = p.get_string(tag::BeginString::value);
            msg.header.msg_type = p.msg_type();
            msg.header.sender_comp_id = p.sender_comp_id();
            msg.header.target_comp_id = p.target_comp_id();
            msg.header.msg_seq_num = p.msg_seq_num();
            msg.header.sending_time = p.sending_time();

            // Use require_field for required RefSeqNum
            auto ref_result = require_field(to_uint32(p.get_int(tag::RefSeqNum::value)), tag::RefSeqNum::value);
            if (!ref_result) [[unlikely]] return std::unexpected{ref_result.error()};
            msg.ref_seq_num = *ref_result;

            // Optional fields with monadic extraction
            if_has_value(to_int(p.get_int(371)),  // RefTagID
                [&msg](int v) { msg.ref_tag_id = v; });
            if_has_value(to_int(p.get_int(373)),  // SessionRejectReason
                [&msg](int v) { msg.session_reject_reason = v; });

            msg.text = p.get_string(tag::Text::value);

            return msg;
        });
    }

    class Builder {
    public:
        Builder& sender_comp_id(std::string_view v) noexcept {
            sender_comp_id_ = v;
            return *this;
        }

        Builder& target_comp_id(std::string_view v) noexcept {
            target_comp_id_ = v;
            return *this;
        }

        Builder& msg_seq_num(uint32_t v) noexcept {
            msg_seq_num_ = v;
            return *this;
        }

        Builder& sending_time(std::string_view v) noexcept {
            sending_time_ = v;
            return *this;
        }

        Builder& ref_seq_num(uint32_t v) noexcept {
            ref_seq_num_ = v;
            return *this;
        }

        Builder& ref_tag_id(int v) noexcept {
            ref_tag_id_ = v;
            return *this;
        }

        Builder& session_reject_reason(int v) noexcept {
            session_reject_reason_ = v;
            return *this;
        }

        Builder& text(std::string_view v) noexcept {
            text_ = v;
            return *this;
        }

        [[nodiscard]] std::span<const char> build(MessageAssembler& asm_) const noexcept {
            asm_.start()
                .field(tag::MsgType::value, MSG_TYPE)
                .field(tag::SenderCompID::value, sender_comp_id_)
                .field(tag::TargetCompID::value, target_comp_id_)
                .field(tag::MsgSeqNum::value, static_cast<int64_t>(msg_seq_num_))
                .field(tag::SendingTime::value, sending_time_)
                .field(tag::RefSeqNum::value, static_cast<int64_t>(ref_seq_num_));

            if (ref_tag_id_ > 0) {
                asm_.field(371, static_cast<int64_t>(ref_tag_id_));
            }

            if (session_reject_reason_ > 0) {
                asm_.field(373, static_cast<int64_t>(session_reject_reason_));
            }

            if (!text_.empty()) {
                asm_.field(tag::Text::value, text_);
            }

            return asm_.finish();
        }

    private:
        std::string_view sender_comp_id_;
        std::string_view target_comp_id_;
        uint32_t msg_seq_num_{1};
        std::string_view sending_time_;
        uint32_t ref_seq_num_{1};
        int ref_tag_id_{0};
        int session_reject_reason_{0};
        std::string_view text_;
    };
};

} // namespace nfx::fix44
