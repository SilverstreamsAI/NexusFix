#pragma once

#include <span>
#include <string_view>

#include "nexusfix/types/tag.hpp"
#include "nexusfix/types/field_types.hpp"
#include "nexusfix/types/error.hpp"
#include "nexusfix/interfaces/i_message.hpp"
#include "nexusfix/parser/runtime_parser.hpp"
#include "nexusfix/messages/common/header.hpp"
#include "nexusfix/messages/common/trailer.hpp"

namespace nfx::fix44 {

// ============================================================================
// NewOrderSingle Message (MsgType = D)
// ============================================================================

/// FIX 4.4 NewOrderSingle message (35=D)
/// Used to submit a new order
struct NewOrderSingle {
    static constexpr char MSG_TYPE = msg_type::NewOrderSingle;

    // Header
    FixHeader header;

    // Required body fields
    std::string_view cl_ord_id;       // Tag 11 - Required
    std::string_view symbol;          // Tag 55 - Required
    Side side;                        // Tag 54 - Required
    std::string_view transact_time;   // Tag 60 - Required
    Qty order_qty;                    // Tag 38 - Required
    OrdType ord_type;                 // Tag 40 - Required

    // Conditional/Optional body fields
    FixedPrice price;                 // Tag 44 - Conditional (Limit orders)
    FixedPrice stop_px;               // Tag 99 - Conditional (Stop orders)
    TimeInForce time_in_force;        // Tag 59 - Optional
    std::string_view account;         // Tag 1 - Optional
    char handl_inst;                  // Tag 21 - Optional
    std::string_view ex_destination;  // Tag 100 - Optional
    std::string_view security_type;   // Tag 167 - Optional
    std::string_view currency;        // Tag 15 - Optional
    Qty min_qty;                      // Tag 110 - Optional
    Qty max_floor;                    // Tag 111 - Optional
    std::string_view text;            // Tag 58 - Optional

    // Raw buffer reference
    std::span<const char> raw_data;

    constexpr NewOrderSingle() noexcept
        : header{}
        , cl_ord_id{}
        , symbol{}
        , side{Side::Buy}
        , transact_time{}
        , order_qty{}
        , ord_type{OrdType::Limit}
        , price{}
        , stop_px{}
        , time_in_force{TimeInForce::Day}
        , account{}
        , handl_inst{'1'}
        , ex_destination{}
        , security_type{}
        , currency{}
        , min_qty{}
        , max_floor{}
        , text{}
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

    /// Check if this is a limit order
    [[nodiscard]] constexpr bool is_limit() const noexcept {
        return ord_type == OrdType::Limit || ord_type == OrdType::StopLimit;
    }

    /// Check if this is a market order
    [[nodiscard]] constexpr bool is_market() const noexcept {
        return ord_type == OrdType::Market;
    }

    /// Check if this is a stop order
    [[nodiscard]] constexpr bool is_stop() const noexcept {
        return ord_type == OrdType::Stop || ord_type == OrdType::StopLimit;
    }

    /// Get order notional value (price * qty)
    [[nodiscard]] constexpr FixedPrice notional() const noexcept {
        return FixedPrice{(price.raw * order_qty.raw) / Qty::SCALE};
    }

    // ========================================================================
    // Parsing
    // ========================================================================

    [[nodiscard]] static ParseResult<NewOrderSingle> from_buffer(
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

        NewOrderSingle msg;
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
        msg.cl_ord_id = p.get_string(tag::ClOrdID::value);
        if (msg.cl_ord_id.empty()) {
            return std::unexpected{ParseError{ParseErrorCode::MissingRequiredField, tag::ClOrdID::value}};
        }

        msg.symbol = p.get_string(tag::Symbol::value);
        if (msg.symbol.empty()) {
            return std::unexpected{ParseError{ParseErrorCode::MissingRequiredField, tag::Symbol::value}};
        }

        char side_char = p.get_char(tag::Side::value);
        if (side_char == '\0') {
            return std::unexpected{ParseError{ParseErrorCode::MissingRequiredField, tag::Side::value}};
        }
        msg.side = static_cast<Side>(side_char);

        msg.transact_time = p.get_string(tag::TransactTime::value);
        if (msg.transact_time.empty()) {
            return std::unexpected{ParseError{ParseErrorCode::MissingRequiredField, tag::TransactTime::value}};
        }

        msg.order_qty = p.get_field(tag::OrderQty::value).as_qty();
        if (msg.order_qty.raw == 0) {
            return std::unexpected{ParseError{ParseErrorCode::MissingRequiredField, tag::OrderQty::value}};
        }

        char ord_type_char = p.get_char(tag::OrdType::value);
        if (ord_type_char == '\0') {
            return std::unexpected{ParseError{ParseErrorCode::MissingRequiredField, tag::OrdType::value}};
        }
        msg.ord_type = static_cast<OrdType>(ord_type_char);

        // Parse optional/conditional fields
        msg.price = p.get_field(tag::Price::value).as_price();
        msg.stop_px = p.get_field(tag::StopPx::value).as_price();

        if (char c = p.get_char(tag::TimeInForce::value); c != '\0') {
            msg.time_in_force = static_cast<TimeInForce>(c);
        }

        msg.account = p.get_string(tag::Account::value);
        msg.handl_inst = p.get_char(tag::HandlInst::value);
        msg.ex_destination = p.get_string(tag::ExDestination::value);
        msg.security_type = p.get_string(tag::SecurityType::value);
        msg.currency = p.get_string(15);  // Currency
        msg.min_qty = p.get_field(110).as_qty();  // MinQty
        msg.max_floor = p.get_field(111).as_qty();  // MaxFloor
        msg.text = p.get_string(tag::Text::value);

        // Validate conditional fields
        if (msg.is_limit() && msg.price.raw == 0) {
            return std::unexpected{ParseError{ParseErrorCode::MissingRequiredField, tag::Price::value}};
        }

        if (msg.is_stop() && msg.stop_px.raw == 0) {
            return std::unexpected{ParseError{ParseErrorCode::MissingRequiredField, tag::StopPx::value}};
        }

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
        Builder& cl_ord_id(std::string_view v) noexcept { cl_ord_id_ = v; return *this; }
        Builder& symbol(std::string_view v) noexcept { symbol_ = v; return *this; }
        Builder& side(Side v) noexcept { side_ = v; return *this; }
        Builder& transact_time(std::string_view v) noexcept { transact_time_ = v; return *this; }
        Builder& order_qty(Qty v) noexcept { order_qty_ = v; return *this; }
        Builder& ord_type(OrdType v) noexcept { ord_type_ = v; return *this; }

        // Optional setters
        Builder& price(FixedPrice v) noexcept { price_ = v; return *this; }
        Builder& stop_px(FixedPrice v) noexcept { stop_px_ = v; return *this; }
        Builder& time_in_force(TimeInForce v) noexcept { time_in_force_ = v; return *this; }
        Builder& account(std::string_view v) noexcept { account_ = v; return *this; }
        Builder& handl_inst(char v) noexcept { handl_inst_ = v; return *this; }
        Builder& ex_destination(std::string_view v) noexcept { ex_destination_ = v; return *this; }
        Builder& text(std::string_view v) noexcept { text_ = v; return *this; }

        [[nodiscard]] std::span<const char> build(MessageAssembler& asm_) const noexcept {
            asm_.start()
                .field(tag::MsgType::value, MSG_TYPE)
                .field(tag::SenderCompID::value, sender_comp_id_)
                .field(tag::TargetCompID::value, target_comp_id_)
                .field(tag::MsgSeqNum::value, static_cast<int64_t>(msg_seq_num_))
                .field(tag::SendingTime::value, sending_time_)
                .field(tag::ClOrdID::value, cl_ord_id_)
                .field(tag::Symbol::value, symbol_)
                .field(tag::Side::value, static_cast<char>(side_))
                .field(tag::TransactTime::value, transact_time_)
                .field(tag::OrderQty::value, static_cast<int64_t>(order_qty_.whole()))
                .field(tag::OrdType::value, static_cast<char>(ord_type_));

            if (price_.raw != 0) {
                asm_.field(tag::Price::value, price_);
            }

            if (stop_px_.raw != 0) {
                asm_.field(tag::StopPx::value, stop_px_);
            }

            asm_.field(tag::TimeInForce::value, static_cast<char>(time_in_force_));

            if (!account_.empty()) {
                asm_.field(tag::Account::value, account_);
            }

            if (handl_inst_ != '\0') {
                asm_.field(tag::HandlInst::value, handl_inst_);
            }

            if (!ex_destination_.empty()) {
                asm_.field(tag::ExDestination::value, ex_destination_);
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
        std::string_view cl_ord_id_;
        std::string_view symbol_;
        Side side_{Side::Buy};
        std::string_view transact_time_;
        Qty order_qty_;
        OrdType ord_type_{OrdType::Limit};
        FixedPrice price_;
        FixedPrice stop_px_;
        TimeInForce time_in_force_{TimeInForce::Day};
        std::string_view account_;
        char handl_inst_{'1'};
        std::string_view ex_destination_;
        std::string_view text_;
    };
};

// ============================================================================
// OrderCancelRequest Message (MsgType = F)
// ============================================================================

/// FIX 4.4 OrderCancelRequest message (35=F)
struct OrderCancelRequest {
    static constexpr char MSG_TYPE = msg_type::OrderCancelRequest;

    FixHeader header;
    std::string_view orig_cl_ord_id;  // Tag 41 - Required
    std::string_view cl_ord_id;       // Tag 11 - Required
    std::string_view symbol;          // Tag 55 - Required
    Side side;                        // Tag 54 - Required
    std::string_view transact_time;   // Tag 60 - Required
    Qty order_qty;                    // Tag 38 - Optional
    std::string_view order_id;        // Tag 37 - Optional
    std::string_view text;            // Tag 58 - Optional
    std::span<const char> raw_data;

    constexpr OrderCancelRequest() noexcept
        : header{}
        , orig_cl_ord_id{}
        , cl_ord_id{}
        , symbol{}
        , side{Side::Buy}
        , transact_time{}
        , order_qty{}
        , order_id{}
        , text{}
        , raw_data{} {}

    [[nodiscard]] constexpr std::span<const char> raw() const noexcept { return raw_data; }
    [[nodiscard]] constexpr uint32_t msg_seq_num() const noexcept { return header.msg_seq_num; }
    [[nodiscard]] constexpr std::string_view sender_comp_id() const noexcept { return header.sender_comp_id; }
    [[nodiscard]] constexpr std::string_view target_comp_id() const noexcept { return header.target_comp_id; }
    [[nodiscard]] constexpr std::string_view sending_time() const noexcept { return header.sending_time; }

    [[nodiscard]] static ParseResult<OrderCancelRequest> from_buffer(
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

        OrderCancelRequest msg;
        msg.raw_data = buffer;
        msg.header.begin_string = p.get_string(tag::BeginString::value);
        msg.header.msg_type = p.msg_type();
        msg.header.sender_comp_id = p.sender_comp_id();
        msg.header.target_comp_id = p.target_comp_id();
        msg.header.msg_seq_num = p.msg_seq_num();
        msg.header.sending_time = p.sending_time();

        msg.orig_cl_ord_id = p.get_string(tag::OrigClOrdID::value);
        msg.cl_ord_id = p.get_string(tag::ClOrdID::value);
        msg.symbol = p.get_string(tag::Symbol::value);

        if (char c = p.get_char(tag::Side::value); c != '\0') {
            msg.side = static_cast<Side>(c);
        }

        msg.transact_time = p.get_string(tag::TransactTime::value);
        msg.order_qty = p.get_field(tag::OrderQty::value).as_qty();
        msg.order_id = p.get_string(tag::OrderID::value);
        msg.text = p.get_string(tag::Text::value);

        return msg;
    }

    class Builder {
    public:
        Builder& sender_comp_id(std::string_view v) noexcept { sender_comp_id_ = v; return *this; }
        Builder& target_comp_id(std::string_view v) noexcept { target_comp_id_ = v; return *this; }
        Builder& msg_seq_num(uint32_t v) noexcept { msg_seq_num_ = v; return *this; }
        Builder& sending_time(std::string_view v) noexcept { sending_time_ = v; return *this; }
        Builder& orig_cl_ord_id(std::string_view v) noexcept { orig_cl_ord_id_ = v; return *this; }
        Builder& cl_ord_id(std::string_view v) noexcept { cl_ord_id_ = v; return *this; }
        Builder& symbol(std::string_view v) noexcept { symbol_ = v; return *this; }
        Builder& side(Side v) noexcept { side_ = v; return *this; }
        Builder& transact_time(std::string_view v) noexcept { transact_time_ = v; return *this; }
        Builder& order_qty(Qty v) noexcept { order_qty_ = v; return *this; }
        Builder& order_id(std::string_view v) noexcept { order_id_ = v; return *this; }

        [[nodiscard]] std::span<const char> build(MessageAssembler& asm_) const noexcept {
            asm_.start()
                .field(tag::MsgType::value, MSG_TYPE)
                .field(tag::SenderCompID::value, sender_comp_id_)
                .field(tag::TargetCompID::value, target_comp_id_)
                .field(tag::MsgSeqNum::value, static_cast<int64_t>(msg_seq_num_))
                .field(tag::SendingTime::value, sending_time_)
                .field(tag::OrigClOrdID::value, orig_cl_ord_id_)
                .field(tag::ClOrdID::value, cl_ord_id_)
                .field(tag::Symbol::value, symbol_)
                .field(tag::Side::value, static_cast<char>(side_))
                .field(tag::TransactTime::value, transact_time_);

            if (order_qty_.raw > 0) {
                asm_.field(tag::OrderQty::value, static_cast<int64_t>(order_qty_.whole()));
            }

            if (!order_id_.empty()) {
                asm_.field(tag::OrderID::value, order_id_);
            }

            return asm_.finish();
        }

    private:
        std::string_view sender_comp_id_;
        std::string_view target_comp_id_;
        uint32_t msg_seq_num_{1};
        std::string_view sending_time_;
        std::string_view orig_cl_ord_id_;
        std::string_view cl_ord_id_;
        std::string_view symbol_;
        Side side_{Side::Buy};
        std::string_view transact_time_;
        Qty order_qty_;
        std::string_view order_id_;
    };
};

} // namespace nfx::fix44
