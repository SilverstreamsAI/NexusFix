#pragma once

#include <span>
#include <string_view>
#include <optional>

#include "nexusfix/types/tag.hpp"
#include "nexusfix/types/field_types.hpp"
#include "nexusfix/types/error.hpp"
#include "nexusfix/types/fix_version.hpp"
#include "nexusfix/interfaces/i_message.hpp"
#include "nexusfix/parser/field_view.hpp"
#include "nexusfix/parser/runtime_parser.hpp"
#include "nexusfix/messages/common/header.hpp"
#include "nexusfix/messages/common/trailer.hpp"

namespace nfx::fixt11 {

// ============================================================================
// FIXT 1.1 Logon Message (MsgType = A)
// ============================================================================

/// FIXT 1.1 Logon message (35=A)
/// Used to establish a FIX 5.0+ session
/// Key difference from FIX 4.4: BeginString = "FIXT.1.1" and DefaultApplVerID required
struct Logon {
    static constexpr char MSG_TYPE = msg_type::Logon;
    static constexpr std::string_view BEGIN_STRING = fix_version::FIXT_1_1;

    // Header fields
    FixHeader header;

    // Body fields
    int encrypt_method;                    // Tag 98 - Required (0=None)
    int heart_bt_int;                      // Tag 108 - Required (heartbeat interval in seconds)
    bool reset_seq_num_flag;               // Tag 141 - Optional
    std::string_view username;             // Tag 553 - Optional
    std::string_view password;             // Tag 554 - Optional
    char default_appl_ver_id;              // Tag 1137 - Required for FIXT 1.1 (e.g., '7' for FIX 5.0)
    std::string_view default_cstm_appl_ver_id; // Tag 1408 - Optional

    // Raw buffer reference
    std::span<const char> raw_data;

    constexpr Logon() noexcept
        : header{}
        , encrypt_method{0}
        , heart_bt_int{30}
        , reset_seq_num_flag{false}
        , username{}
        , password{}
        , default_appl_ver_id{appl_ver_id::FIX_5_0}  // Default to FIX 5.0
        , default_cstm_appl_ver_id{}
        , raw_data{} {}

    // ========================================================================
    // Message Concept Implementation
    // ========================================================================

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

    /// Get application version as string
    [[nodiscard]] constexpr std::string_view appl_ver_string() const noexcept {
        return appl_ver_id::to_string(default_appl_ver_id);
    }

    // ========================================================================
    // Parsing
    // ========================================================================

    /// Parse Logon from raw buffer (zero-copy)
    [[nodiscard]] static ParseResult<Logon> from_buffer(
        std::span<const char> buffer) noexcept
    {
        auto parsed = IndexedParser::parse(buffer);
        if (!parsed.has_value()) {
            return std::unexpected{parsed.error()};
        }

        auto& p = *parsed;

        // Verify message type
        if (p.msg_type() != MSG_TYPE) {
            return std::unexpected{ParseError{ParseErrorCode::InvalidMsgType, tag::MsgType::value}};
        }

        // Verify FIXT.1.1 protocol
        auto begin_str = p.get_string(tag::BeginString::value);
        if (begin_str != BEGIN_STRING) {
            return std::unexpected{ParseError{ParseErrorCode::InvalidFieldFormat, tag::BeginString::value}};
        }

        Logon msg;
        msg.raw_data = buffer;

        // Parse header
        msg.header.begin_string = begin_str;
        if (auto v = p.get_int(tag::BodyLength::value)) {
            msg.header.body_length = static_cast<int>(*v);
        }
        msg.header.msg_type = p.msg_type();
        msg.header.sender_comp_id = p.sender_comp_id();
        msg.header.target_comp_id = p.target_comp_id();
        msg.header.msg_seq_num = p.msg_seq_num();
        msg.header.sending_time = p.sending_time();
        msg.header.poss_dup_flag = p.get_char(tag::PossDupFlag::value) == 'Y';

        // Parse body fields
        if (auto v = p.get_int(98)) {  // EncryptMethod
            msg.encrypt_method = static_cast<int>(*v);
        } else {
            return std::unexpected{ParseError{ParseErrorCode::MissingRequiredField, 98}};
        }

        if (auto v = p.get_int(108)) {  // HeartBtInt
            msg.heart_bt_int = static_cast<int>(*v);
        } else {
            return std::unexpected{ParseError{ParseErrorCode::MissingRequiredField, 108}};
        }

        // DefaultApplVerID is required for FIXT 1.1
        char appl_ver = p.get_char(tag::DefaultApplVerID::value);
        if (appl_ver == '\0') {
            return std::unexpected{ParseError{ParseErrorCode::MissingRequiredField, tag::DefaultApplVerID::value}};
        }
        msg.default_appl_ver_id = appl_ver;

        msg.reset_seq_num_flag = p.get_char(141) == 'Y';  // ResetSeqNumFlag
        msg.username = p.get_string(553);  // Username
        msg.password = p.get_string(554);  // Password
        msg.default_cstm_appl_ver_id = p.get_string(tag::DefaultCstmApplVerID::value);

        return msg;
    }

    // ========================================================================
    // Building
    // ========================================================================

    /// Builder for constructing FIXT 1.1 Logon messages
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

        Builder& encrypt_method(int v) noexcept {
            encrypt_method_ = v;
            return *this;
        }

        Builder& heart_bt_int(int v) noexcept {
            heart_bt_int_ = v;
            return *this;
        }

        Builder& reset_seq_num_flag(bool v) noexcept {
            reset_seq_num_flag_ = v;
            return *this;
        }

        Builder& username(std::string_view v) noexcept {
            username_ = v;
            return *this;
        }

        Builder& password(std::string_view v) noexcept {
            password_ = v;
            return *this;
        }

        /// Set application version (required for FIXT 1.1)
        Builder& default_appl_ver_id(char v) noexcept {
            default_appl_ver_id_ = v;
            return *this;
        }

        /// Convenience: set to FIX 5.0
        Builder& use_fix50() noexcept {
            default_appl_ver_id_ = appl_ver_id::FIX_5_0;
            return *this;
        }

        /// Convenience: set to FIX 5.0 SP1
        Builder& use_fix50_sp1() noexcept {
            default_appl_ver_id_ = appl_ver_id::FIX_5_0_SP1;
            return *this;
        }

        /// Convenience: set to FIX 5.0 SP2
        Builder& use_fix50_sp2() noexcept {
            default_appl_ver_id_ = appl_ver_id::FIX_5_0_SP2;
            return *this;
        }

        /// Build the message into provided buffer
        [[nodiscard]] std::span<const char> build(MessageAssembler& asm_) const noexcept {
            asm_.start_fixt11()  // Use FIXT.1.1 BeginString
                .field(tag::MsgType::value, MSG_TYPE)
                .field(tag::SenderCompID::value, sender_comp_id_)
                .field(tag::TargetCompID::value, target_comp_id_)
                .field(tag::MsgSeqNum::value, static_cast<int64_t>(msg_seq_num_))
                .field(tag::SendingTime::value, sending_time_)
                .field(98, static_cast<int64_t>(encrypt_method_))   // EncryptMethod
                .field(108, static_cast<int64_t>(heart_bt_int_))    // HeartBtInt
                .field(tag::DefaultApplVerID::value, default_appl_ver_id_);  // Required

            if (reset_seq_num_flag_) {
                asm_.field(141, 'Y');  // ResetSeqNumFlag
            }

            if (!username_.empty()) {
                asm_.field(553, username_);  // Username
            }

            if (!password_.empty()) {
                asm_.field(554, password_);  // Password
            }

            return asm_.finish();
        }

    private:
        std::string_view sender_comp_id_;
        std::string_view target_comp_id_;
        uint32_t msg_seq_num_{1};
        std::string_view sending_time_;
        int encrypt_method_{0};
        int heart_bt_int_{30};
        bool reset_seq_num_flag_{false};
        std::string_view username_;
        std::string_view password_;
        char default_appl_ver_id_{appl_ver_id::FIX_5_0};
    };
};

// ============================================================================
// FIXT 1.1 Logout Message (MsgType = 5)
// ============================================================================

/// FIXT 1.1 Logout message (35=5)
struct Logout {
    static constexpr char MSG_TYPE = msg_type::Logout;
    static constexpr std::string_view BEGIN_STRING = fix_version::FIXT_1_1;

    FixHeader header;
    std::string_view text;  // Tag 58 - Optional
    std::span<const char> raw_data;

    constexpr Logout() noexcept : header{}, text{}, raw_data{} {}

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

    [[nodiscard]] static ParseResult<Logout> from_buffer(
        std::span<const char> buffer) noexcept
    {
        auto parsed = IndexedParser::parse(buffer);
        if (!parsed.has_value()) {
            return std::unexpected{parsed.error()};
        }

        auto& p = *parsed;

        if (p.msg_type() != MSG_TYPE) {
            return std::unexpected{ParseError{ParseErrorCode::InvalidMsgType}};
        }

        Logout msg;
        msg.raw_data = buffer;
        msg.header.begin_string = p.get_string(tag::BeginString::value);
        msg.header.msg_type = p.msg_type();
        msg.header.sender_comp_id = p.sender_comp_id();
        msg.header.target_comp_id = p.target_comp_id();
        msg.header.msg_seq_num = p.msg_seq_num();
        msg.header.sending_time = p.sending_time();
        msg.text = p.get_string(tag::Text::value);

        return msg;
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

        Builder& text(std::string_view v) noexcept {
            text_ = v;
            return *this;
        }

        [[nodiscard]] std::span<const char> build(MessageAssembler& asm_) const noexcept {
            asm_.start_fixt11()
                .field(tag::MsgType::value, MSG_TYPE)
                .field(tag::SenderCompID::value, sender_comp_id_)
                .field(tag::TargetCompID::value, target_comp_id_)
                .field(tag::MsgSeqNum::value, static_cast<int64_t>(msg_seq_num_))
                .field(tag::SendingTime::value, sending_time_);

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
        std::string_view text_;
    };
};

} // namespace nfx::fixt11
