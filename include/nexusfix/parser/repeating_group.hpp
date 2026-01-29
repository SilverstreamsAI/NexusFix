#pragma once

#include <span>
#include <string_view>
#include <cstdint>
#include <array>
#include <optional>

#include "nexusfix/platform/platform.hpp"
#include "nexusfix/types/tag.hpp"
#include "nexusfix/types/field_types.hpp"
#include "nexusfix/types/market_data_types.hpp"
#include "nexusfix/parser/field_view.hpp"

namespace nfx::parser {

// ============================================================================
// Repeating Group Iterator
// ============================================================================

/// Zero-copy iterator over repeating group entries in a FIX message.
/// Each entry starts with the delimiter tag (first tag in the group definition).
class RepeatingGroupIterator {
public:
    struct Entry {
        std::span<const char> data;  // Raw data for this entry
        size_t start_pos;
        size_t end_pos;

        /// Get a field value by tag number
        [[nodiscard]] NFX_HOT
        FieldView get_field(int target_tag) const noexcept {
            FieldIterator iter{data};
            while (iter.has_next()) [[likely]] {
                FieldView field = iter.next();
                if (!field.is_valid()) [[unlikely]] break;
                if (field.tag == target_tag) [[unlikely]] {
                    return field;
                }
            }
            return FieldView{};
        }

        [[nodiscard]] std::string_view get_string(int target_tag) const noexcept {
            auto fv = get_field(target_tag);
            return std::string_view{fv.value.data(), fv.value.size()};
        }

        [[nodiscard]] char get_char(int target_tag) const noexcept {
            auto fv = get_field(target_tag);
            return fv.is_valid() && !fv.value.empty() ? fv.value[0] : '\0';
        }

        [[nodiscard]] std::optional<int64_t> get_int(int tag) const noexcept {
            return get_field(tag).as_int();
        }

        [[nodiscard]] FixedPrice get_price(int tag) const noexcept {
            return get_field(tag).as_price();
        }

        [[nodiscard]] Qty get_qty(int tag) const noexcept {
            return get_field(tag).as_qty();
        }
    };

    constexpr RepeatingGroupIterator() noexcept = default;

    RepeatingGroupIterator(std::span<const char> data, int delimiter_tag, size_t count) noexcept
        : data_{data}
        , delimiter_tag_{delimiter_tag}
        , total_count_{count}
        , current_index_{0}
        , current_pos_{0}
    {
        if (count > 0) [[likely]] {
            find_first_entry();
        }
    }

    [[nodiscard]] bool has_next() const noexcept {
        return current_index_ < total_count_;
    }

    [[nodiscard]] NFX_HOT
    Entry next() noexcept {
        if (current_index_ >= total_count_) [[unlikely]] {
            return Entry{{}, 0, 0};
        }

        size_t start = current_pos_;
        size_t end = find_next_entry_start();

        Entry entry{data_.subspan(start, end - start), start, end};
        current_pos_ = end;
        ++current_index_;

        return entry;
    }

    [[nodiscard]] size_t count() const noexcept {
        return total_count_;
    }

    [[nodiscard]] size_t current() const noexcept {
        return current_index_;
    }

private:
    void find_first_entry() noexcept {
        // Find the first occurrence of the delimiter tag
        current_pos_ = find_tag_position(0, delimiter_tag_);
    }

    [[nodiscard]] NFX_HOT
    size_t find_next_entry_start() noexcept {
        // Find the next occurrence of delimiter tag, or end of data
        size_t search_start = current_pos_;

        // Skip past current tag=value pair
        while (search_start < data_.size() && data_[search_start] != fix::SOH) [[likely]] {
            ++search_start;
        }
        if (search_start < data_.size()) [[likely]] {
            ++search_start; // Skip SOH
        }

        // Look for next delimiter tag or end
        if (current_index_ + 1 >= total_count_) [[unlikely]] {
            // Last entry - find end by looking for CheckSum tag (10=)
            // or just return remaining data
            size_t pos = search_start;
            while (pos < data_.size()) [[likely]] {
                // Check for tag 10= (checksum)
                if (pos + 3 < data_.size() &&
                    data_[pos] == '1' && data_[pos + 1] == '0' && data_[pos + 2] == '=') [[unlikely]] {
                    return pos;
                }
                // Move to next field
                while (pos < data_.size() && data_[pos] != fix::SOH) [[likely]] {
                    ++pos;
                }
                if (pos < data_.size()) [[likely]] ++pos;
            }
            return data_.size();
        }

        // Find next occurrence of delimiter tag
        return find_tag_position(search_start, delimiter_tag_);
    }

    [[nodiscard]] NFX_HOT
    size_t find_tag_position(size_t start, int tag) const noexcept {
        // Convert tag to string for comparison
        char tag_buf[12];
        int tag_len = 0;
        int t = tag;
        do {
            tag_buf[tag_len++] = '0' + (t % 10);
            t /= 10;
        } while (t > 0);

        // Reverse
        for (int i = 0; i < tag_len / 2; ++i) {
            char tmp = tag_buf[i];
            tag_buf[i] = tag_buf[tag_len - 1 - i];
            tag_buf[tag_len - 1 - i] = tmp;
        }

        // Search for "tag=" pattern at field boundaries
        size_t pos = start;
        while (pos < data_.size()) [[likely]] {
            // Check if this position matches our tag
            bool match = true;
            for (int i = 0; i < tag_len && pos + i < data_.size(); ++i) {
                if (data_[pos + i] != tag_buf[i]) {
                    match = false;
                    break;
                }
            }

            if (match && pos + tag_len < data_.size() && data_[pos + tag_len] == '=') [[unlikely]] {
                return pos;
            }

            // Move to next SOH
            while (pos < data_.size() && data_[pos] != fix::SOH) [[likely]] {
                ++pos;
            }
            if (pos < data_.size()) [[likely]] ++pos;
        }

        return data_.size();
    }

    std::span<const char> data_{};
    int delimiter_tag_{0};
    size_t total_count_{0};
    size_t current_index_{0};
    size_t current_pos_{0};
};

// ============================================================================
// MD Entry Parser Helper
// ============================================================================

/// Parse a single MDEntry from a repeating group entry
[[nodiscard]] NFX_HOT
inline MDEntry parse_md_entry(const RepeatingGroupIterator::Entry& entry) noexcept {
    MDEntry md;

    if (auto c = entry.get_char(tag::MDEntryType::value); c != '\0') {
        md.entry_type = static_cast<MDEntryType>(c);
    }

    md.price_raw = entry.get_price(tag::MDEntryPx::value).raw;
    md.size_raw = entry.get_qty(tag::MDEntrySize::value).raw;

    if (auto c = entry.get_char(tag::MDUpdateAction::value); c != '\0') {
        md.update_action = static_cast<MDUpdateAction>(c);
    }

    md.entry_id = entry.get_string(tag::MDEntryID::value);
    md.symbol = entry.get_string(tag::Symbol::value);
    md.entry_date = entry.get_string(tag::MDEntryDate::value);
    md.entry_time = entry.get_string(tag::MDEntryTime::value);

    if (auto v = entry.get_int(tag::MDEntryPositionNo::value)) {
        md.position_no = static_cast<int>(*v);
    }

    if (auto v = entry.get_int(tag::NumberOfOrders::value)) {
        md.number_of_orders = static_cast<int>(*v);
    }

    return md;
}

// ============================================================================
// Related Symbol Parser Helper
// ============================================================================

/// Parse a single RelatedSym entry from a repeating group entry
[[nodiscard]] inline RelatedSymbol parse_related_symbol(
    const RepeatingGroupIterator::Entry& entry) noexcept
{
    RelatedSymbol sym;
    sym.symbol = entry.get_string(tag::Symbol::value);
    sym.security_id = entry.get_string(tag::SecurityID::value);
    sym.security_exchange = entry.get_string(tag::SecurityExchange::value);
    return sym;
}

// ============================================================================
// MDEntry Iterator Wrapper
// ============================================================================

/// Typed iterator specifically for MDEntry repeating groups
class MDEntryIterator {
public:
    MDEntryIterator() noexcept = default;

    /// Constructor with custom delimiter tag
    /// @param data Message data containing the repeating group
    /// @param count Number of entries in the group
    /// @param delimiter_tag First tag of each entry (269 for snapshot, 279 for incremental)
    explicit MDEntryIterator(std::span<const char> data, size_t count,
                             int delimiter_tag = tag::MDEntryType::value) noexcept
        : iter_{data, delimiter_tag, count}
    {}

    [[nodiscard]] bool has_next() const noexcept {
        return iter_.has_next();
    }

    [[nodiscard]] NFX_HOT
    MDEntry next() noexcept {
        auto entry = iter_.next();
        return parse_md_entry(entry);
    }

    [[nodiscard]] size_t count() const noexcept {
        return iter_.count();
    }

private:
    RepeatingGroupIterator iter_;
};

// ============================================================================
// Related Symbol Iterator Wrapper
// ============================================================================

/// Typed iterator specifically for RelatedSym repeating groups
class RelatedSymIterator {
public:
    RelatedSymIterator() noexcept = default;

    explicit RelatedSymIterator(std::span<const char> data, size_t count) noexcept
        : iter_{data, tag::Symbol::value, count}
    {}

    [[nodiscard]] bool has_next() const noexcept {
        return iter_.has_next();
    }

    [[nodiscard]] RelatedSymbol next() noexcept {
        auto entry = iter_.next();
        return parse_related_symbol(entry);
    }

    [[nodiscard]] size_t count() const noexcept {
        return iter_.count();
    }

private:
    RepeatingGroupIterator iter_;
};

} // namespace nfx::parser
