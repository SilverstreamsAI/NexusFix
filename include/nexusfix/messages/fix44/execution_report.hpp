#pragma once

#include <span>
#include <string_view>
#include <optional>

#include "nexusfix/types/tag.hpp"
#include "nexusfix/types/field_types.hpp"
#include "nexusfix/types/error.hpp"
#include "nexusfix/interfaces/i_message.hpp"
#include "nexusfix/parser/runtime_parser.hpp"
#include "nexusfix/messages/common/header.hpp"
#include "nexusfix/messages/common/trailer.hpp"

namespace nfx::fix44 {

// ============================================================================
// ExecutionReport Message (MsgType = 8)
// ============================================================================

/// FIX 4.4 ExecutionReport message (35=8)
/// Confirms order status, fills, and rejections
struct ExecutionReport {
    static constexpr char MSG_TYPE = msg_type::ExecutionReport;

    // Header
    FixHeader header;

    // Required body fields
    std::string_view order_id;       // Tag 37 - Required
    std::string_view exec_id;        // Tag 17 - Required
    ExecType exec_type;              // Tag 150 - Required
    OrdStatus ord_status;            // Tag 39 - Required
    std::string_view symbol;         // Tag 55 - Required
    Side side;                       // Tag 54 - Required
    Qty leaves_qty;                  // Tag 151 - Required
    Qty cum_qty;                     // Tag 14 - Required
    FixedPrice avg_px;               // Tag 6 - Required

    // Conditional/Optional body fields
    std::string_view cl_ord_id;      // Tag 11 - Conditional
    std::string_view orig_cl_ord_id; // Tag 41 - Conditional
    std::string_view exec_ref_id;    // Tag 19 - Conditional
    Qty order_qty;                   // Tag 38 - Conditional
    OrdType ord_type;                // Tag 40 - Conditional
    FixedPrice price;                // Tag 44 - Conditional
    FixedPrice stop_px;              // Tag 99 - Conditional
    TimeInForce time_in_force;       // Tag 59 - Conditional
    FixedPrice last_px;              // Tag 31 - Conditional (fill)
    Qty last_qty;                    // Tag 32 - Conditional (fill)
    std::string_view text;           // Tag 58 - Optional
    int ord_rej_reason;              // Tag 103 - Conditional (reject)
    std::string_view account;        // Tag 1 - Optional
    std::string_view transact_time;  // Tag 60 - Conditional

    // Raw buffer reference
    std::span<const char> raw_data;

    constexpr ExecutionReport() noexcept
        : header{}
        , order_id{}
        , exec_id{}
        , exec_type{ExecType::New}
        , ord_status{OrdStatus::New}
        , symbol{}
        , side{Side::Buy}
        , leaves_qty{}
        , cum_qty{}
        , avg_px{}
        , cl_ord_id{}
        , orig_cl_ord_id{}
        , exec_ref_id{}
        , order_qty{}
        , ord_type{OrdType::Market}
        , price{}
        , stop_px{}
        , time_in_force{TimeInForce::Day}
        , last_px{}
        , last_qty{}
        , text{}
        , ord_rej_reason{0}
        , account{}
        , transact_time{}
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

    // ========================================================================
    // Convenience Methods
    // ========================================================================

    /// Check if this is a fill execution
    [[nodiscard]] constexpr bool is_fill() const noexcept {
        return exec_type == ExecType::Fill ||
               exec_type == ExecType::PartialFill ||
               exec_type == ExecType::Trade;
    }

    /// Check if order is terminal (no more updates expected)
    [[nodiscard]] constexpr bool is_terminal() const noexcept {
        return is_terminal_status(ord_status);
    }

    /// Check if this is a rejection
    [[nodiscard]] constexpr bool is_rejected() const noexcept {
        return exec_type == ExecType::Rejected ||
               ord_status == OrdStatus::Rejected;
    }

    /// Get fill value (last_px * last_qty)
    [[nodiscard]] constexpr FixedPrice fill_value() const noexcept {
        return FixedPrice{(last_px.raw * last_qty.raw) / Qty::SCALE};
    }

    // ========================================================================
    // Parsing
    // ========================================================================

    [[nodiscard]] static ParseResult<ExecutionReport> from_buffer(
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

        ExecutionReport msg;
        msg.raw_data = buffer;

        // Parse header
        msg.header.begin_string = p.get_string(tag::BeginString::value);
        if (auto v = p.get_int(tag::BodyLength::value)) {
            msg.header.body_length = static_cast<int>(*v);
        }
        msg.header.msg_type = p.msg_type();
        msg.header.sender_comp_id = p.sender_comp_id();
        msg.header.target_comp_id = p.target_comp_id();
        msg.header.msg_seq_num = p.msg_seq_num();
        msg.header.sending_time = p.sending_time();

        // Parse required body fields
        msg.order_id = p.get_string(tag::OrderID::value);
        if (msg.order_id.empty()) {
            return std::unexpected{ParseError{ParseErrorCode::MissingRequiredField, tag::OrderID::value}};
        }

        msg.exec_id = p.get_string(tag::ExecID::value);
        if (msg.exec_id.empty()) {
            return std::unexpected{ParseError{ParseErrorCode::MissingRequiredField, tag::ExecID::value}};
        }

        char exec_type_char = p.get_char(tag::ExecType::value);
        if (exec_type_char == '\0') {
            return std::unexpected{ParseError{ParseErrorCode::MissingRequiredField, tag::ExecType::value}};
        }
        msg.exec_type = static_cast<ExecType>(exec_type_char);

        char ord_status_char = p.get_char(tag::OrdStatus::value);
        if (ord_status_char == '\0') {
            return std::unexpected{ParseError{ParseErrorCode::MissingRequiredField, tag::OrdStatus::value}};
        }
        msg.ord_status = static_cast<OrdStatus>(ord_status_char);

        msg.symbol = p.get_string(tag::Symbol::value);
        if (msg.symbol.empty()) {
            return std::unexpected{ParseError{ParseErrorCode::MissingRequiredField, tag::Symbol::value}};
        }

        char side_char = p.get_char(tag::Side::value);
        if (side_char == '\0') {
            return std::unexpected{ParseError{ParseErrorCode::MissingRequiredField, tag::Side::value}};
        }
        msg.side = static_cast<Side>(side_char);

        msg.leaves_qty = p.get_field(tag::LeavesQty::value).as_qty();
        msg.cum_qty = p.get_field(tag::CumQty::value).as_qty();
        msg.avg_px = p.get_field(tag::AvgPx::value).as_price();

        // Parse optional/conditional fields
        msg.cl_ord_id = p.get_string(tag::ClOrdID::value);
        msg.orig_cl_ord_id = p.get_string(tag::OrigClOrdID::value);
        msg.exec_ref_id = p.get_string(19);  // ExecRefID
        msg.order_qty = p.get_field(tag::OrderQty::value).as_qty();

        if (char c = p.get_char(tag::OrdType::value); c != '\0') {
            msg.ord_type = static_cast<OrdType>(c);
        }

        msg.price = p.get_field(tag::Price::value).as_price();
        msg.stop_px = p.get_field(tag::StopPx::value).as_price();

        if (char c = p.get_char(tag::TimeInForce::value); c != '\0') {
            msg.time_in_force = static_cast<TimeInForce>(c);
        }

        msg.last_px = p.get_field(tag::LastPx::value).as_price();
        msg.last_qty = p.get_field(tag::LastQty::value).as_qty();
        msg.text = p.get_string(tag::Text::value);

        if (auto v = p.get_int(tag::OrdRejReason::value)) {
            msg.ord_rej_reason = static_cast<int>(*v);
        }

        msg.account = p.get_string(tag::Account::value);
        msg.transact_time = p.get_string(tag::TransactTime::value);

        return msg;
    }

    // ========================================================================
    // Building
    // ========================================================================

    class Builder {
    public:
        // Required setters
        Builder& sender_comp_id(std::string_view v) noexcept { sender_comp_id_ = v; return *this; }
        Builder& target_comp_id(std::string_view v) noexcept { target_comp_id_ = v; return *this; }
        Builder& msg_seq_num(uint32_t v) noexcept { msg_seq_num_ = v; return *this; }
        Builder& sending_time(std::string_view v) noexcept { sending_time_ = v; return *this; }
        Builder& order_id(std::string_view v) noexcept { order_id_ = v; return *this; }
        Builder& exec_id(std::string_view v) noexcept { exec_id_ = v; return *this; }
        Builder& exec_type(ExecType v) noexcept { exec_type_ = v; return *this; }
        Builder& ord_status(OrdStatus v) noexcept { ord_status_ = v; return *this; }
        Builder& symbol(std::string_view v) noexcept { symbol_ = v; return *this; }
        Builder& side(Side v) noexcept { side_ = v; return *this; }
        Builder& leaves_qty(Qty v) noexcept { leaves_qty_ = v; return *this; }
        Builder& cum_qty(Qty v) noexcept { cum_qty_ = v; return *this; }
        Builder& avg_px(FixedPrice v) noexcept { avg_px_ = v; return *this; }

        // Optional setters
        Builder& cl_ord_id(std::string_view v) noexcept { cl_ord_id_ = v; return *this; }
        Builder& order_qty(Qty v) noexcept { order_qty_ = v; return *this; }
        Builder& ord_type(OrdType v) noexcept { ord_type_ = v; return *this; }
        Builder& price(FixedPrice v) noexcept { price_ = v; return *this; }
        Builder& time_in_force(TimeInForce v) noexcept { time_in_force_ = v; return *this; }
        Builder& last_px(FixedPrice v) noexcept { last_px_ = v; return *this; }
        Builder& last_qty(Qty v) noexcept { last_qty_ = v; return *this; }
        Builder& text(std::string_view v) noexcept { text_ = v; return *this; }
        Builder& transact_time(std::string_view v) noexcept { transact_time_ = v; return *this; }
        Builder& account(std::string_view v) noexcept { account_ = v; return *this; }

        [[nodiscard]] std::span<const char> build(MessageAssembler& asm_) const noexcept {
            asm_.start()
                .field(tag::MsgType::value, MSG_TYPE)
                .field(tag::SenderCompID::value, sender_comp_id_)
                .field(tag::TargetCompID::value, target_comp_id_)
                .field(tag::MsgSeqNum::value, static_cast<int64_t>(msg_seq_num_))
                .field(tag::SendingTime::value, sending_time_)
                .field(tag::OrderID::value, order_id_)
                .field(tag::ExecID::value, exec_id_)
                .field(tag::ExecType::value, static_cast<char>(exec_type_))
                .field(tag::OrdStatus::value, static_cast<char>(ord_status_))
                .field(tag::Symbol::value, symbol_)
                .field(tag::Side::value, static_cast<char>(side_))
                .field(tag::LeavesQty::value, static_cast<int64_t>(leaves_qty_.whole()))
                .field(tag::CumQty::value, static_cast<int64_t>(cum_qty_.whole()))
                .field(tag::AvgPx::value, avg_px_);

            if (!cl_ord_id_.empty()) {
                asm_.field(tag::ClOrdID::value, cl_ord_id_);
            }

            if (order_qty_.raw > 0) {
                asm_.field(tag::OrderQty::value, static_cast<int64_t>(order_qty_.whole()));
            }

            if (last_qty_.raw > 0) {
                asm_.field(tag::LastQty::value, static_cast<int64_t>(last_qty_.whole()));
                asm_.field(tag::LastPx::value, last_px_);
            }

            if (!transact_time_.empty()) {
                asm_.field(tag::TransactTime::value, transact_time_);
            }

            if (!account_.empty()) {
                asm_.field(tag::Account::value, account_);
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
        std::string_view order_id_;
        std::string_view exec_id_;
        ExecType exec_type_{ExecType::New};
        OrdStatus ord_status_{OrdStatus::New};
        std::string_view symbol_;
        Side side_{Side::Buy};
        Qty leaves_qty_;
        Qty cum_qty_;
        FixedPrice avg_px_;
        std::string_view cl_ord_id_;
        Qty order_qty_;
        OrdType ord_type_{OrdType::Limit};
        FixedPrice price_;
        TimeInForce time_in_force_{TimeInForce::Day};
        FixedPrice last_px_;
        Qty last_qty_;
        std::string_view text_;
        std::string_view transact_time_;
        std::string_view account_;
    };
};

// ============================================================================
// OrderCancelReject Message (MsgType = 9)
// ============================================================================

/// FIX 4.4 OrderCancelReject message (35=9)
struct OrderCancelReject {
    static constexpr char MSG_TYPE = msg_type::OrderCancelReject;

    FixHeader header;
    std::string_view order_id;        // Tag 37 - Required
    std::string_view cl_ord_id;       // Tag 11 - Required
    std::string_view orig_cl_ord_id;  // Tag 41 - Required
    OrdStatus ord_status;             // Tag 39 - Required
    int cxl_rej_response_to;          // Tag 434 - Required
    int cxl_rej_reason;               // Tag 102 - Optional
    std::string_view text;            // Tag 58 - Optional
    std::span<const char> raw_data;

    constexpr OrderCancelReject() noexcept
        : header{}
        , order_id{}
        , cl_ord_id{}
        , orig_cl_ord_id{}
        , ord_status{OrdStatus::New}
        , cxl_rej_response_to{1}
        , cxl_rej_reason{0}
        , text{}
        , raw_data{} {}

    [[nodiscard]] constexpr std::span<const char> raw() const noexcept { return raw_data; }
    [[nodiscard]] constexpr uint32_t msg_seq_num() const noexcept { return header.msg_seq_num; }
    [[nodiscard]] constexpr std::string_view sender_comp_id() const noexcept { return header.sender_comp_id; }
    [[nodiscard]] constexpr std::string_view target_comp_id() const noexcept { return header.target_comp_id; }
    [[nodiscard]] constexpr std::string_view sending_time() const noexcept { return header.sending_time; }

    [[nodiscard]] static ParseResult<OrderCancelReject> from_buffer(
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

        OrderCancelReject msg;
        msg.raw_data = buffer;
        msg.header.begin_string = p.get_string(tag::BeginString::value);
        msg.header.msg_type = p.msg_type();
        msg.header.sender_comp_id = p.sender_comp_id();
        msg.header.target_comp_id = p.target_comp_id();
        msg.header.msg_seq_num = p.msg_seq_num();
        msg.header.sending_time = p.sending_time();

        msg.order_id = p.get_string(tag::OrderID::value);
        msg.cl_ord_id = p.get_string(tag::ClOrdID::value);
        msg.orig_cl_ord_id = p.get_string(tag::OrigClOrdID::value);

        if (char c = p.get_char(tag::OrdStatus::value); c != '\0') {
            msg.ord_status = static_cast<OrdStatus>(c);
        }

        if (auto v = p.get_int(tag::CxlRejResponseTo::value)) {
            msg.cxl_rej_response_to = static_cast<int>(*v);
        }

        if (auto v = p.get_int(tag::CxlRejReason::value)) {
            msg.cxl_rej_reason = static_cast<int>(*v);
        }

        msg.text = p.get_string(tag::Text::value);

        return msg;
    }
};

} // namespace nfx::fix44
