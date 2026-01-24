#pragma once

#include <span>
#include <string_view>
#include <cstdint>
#include <charconv>
#include <optional>
#include <limits>

#include "nexusfix/types/tag.hpp"
#include "nexusfix/types/field_types.hpp"
#include "nexusfix/interfaces/i_message.hpp"
#include "nexusfix/memory/buffer_pool.hpp"  // For CACHE_LINE_SIZE

namespace nfx {

// ============================================================================
// Zero-Copy Field View
// ============================================================================

/// View into a FIX field without copying data
struct FieldView {
    int tag;                        // Field tag number
    std::span<const char> value;    // Points into original buffer

    constexpr FieldView() noexcept : tag{0}, value{} {}

    constexpr FieldView(int t, std::span<const char> v) noexcept
        : tag{t}, value{v} {}

    constexpr FieldView(int t, const char* data, size_t len) noexcept
        : tag{t}, value{data, len} {}

    // ========================================================================
    // Zero-copy Value Accessors
    // ========================================================================

    /// Get value as string_view (zero-copy)
    [[nodiscard]] constexpr std::string_view as_string() const noexcept {
        return std::string_view{value.data(), value.size()};
    }

    /// Get single character value
    [[nodiscard]] constexpr char as_char() const noexcept {
        return value.empty() ? '\0' : value[0];
    }

    /// Get value as boolean (Y/N)
    [[nodiscard]] constexpr bool as_bool() const noexcept {
        return !value.empty() && value[0] == 'Y';
    }

    /// Parse value as integer
    [[nodiscard]] constexpr std::optional<int64_t> as_int() const noexcept {
        if (value.empty()) [[unlikely]] return std::nullopt;

        int64_t result = 0;
        bool negative = false;
        size_t i = 0;

        if (value[0] == '-') [[unlikely]] {
            negative = true;
            i = 1;
        }

        for (; i < value.size(); ++i) [[likely]] {
            char c = value[i];
            if (c < '0' || c > '9') [[unlikely]] return std::nullopt;
            result = result * 10 + (c - '0');
        }

        return negative ? -result : result;
    }

    /// Parse value as unsigned integer
    [[nodiscard]] constexpr std::optional<uint64_t> as_uint() const noexcept {
        if (value.empty()) [[unlikely]] return std::nullopt;

        uint64_t result = 0;
        for (char c : value) [[likely]] {
            if (c < '0' || c > '9') [[unlikely]] return std::nullopt;
            result = result * 10 + (c - '0');
        }
        return result;
    }

    /// Parse value as fixed-point price
    [[nodiscard]] constexpr FixedPrice as_price() const noexcept {
        return FixedPrice::from_string(as_string());
    }

    /// Parse value as quantity
    [[nodiscard]] constexpr Qty as_qty() const noexcept {
        return Qty::from_string(as_string());
    }

    /// Parse value as Side enum
    [[nodiscard]] constexpr std::optional<Side> as_side() const noexcept {
        if (value.empty()) return std::nullopt;
        char c = value[0];
        if (c >= '1' && c <= '9') {
            return static_cast<Side>(c);
        }
        return std::nullopt;
    }

    /// Parse value as OrdType enum
    [[nodiscard]] constexpr std::optional<OrdType> as_ord_type() const noexcept {
        if (value.empty()) return std::nullopt;
        return static_cast<OrdType>(value[0]);
    }

    /// Parse value as OrdStatus enum
    [[nodiscard]] constexpr std::optional<OrdStatus> as_ord_status() const noexcept {
        if (value.empty()) return std::nullopt;
        return static_cast<OrdStatus>(value[0]);
    }

    /// Parse value as ExecType enum
    [[nodiscard]] constexpr std::optional<ExecType> as_exec_type() const noexcept {
        if (value.empty()) return std::nullopt;
        return static_cast<ExecType>(value[0]);
    }

    /// Parse value as TimeInForce enum
    [[nodiscard]] constexpr std::optional<TimeInForce> as_time_in_force() const noexcept {
        if (value.empty()) return std::nullopt;
        return static_cast<TimeInForce>(value[0]);
    }

    // ========================================================================
    // Validation
    // ========================================================================

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return tag > 0;
    }

    [[nodiscard]] constexpr bool is_empty() const noexcept {
        return value.empty();
    }

    [[nodiscard]] constexpr size_t size() const noexcept {
        return value.size();
    }
};

// ============================================================================
// Field Iterator (for parsing)
// ============================================================================

/// Iterator over FIX fields in a buffer
class FieldIterator {
public:
    constexpr FieldIterator() noexcept
        : data_{}, pos_{0} {}

    constexpr explicit FieldIterator(std::span<const char> data) noexcept
        : data_{data}, pos_{0} {}

    /// Get next field (returns invalid FieldView if no more fields)
    [[nodiscard]] [[gnu::hot]] constexpr FieldView next() noexcept {
        if (pos_ >= data_.size()) [[unlikely]] {
            return FieldView{};
        }

        const char* __restrict ptr = data_.data();

        // Parse tag number
        int tag = 0;
        while (pos_ < data_.size() && ptr[pos_] != fix::EQUALS) [[likely]] {
            char c = ptr[pos_];
            if (c < '0' || c > '9') [[unlikely]] {
                return FieldView{};  // Invalid tag
            }
            tag = tag * 10 + (c - '0');
            ++pos_;
        }

        if (pos_ >= data_.size() || ptr[pos_] != fix::EQUALS) [[unlikely]] {
            return FieldView{};  // Missing '='
        }
        ++pos_;  // Skip '='

        // Find value (until SOH)
        size_t value_start = pos_;
        while (pos_ < data_.size() && ptr[pos_] != fix::SOH) [[likely]] {
            ++pos_;
        }

        size_t value_len = pos_ - value_start;

        if (pos_ < data_.size()) [[likely]] {
            ++pos_;  // Skip SOH
        }

        return FieldView{tag, std::span<const char>{ptr + value_start, value_len}};
    }

    /// Check if more fields available
    [[nodiscard]] constexpr bool has_next() const noexcept {
        return pos_ < data_.size();
    }

    /// Get current position in buffer
    [[nodiscard]] constexpr size_t position() const noexcept {
        return pos_;
    }

    /// Reset to beginning
    constexpr void reset() noexcept {
        pos_ = 0;
    }

    /// Skip to a specific position
    constexpr void seek(size_t pos) noexcept {
        pos_ = pos < data_.size() ? pos : data_.size();
    }

private:
    std::span<const char> data_;
    size_t pos_;
};

// ============================================================================
// Field Lookup Table (for fast tag access)
// ============================================================================

/// Fixed-size lookup table for common tags (O(1) access)
/// Aligned to cache line boundary for optimal memory access
template <size_t MaxTag = 512>
class alignas(CACHE_LINE_SIZE) FieldTable {
public:
    constexpr FieldTable() noexcept {
        for (auto& entry : entries_) {
            entry = FieldView{};
        }
    }

    /// Set field value
    constexpr void set(int tag, std::span<const char> value) noexcept {
        if (tag > 0 && static_cast<size_t>(tag) < MaxTag) [[likely]] {
            entries_[tag] = FieldView{tag, value};
        }
    }

    /// Get field value (O(1))
    [[nodiscard]] [[gnu::hot]] constexpr FieldView get(int tag) const noexcept {
        if (tag > 0 && static_cast<size_t>(tag) < MaxTag) [[likely]] {
            return entries_[tag];
        }
        return FieldView{};
    }

    /// Check if tag exists
    [[nodiscard]] [[gnu::hot]] constexpr bool has(int tag) const noexcept {
        return tag > 0 && static_cast<size_t>(tag) < MaxTag &&
               entries_[tag].is_valid();
    }

    /// Get string value for tag
    [[nodiscard]] [[gnu::hot]] constexpr std::string_view get_string(int tag) const noexcept {
        return get(tag).as_string();
    }

    /// Get int value for tag
    [[nodiscard]] [[gnu::hot]] constexpr std::optional<int64_t> get_int(int tag) const noexcept {
        return get(tag).as_int();
    }

    /// Get char value for tag
    [[nodiscard]] [[gnu::hot]] constexpr char get_char(int tag) const noexcept {
        return get(tag).as_char();
    }

    /// Clear all entries
    constexpr void clear() noexcept {
        for (auto& entry : entries_) {
            entry = FieldView{};
        }
    }

private:
    std::array<FieldView, MaxTag> entries_;
};

// Static assertion: FieldTable should be cache-line aligned
static_assert(alignof(FieldTable<512>) >= CACHE_LINE_SIZE,
    "FieldTable must be cache-line aligned for optimal memory access");

// ============================================================================
// Utility Functions
// ============================================================================

/// Parse single field at specific position
[[nodiscard]] [[gnu::hot]]
constexpr FieldView parse_field_at(
    std::span<const char> data,
    size_t pos) noexcept
{
    FieldIterator iter{data};
    iter.seek(pos);
    return iter.next();
}

/// Find field by tag (linear scan)
[[nodiscard]] [[gnu::hot]]
constexpr FieldView find_field(
    std::span<const char> data,
    int target_tag) noexcept
{
    FieldIterator iter{data};
    while (iter.has_next()) [[likely]] {
        FieldView field = iter.next();
        if (field.tag == target_tag) [[unlikely]] {
            return field;
        }
    }
    return FieldView{};
}

// ============================================================================
// Static Assertions for Struct Layout
// ============================================================================

// FieldView should be compact (typically 24 bytes: 4 for tag + padding + 16 for span)
static_assert(sizeof(FieldView) <= 32,
    "FieldView should be compact for cache efficiency");

// FieldIterator should be compact
static_assert(sizeof(FieldIterator) <= 32,
    "FieldIterator should be compact for stack allocation");

} // namespace nfx
