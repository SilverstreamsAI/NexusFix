#pragma once

#include <span>
#include <string_view>
#include <vector>
#include <optional>

#include "nexusfix/types/tag.hpp"
#include "nexusfix/types/field_types.hpp"
#include "nexusfix/types/market_data_types.hpp"
#include "nexusfix/types/error.hpp"
#include "nexusfix/interfaces/i_message.hpp"
#include "nexusfix/parser/runtime_parser.hpp"
#include "nexusfix/parser/repeating_group.hpp"
#include "nexusfix/messages/common/header.hpp"
#include "nexusfix/messages/common/trailer.hpp"

namespace nfx::fix44 {

// ============================================================================
// MarketDataRequest Message (MsgType = V)
// ============================================================================

/// FIX 4.4 MarketDataRequest message (35=V)
/// Used to subscribe/unsubscribe to market data
struct MarketDataRequest {
    static constexpr char MSG_TYPE = 'V';

    FixHeader header;
    std::string_view md_req_id;              // Tag 262 - Required
    SubscriptionRequestType subscription_type; // Tag 263 - Required
    int market_depth;                         // Tag 264 - Required
    MDUpdateType md_update_type;              // Tag 265 - Conditional
    bool aggregated_book;                     // Tag 266 - Optional

    // Repeating groups (stored as raw data for parsing)
    size_t no_md_entry_types;                 // Tag 267
    size_t no_related_sym;                    // Tag 146

    std::span<const char> raw_data;

    constexpr MarketDataRequest() noexcept
        : header{}
        , md_req_id{}
        , subscription_type{SubscriptionRequestType::Snapshot}
        , market_depth{0}
        , md_update_type{MDUpdateType::FullRefresh}
        , aggregated_book{true}
        , no_md_entry_types{0}
        , no_related_sym{0}
        , raw_data{} {}

    [[nodiscard]] constexpr std::span<const char> raw() const noexcept { return raw_data; }
    [[nodiscard]] constexpr uint32_t msg_seq_num() const noexcept { return header.msg_seq_num; }
    [[nodiscard]] constexpr std::string_view sender_comp_id() const noexcept { return header.sender_comp_id; }
    [[nodiscard]] constexpr std::string_view target_comp_id() const noexcept { return header.target_comp_id; }
    [[nodiscard]] constexpr std::string_view sending_time() const noexcept { return header.sending_time; }

    /// Check if this is a snapshot request
    [[nodiscard]] constexpr bool is_snapshot() const noexcept {
        return subscription_type == SubscriptionRequestType::Snapshot;
    }

    /// Check if this is a subscription request
    [[nodiscard]] constexpr bool is_subscribe() const noexcept {
        return subscription_type == SubscriptionRequestType::SnapshotPlusUpdates;
    }

    /// Check if this is an unsubscription request
    [[nodiscard]] constexpr bool is_unsubscribe() const noexcept {
        return subscription_type == SubscriptionRequestType::DisablePreviousSnapshot;
    }

    // ========================================================================
    // Builder
    // ========================================================================

    class Builder {
    public:
        Builder& sender_comp_id(std::string_view v) noexcept { sender_comp_id_ = v; return *this; }
        Builder& target_comp_id(std::string_view v) noexcept { target_comp_id_ = v; return *this; }
        Builder& msg_seq_num(uint32_t v) noexcept { msg_seq_num_ = v; return *this; }
        Builder& sending_time(std::string_view v) noexcept { sending_time_ = v; return *this; }
        Builder& md_req_id(std::string_view v) noexcept { md_req_id_ = v; return *this; }
        Builder& subscription_type(SubscriptionRequestType v) noexcept { subscription_type_ = v; return *this; }
        Builder& market_depth(int v) noexcept { market_depth_ = v; return *this; }
        Builder& md_update_type(MDUpdateType v) noexcept { md_update_type_ = v; return *this; }
        Builder& aggregated_book(bool v) noexcept { aggregated_book_ = v; return *this; }

        /// Add MDEntryType to request (Bid, Offer, Trade, etc.)
        Builder& add_entry_type(MDEntryType type) noexcept {
            if (entry_type_count_ < MAX_ENTRY_TYPES) {
                entry_types_[entry_type_count_++] = type;
            }
            return *this;
        }

        /// Add symbol to request
        Builder& add_symbol(std::string_view symbol) noexcept {
            if (symbol_count_ < MAX_SYMBOLS) {
                symbols_[symbol_count_++] = symbol;
            }
            return *this;
        }

        [[nodiscard]] std::span<const char> build(MessageAssembler& asm_) const noexcept {
            asm_.start()
                .field(tag::MsgType::value, MSG_TYPE)
                .field(tag::SenderCompID::value, sender_comp_id_)
                .field(tag::TargetCompID::value, target_comp_id_)
                .field(tag::MsgSeqNum::value, static_cast<int64_t>(msg_seq_num_))
                .field(tag::SendingTime::value, sending_time_)
                .field(tag::MDReqID::value, md_req_id_)
                .field(tag::SubscriptionRequestType::value, static_cast<char>(subscription_type_))
                .field(tag::MarketDepth::value, static_cast<int64_t>(market_depth_));

            // MDUpdateType required if subscribing
            if (subscription_type_ == SubscriptionRequestType::SnapshotPlusUpdates) {
                asm_.field(tag::MDUpdateType::value, static_cast<int64_t>(static_cast<int>(md_update_type_)));
            }

            asm_.field(tag::AggregatedBook::value, aggregated_book_ ? 'Y' : 'N');

            // MDEntryTypes repeating group
            if (entry_type_count_ > 0) {
                asm_.field(tag::NoMDEntryTypes::value, static_cast<int64_t>(entry_type_count_));
                for (size_t i = 0; i < entry_type_count_; ++i) {
                    asm_.field(tag::MDEntryType::value, static_cast<char>(entry_types_[i]));
                }
            }

            // RelatedSym repeating group
            if (symbol_count_ > 0) {
                asm_.field(tag::NoRelatedSym::value, static_cast<int64_t>(symbol_count_));
                for (size_t i = 0; i < symbol_count_; ++i) {
                    asm_.field(tag::Symbol::value, symbols_[i]);
                }
            }

            return asm_.finish();
        }

    private:
        static constexpr size_t MAX_ENTRY_TYPES = 16;
        static constexpr size_t MAX_SYMBOLS = 64;

        std::string_view sender_comp_id_;
        std::string_view target_comp_id_;
        uint32_t msg_seq_num_{1};
        std::string_view sending_time_;
        std::string_view md_req_id_;
        SubscriptionRequestType subscription_type_{SubscriptionRequestType::SnapshotPlusUpdates};
        int market_depth_{0};
        MDUpdateType md_update_type_{MDUpdateType::IncrementalRefresh};
        bool aggregated_book_{true};

        std::array<MDEntryType, MAX_ENTRY_TYPES> entry_types_;
        size_t entry_type_count_{0};

        std::array<std::string_view, MAX_SYMBOLS> symbols_;
        size_t symbol_count_{0};
    };
};

// ============================================================================
// MarketDataSnapshotFullRefresh Message (MsgType = W)
// ============================================================================

/// FIX 4.4 MarketDataSnapshotFullRefresh message (35=W)
/// Contains full market data snapshot for a symbol
struct MarketDataSnapshotFullRefresh {
    static constexpr char MSG_TYPE = 'W';

    FixHeader header;
    std::string_view md_req_id;       // Tag 262 - Conditional
    std::string_view symbol;          // Tag 55 - Required
    std::string_view security_id;     // Tag 48 - Optional
    std::string_view security_exchange; // Tag 207 - Optional
    size_t no_md_entries;             // Tag 268 - Required

    std::span<const char> raw_data;

    constexpr MarketDataSnapshotFullRefresh() noexcept
        : header{}
        , md_req_id{}
        , symbol{}
        , security_id{}
        , security_exchange{}
        , no_md_entries{0}
        , raw_data{} {}

    [[nodiscard]] constexpr std::span<const char> raw() const noexcept { return raw_data; }
    [[nodiscard]] constexpr uint32_t msg_seq_num() const noexcept { return header.msg_seq_num; }
    [[nodiscard]] constexpr std::string_view sender_comp_id() const noexcept { return header.sender_comp_id; }
    [[nodiscard]] constexpr std::string_view target_comp_id() const noexcept { return header.target_comp_id; }
    [[nodiscard]] constexpr std::string_view sending_time() const noexcept { return header.sending_time; }

    /// Get iterator over MDEntry repeating group
    [[nodiscard]] parser::MDEntryIterator entries() const noexcept {
        return parser::MDEntryIterator{raw_data, no_md_entries};
    }

    /// Get number of entries
    [[nodiscard]] constexpr size_t entry_count() const noexcept {
        return no_md_entries;
    }

    // ========================================================================
    // Parsing
    // ========================================================================

    [[nodiscard]] static ParseResult<MarketDataSnapshotFullRefresh> from_buffer(
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

        MarketDataSnapshotFullRefresh msg;
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

        // Parse body fields
        msg.md_req_id = p.get_string(tag::MDReqID::value);
        msg.symbol = p.get_string(tag::Symbol::value);

        if (msg.symbol.empty()) {
            return std::unexpected{ParseError{ParseErrorCode::MissingRequiredField, tag::Symbol::value}};
        }

        msg.security_id = p.get_string(tag::SecurityID::value);
        msg.security_exchange = p.get_string(tag::SecurityExchange::value);

        if (auto v = p.get_int(tag::NoMDEntries::value)) {
            msg.no_md_entries = static_cast<size_t>(*v);
        }

        return msg;
    }
};

// ============================================================================
// MarketDataIncrementalRefresh Message (MsgType = X)
// ============================================================================

/// FIX 4.4 MarketDataIncrementalRefresh message (35=X)
/// Contains incremental market data updates
struct MarketDataIncrementalRefresh {
    static constexpr char MSG_TYPE = 'X';

    FixHeader header;
    std::string_view md_req_id;       // Tag 262 - Optional
    size_t no_md_entries;             // Tag 268 - Required

    std::span<const char> raw_data;

    constexpr MarketDataIncrementalRefresh() noexcept
        : header{}
        , md_req_id{}
        , no_md_entries{0}
        , raw_data{} {}

    [[nodiscard]] constexpr std::span<const char> raw() const noexcept { return raw_data; }
    [[nodiscard]] constexpr uint32_t msg_seq_num() const noexcept { return header.msg_seq_num; }
    [[nodiscard]] constexpr std::string_view sender_comp_id() const noexcept { return header.sender_comp_id; }
    [[nodiscard]] constexpr std::string_view target_comp_id() const noexcept { return header.target_comp_id; }
    [[nodiscard]] constexpr std::string_view sending_time() const noexcept { return header.sending_time; }

    /// Get iterator over MDEntry repeating group (with update actions)
    /// Note: Incremental refresh uses MDUpdateAction (279) as delimiter, not MDEntryType (269)
    [[nodiscard]] parser::MDEntryIterator entries() const noexcept {
        return parser::MDEntryIterator{raw_data, no_md_entries, tag::MDUpdateAction::value};
    }

    /// Get number of entries
    [[nodiscard]] constexpr size_t entry_count() const noexcept {
        return no_md_entries;
    }

    // ========================================================================
    // Parsing
    // ========================================================================

    [[nodiscard]] static ParseResult<MarketDataIncrementalRefresh> from_buffer(
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

        MarketDataIncrementalRefresh msg;
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

        // Parse body fields
        msg.md_req_id = p.get_string(tag::MDReqID::value);

        if (auto v = p.get_int(tag::NoMDEntries::value)) {
            msg.no_md_entries = static_cast<size_t>(*v);
        } else {
            return std::unexpected{ParseError{ParseErrorCode::MissingRequiredField, tag::NoMDEntries::value}};
        }

        return msg;
    }
};

// ============================================================================
// MarketDataRequestReject Message (MsgType = Y)
// ============================================================================

/// FIX 4.4 MarketDataRequestReject message (35=Y)
/// Indicates rejection of a market data subscription request
struct MarketDataRequestReject {
    static constexpr char MSG_TYPE = 'Y';

    FixHeader header;
    std::string_view md_req_id;       // Tag 262 - Required
    MDReqRejReason md_req_rej_reason; // Tag 281 - Optional
    std::string_view text;            // Tag 58 - Optional

    std::span<const char> raw_data;

    constexpr MarketDataRequestReject() noexcept
        : header{}
        , md_req_id{}
        , md_req_rej_reason{MDReqRejReason::Other}
        , text{}
        , raw_data{} {}

    [[nodiscard]] constexpr std::span<const char> raw() const noexcept { return raw_data; }
    [[nodiscard]] constexpr uint32_t msg_seq_num() const noexcept { return header.msg_seq_num; }
    [[nodiscard]] constexpr std::string_view sender_comp_id() const noexcept { return header.sender_comp_id; }
    [[nodiscard]] constexpr std::string_view target_comp_id() const noexcept { return header.target_comp_id; }
    [[nodiscard]] constexpr std::string_view sending_time() const noexcept { return header.sending_time; }

    /// Get human-readable rejection reason
    [[nodiscard]] std::string_view rejection_reason_name() const noexcept {
        return md_rej_reason_name(md_req_rej_reason);
    }

    // ========================================================================
    // Parsing
    // ========================================================================

    [[nodiscard]] static ParseResult<MarketDataRequestReject> from_buffer(
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

        MarketDataRequestReject msg;
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

        // Parse body fields
        msg.md_req_id = p.get_string(tag::MDReqID::value);
        if (msg.md_req_id.empty()) {
            return std::unexpected{ParseError{ParseErrorCode::MissingRequiredField, tag::MDReqID::value}};
        }

        if (char c = p.get_char(tag::MDReqRejReason::value); c != '\0') {
            msg.md_req_rej_reason = static_cast<MDReqRejReason>(c);
        }

        msg.text = p.get_string(tag::Text::value);

        return msg;
    }
};

} // namespace nfx::fix44
