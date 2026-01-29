/*
    NexusFIX Ranges Utilities

    Modern C++23 ranges and views for cleaner, more expressive code.

    Features:
    - Lazy evaluation with views
    - Composable range adapters
    - Zero-overhead abstractions

    IMPORTANT: Use ranges for:
    - Configuration parsing
    - Field iteration (non-hot-path)
    - Data transformation pipelines
    - Collection filtering

    DO NOT use in hot paths where manual loops are optimized.
*/

#pragma once

#include <ranges>
#include <algorithm>
#include <string_view>
#include <span>
#include <cstdint>

namespace nfx::util {

// ============================================================================
// Namespace Aliases for Convenience
// ============================================================================

namespace ranges = std::ranges;
namespace views = std::views;

// ============================================================================
// Common View Pipelines
// ============================================================================

/// Split string by delimiter and return range of string_views
[[nodiscard]] inline auto split_string(std::string_view str, char delimiter) {
    return str | views::split(delimiter)
               | views::transform([](auto&& part) {
                     return std::string_view(part.begin(), part.end());
                 });
}

/// Filter non-empty strings from a range
inline constexpr auto non_empty = views::filter([](std::string_view sv) {
    return !sv.empty();
});

/// Trim whitespace from string_view (returns trimmed view)
[[nodiscard]] constexpr std::string_view trim(std::string_view sv) noexcept {
    auto start = sv.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) return {};
    auto end = sv.find_last_not_of(" \t\r\n");
    return sv.substr(start, end - start + 1);
}

/// View adapter to trim strings
inline constexpr auto trimmed = views::transform([](std::string_view sv) {
    return trim(sv);
});

// ============================================================================
// Numeric Range Utilities
// ============================================================================

/// Generate indices for a container
[[nodiscard]] inline auto indices(size_t count) {
    return views::iota(size_t{0}, count);
}

/// Enumerate a range (returns pairs of index, value)
template<ranges::input_range R>
[[nodiscard]] auto enumerate(R&& range) {
    return views::zip(views::iota(size_t{0}), std::forward<R>(range));
}

/// Take first N elements
template<ranges::input_range R>
[[nodiscard]] auto take_n(R&& range, size_t n) {
    return std::forward<R>(range) | views::take(n);
}

/// Skip first N elements
template<ranges::input_range R>
[[nodiscard]] auto skip_n(R&& range, size_t n) {
    return std::forward<R>(range) | views::drop(n);
}

// ============================================================================
// Range Algorithms with Predicates
// ============================================================================

/// Find first element matching predicate
template<ranges::input_range R, typename Pred>
[[nodiscard]] auto find_if(R&& range, Pred predicate) {
    return ranges::find_if(std::forward<R>(range), predicate);
}

/// Check if any element matches predicate
template<ranges::input_range R, typename Pred>
[[nodiscard]] bool any_of(R&& range, Pred predicate) {
    return ranges::any_of(std::forward<R>(range), predicate);
}

/// Check if all elements match predicate
template<ranges::input_range R, typename Pred>
[[nodiscard]] bool all_of(R&& range, Pred predicate) {
    return ranges::all_of(std::forward<R>(range), predicate);
}

/// Check if no elements match predicate
template<ranges::input_range R, typename Pred>
[[nodiscard]] bool none_of(R&& range, Pred predicate) {
    return ranges::none_of(std::forward<R>(range), predicate);
}

/// Count elements matching predicate
template<ranges::input_range R, typename Pred>
[[nodiscard]] auto count_if(R&& range, Pred predicate) {
    return ranges::count_if(std::forward<R>(range), predicate);
}

// ============================================================================
// Span Utilities
// ============================================================================

/// Create span from container
template<typename Container>
[[nodiscard]] auto as_span(Container& container) {
    return std::span(container);
}

/// Create const span from container
template<typename Container>
[[nodiscard]] auto as_const_span(const Container& container) {
    return std::span(container);
}

/// Byte span view
[[nodiscard]] inline auto as_bytes(std::span<const char> data) {
    return std::as_bytes(data);
}

// ============================================================================
// FIX-specific Range Utilities
// ============================================================================

/// View for iterating FIX fields (tag=value\x01 format)
class FixFieldView {
public:
    class Iterator {
    public:
        using value_type = std::pair<int, std::string_view>;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::forward_iterator_tag;

        Iterator() = default;
        explicit Iterator(std::string_view data) : data_(data), current_(parse_next()) {}

        [[nodiscard]] value_type operator*() const { return current_; }

        Iterator& operator++() {
            advance();
            current_ = parse_next();
            return *this;
        }

        Iterator operator++(int) {
            auto tmp = *this;
            ++*this;
            return tmp;
        }

        [[nodiscard]] bool operator==(const Iterator& other) const {
            return data_.data() == other.data_.data() && data_.size() == other.data_.size();
        }

    private:
        std::string_view data_;
        value_type current_{0, {}};

        [[nodiscard]] value_type parse_next() const noexcept {
            if (data_.empty()) return {0, {}};

            auto eq_pos = data_.find('=');
            if (eq_pos == std::string_view::npos) return {0, {}};

            int tag = 0;
            for (size_t i = 0; i < eq_pos; ++i) {
                char c = data_[i];
                if (c < '0' || c > '9') return {0, {}};
                tag = tag * 10 + (c - '0');
            }

            auto soh_pos = data_.find('\x01', eq_pos + 1);
            if (soh_pos == std::string_view::npos) soh_pos = data_.size();

            return {tag, data_.substr(eq_pos + 1, soh_pos - eq_pos - 1)};
        }

        void advance() noexcept {
            auto soh_pos = data_.find('\x01');
            if (soh_pos == std::string_view::npos) {
                data_ = {};
            } else {
                data_ = data_.substr(soh_pos + 1);
            }
        }
    };

    explicit FixFieldView(std::string_view data) : data_(data) {}

    [[nodiscard]] Iterator begin() const { return Iterator(data_); }
    [[nodiscard]] Iterator end() const { return Iterator(); }

private:
    std::string_view data_;
};

/// Create view for iterating FIX fields
[[nodiscard]] inline FixFieldView fix_fields(std::string_view data) {
    return FixFieldView(data);
}

// ============================================================================
// Pipeline Combinators
// ============================================================================

/// Compose multiple view adapters
template<typename... Adapters>
[[nodiscard]] auto compose(Adapters&&... adapters) {
    return (... | std::forward<Adapters>(adapters));
}

} // namespace nfx::util
