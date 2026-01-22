#pragma once

#include <span>
#include <string_view>

#include "nexusfix/types/tag.hpp"
#include "nexusfix/types/field_types.hpp"
#include "nexusfix/types/error.hpp"
#include "nexusfix/types/fix_version.hpp"
#include "nexusfix/interfaces/i_message.hpp"
#include "nexusfix/parser/field_view.hpp"
#include "nexusfix/parser/runtime_parser.hpp"
#include "nexusfix/messages/common/header.hpp"

namespace nfx::fixt11 {

// ============================================================================
// FIXT 1.1 Heartbeat Message (MsgType = 0)
// ============================================================================

struct Heartbeat {
    static constexpr char MSG_TYPE = msg_type::Heartbeat;
    static constexpr std::string_view BEGIN_STRING = fix_version::FIXT_1_1;

    FixHeader header;
    std::string_view test_req_id;  // Tag 112 - Conditional (required if responding to TestRequest)
    std::span<const char> raw_data;

    constexpr Heartbeat() noexcept : header{}, test_req_id{}, raw_data{} {}

    [[nodiscard]] constexpr std::span<const char> raw() const noexcept { return raw_data; }
    [[nodiscard]] constexpr uint32_t msg_seq_num() const noexcept { return header.msg_seq_num; }
    [[nodiscard]] constexpr std::string_view sender_comp_id() const noexcept { return header.sender_comp_id; }
    [[nodiscard]] constexpr std::string_view target_comp_id() const noexcept { return header.target_comp_id; }
    [[nodiscard]] constexpr std::string_view sending_time() const noexcept { return header.sending_time; }

    [[nodiscard]] static ParseResult<Heartbeat> from_buffer(std::span<const char> buffer) noexcept {
        auto parsed = IndexedParser::parse(buffer);
        if (!parsed.has_value()) return std::unexpected{parsed.error()};

        auto& p = *parsed;
        if (p.msg_type() != MSG_TYPE) {
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
    }

    class Builder {
    public:
        Builder& sender_comp_id(std::string_view v) noexcept { sender_comp_id_ = v; return *this; }
        Builder& target_comp_id(std::string_view v) noexcept { target_comp_id_ = v; return *this; }
        Builder& msg_seq_num(uint32_t v) noexcept { msg_seq_num_ = v; return *this; }
        Builder& sending_time(std::string_view v) noexcept { sending_time_ = v; return *this; }
        Builder& test_req_id(std::string_view v) noexcept { test_req_id_ = v; return *this; }

        [[nodiscard]] std::span<const char> build(MessageAssembler& asm_) const noexcept {
            asm_.start_fixt11()
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
// FIXT 1.1 TestRequest Message (MsgType = 1)
// ============================================================================

struct TestRequest {
    static constexpr char MSG_TYPE = msg_type::TestRequest;
    static constexpr std::string_view BEGIN_STRING = fix_version::FIXT_1_1;

    FixHeader header;
    std::string_view test_req_id;  // Tag 112 - Required
    std::span<const char> raw_data;

    constexpr TestRequest() noexcept : header{}, test_req_id{}, raw_data{} {}

    [[nodiscard]] constexpr std::span<const char> raw() const noexcept { return raw_data; }
    [[nodiscard]] constexpr uint32_t msg_seq_num() const noexcept { return header.msg_seq_num; }
    [[nodiscard]] constexpr std::string_view sender_comp_id() const noexcept { return header.sender_comp_id; }
    [[nodiscard]] constexpr std::string_view target_comp_id() const noexcept { return header.target_comp_id; }
    [[nodiscard]] constexpr std::string_view sending_time() const noexcept { return header.sending_time; }

    [[nodiscard]] static ParseResult<TestRequest> from_buffer(std::span<const char> buffer) noexcept {
        auto parsed = IndexedParser::parse(buffer);
        if (!parsed.has_value()) return std::unexpected{parsed.error()};

        auto& p = *parsed;
        if (p.msg_type() != MSG_TYPE) {
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

        return msg;
    }

    class Builder {
    public:
        Builder& sender_comp_id(std::string_view v) noexcept { sender_comp_id_ = v; return *this; }
        Builder& target_comp_id(std::string_view v) noexcept { target_comp_id_ = v; return *this; }
        Builder& msg_seq_num(uint32_t v) noexcept { msg_seq_num_ = v; return *this; }
        Builder& sending_time(std::string_view v) noexcept { sending_time_ = v; return *this; }
        Builder& test_req_id(std::string_view v) noexcept { test_req_id_ = v; return *this; }

        [[nodiscard]] std::span<const char> build(MessageAssembler& asm_) const noexcept {
            asm_.start_fixt11()
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
// FIXT 1.1 ResendRequest Message (MsgType = 2)
// ============================================================================

struct ResendRequest {
    static constexpr char MSG_TYPE = msg_type::ResendRequest;
    static constexpr std::string_view BEGIN_STRING = fix_version::FIXT_1_1;

    FixHeader header;
    uint32_t begin_seq_no;  // Tag 7 - Required
    uint32_t end_seq_no;    // Tag 16 - Required (0 = infinity)
    std::span<const char> raw_data;

    constexpr ResendRequest() noexcept : header{}, begin_seq_no{0}, end_seq_no{0}, raw_data{} {}

    [[nodiscard]] constexpr std::span<const char> raw() const noexcept { return raw_data; }
    [[nodiscard]] constexpr uint32_t msg_seq_num() const noexcept { return header.msg_seq_num; }
    [[nodiscard]] constexpr std::string_view sender_comp_id() const noexcept { return header.sender_comp_id; }
    [[nodiscard]] constexpr std::string_view target_comp_id() const noexcept { return header.target_comp_id; }
    [[nodiscard]] constexpr std::string_view sending_time() const noexcept { return header.sending_time; }

    [[nodiscard]] static ParseResult<ResendRequest> from_buffer(std::span<const char> buffer) noexcept {
        auto parsed = IndexedParser::parse(buffer);
        if (!parsed.has_value()) return std::unexpected{parsed.error()};

        auto& p = *parsed;
        if (p.msg_type() != MSG_TYPE) {
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

        if (auto v = p.get_int(tag::BeginSeqNo::value)) {
            msg.begin_seq_no = static_cast<uint32_t>(*v);
        }
        if (auto v = p.get_int(tag::EndSeqNo::value)) {
            msg.end_seq_no = static_cast<uint32_t>(*v);
        }

        return msg;
    }

    class Builder {
    public:
        Builder& sender_comp_id(std::string_view v) noexcept { sender_comp_id_ = v; return *this; }
        Builder& target_comp_id(std::string_view v) noexcept { target_comp_id_ = v; return *this; }
        Builder& msg_seq_num(uint32_t v) noexcept { msg_seq_num_ = v; return *this; }
        Builder& sending_time(std::string_view v) noexcept { sending_time_ = v; return *this; }
        Builder& begin_seq_no(uint32_t v) noexcept { begin_seq_no_ = v; return *this; }
        Builder& end_seq_no(uint32_t v) noexcept { end_seq_no_ = v; return *this; }

        [[nodiscard]] std::span<const char> build(MessageAssembler& asm_) const noexcept {
            asm_.start_fixt11()
                .field(tag::MsgType::value, MSG_TYPE)
                .field(tag::SenderCompID::value, sender_comp_id_)
                .field(tag::TargetCompID::value, target_comp_id_)
                .field(tag::MsgSeqNum::value, static_cast<int64_t>(msg_seq_num_))
                .field(tag::SendingTime::value, sending_time_)
                .field(tag::BeginSeqNo::value, static_cast<int64_t>(begin_seq_no_))
                .field(tag::EndSeqNo::value, static_cast<int64_t>(end_seq_no_));

            return asm_.finish();
        }

    private:
        std::string_view sender_comp_id_;
        std::string_view target_comp_id_;
        uint32_t msg_seq_num_{1};
        std::string_view sending_time_;
        uint32_t begin_seq_no_{0};
        uint32_t end_seq_no_{0};
    };
};

// ============================================================================
// FIXT 1.1 SequenceReset Message (MsgType = 4)
// ============================================================================

struct SequenceReset {
    static constexpr char MSG_TYPE = msg_type::SequenceReset;
    static constexpr std::string_view BEGIN_STRING = fix_version::FIXT_1_1;

    FixHeader header;
    uint32_t new_seq_no;   // Tag 36 - Required
    bool gap_fill_flag;    // Tag 123 - Optional (Y = gap fill mode)
    std::span<const char> raw_data;

    constexpr SequenceReset() noexcept : header{}, new_seq_no{0}, gap_fill_flag{false}, raw_data{} {}

    [[nodiscard]] constexpr std::span<const char> raw() const noexcept { return raw_data; }
    [[nodiscard]] constexpr uint32_t msg_seq_num() const noexcept { return header.msg_seq_num; }
    [[nodiscard]] constexpr std::string_view sender_comp_id() const noexcept { return header.sender_comp_id; }
    [[nodiscard]] constexpr std::string_view target_comp_id() const noexcept { return header.target_comp_id; }
    [[nodiscard]] constexpr std::string_view sending_time() const noexcept { return header.sending_time; }

    [[nodiscard]] static ParseResult<SequenceReset> from_buffer(std::span<const char> buffer) noexcept {
        auto parsed = IndexedParser::parse(buffer);
        if (!parsed.has_value()) return std::unexpected{parsed.error()};

        auto& p = *parsed;
        if (p.msg_type() != MSG_TYPE) {
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

        if (auto v = p.get_int(tag::NewSeqNo::value)) {
            msg.new_seq_no = static_cast<uint32_t>(*v);
        }
        msg.gap_fill_flag = p.get_char(tag::GapFillFlag::value) == 'Y';

        return msg;
    }

    class Builder {
    public:
        Builder& sender_comp_id(std::string_view v) noexcept { sender_comp_id_ = v; return *this; }
        Builder& target_comp_id(std::string_view v) noexcept { target_comp_id_ = v; return *this; }
        Builder& msg_seq_num(uint32_t v) noexcept { msg_seq_num_ = v; return *this; }
        Builder& sending_time(std::string_view v) noexcept { sending_time_ = v; return *this; }
        Builder& new_seq_no(uint32_t v) noexcept { new_seq_no_ = v; return *this; }
        Builder& gap_fill_flag(bool v) noexcept { gap_fill_flag_ = v; return *this; }

        [[nodiscard]] std::span<const char> build(MessageAssembler& asm_) const noexcept {
            asm_.start_fixt11()
                .field(tag::MsgType::value, MSG_TYPE)
                .field(tag::SenderCompID::value, sender_comp_id_)
                .field(tag::TargetCompID::value, target_comp_id_)
                .field(tag::MsgSeqNum::value, static_cast<int64_t>(msg_seq_num_))
                .field(tag::SendingTime::value, sending_time_)
                .field(tag::NewSeqNo::value, static_cast<int64_t>(new_seq_no_));

            if (gap_fill_flag_) {
                asm_.field(tag::GapFillFlag::value, 'Y');
            }

            return asm_.finish();
        }

    private:
        std::string_view sender_comp_id_;
        std::string_view target_comp_id_;
        uint32_t msg_seq_num_{1};
        std::string_view sending_time_;
        uint32_t new_seq_no_{0};
        bool gap_fill_flag_{false};
    };
};

// ============================================================================
// FIXT 1.1 Reject Message (MsgType = 3)
// ============================================================================

struct Reject {
    static constexpr char MSG_TYPE = msg_type::Reject;
    static constexpr std::string_view BEGIN_STRING = fix_version::FIXT_1_1;

    FixHeader header;
    uint32_t ref_seq_num;       // Tag 45 - Required
    int ref_tag_id;             // Tag 371 - Optional
    int session_reject_reason;  // Tag 373 - Optional
    std::string_view text;      // Tag 58 - Optional
    std::span<const char> raw_data;

    constexpr Reject() noexcept
        : header{}, ref_seq_num{0}, ref_tag_id{0}, session_reject_reason{0}, text{}, raw_data{} {}

    [[nodiscard]] constexpr std::span<const char> raw() const noexcept { return raw_data; }
    [[nodiscard]] constexpr uint32_t msg_seq_num() const noexcept { return header.msg_seq_num; }
    [[nodiscard]] constexpr std::string_view sender_comp_id() const noexcept { return header.sender_comp_id; }
    [[nodiscard]] constexpr std::string_view target_comp_id() const noexcept { return header.target_comp_id; }
    [[nodiscard]] constexpr std::string_view sending_time() const noexcept { return header.sending_time; }

    [[nodiscard]] static ParseResult<Reject> from_buffer(std::span<const char> buffer) noexcept {
        auto parsed = IndexedParser::parse(buffer);
        if (!parsed.has_value()) return std::unexpected{parsed.error()};

        auto& p = *parsed;
        if (p.msg_type() != MSG_TYPE) {
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

        if (auto v = p.get_int(tag::RefSeqNum::value)) {
            msg.ref_seq_num = static_cast<uint32_t>(*v);
        }
        if (auto v = p.get_int(371)) {  // RefTagID
            msg.ref_tag_id = static_cast<int>(*v);
        }
        if (auto v = p.get_int(373)) {  // SessionRejectReason
            msg.session_reject_reason = static_cast<int>(*v);
        }
        msg.text = p.get_string(tag::Text::value);

        return msg;
    }

    class Builder {
    public:
        Builder& sender_comp_id(std::string_view v) noexcept { sender_comp_id_ = v; return *this; }
        Builder& target_comp_id(std::string_view v) noexcept { target_comp_id_ = v; return *this; }
        Builder& msg_seq_num(uint32_t v) noexcept { msg_seq_num_ = v; return *this; }
        Builder& sending_time(std::string_view v) noexcept { sending_time_ = v; return *this; }
        Builder& ref_seq_num(uint32_t v) noexcept { ref_seq_num_ = v; return *this; }
        Builder& ref_tag_id(int v) noexcept { ref_tag_id_ = v; return *this; }
        Builder& session_reject_reason(int v) noexcept { session_reject_reason_ = v; return *this; }
        Builder& text(std::string_view v) noexcept { text_ = v; return *this; }

        [[nodiscard]] std::span<const char> build(MessageAssembler& asm_) const noexcept {
            asm_.start_fixt11()
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
        uint32_t ref_seq_num_{0};
        int ref_tag_id_{0};
        int session_reject_reason_{0};
        std::string_view text_;
    };
};

} // namespace nfx::fixt11
