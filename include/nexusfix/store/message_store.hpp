/*
    TICKET_024 Phase 2: Variant-based Message Store

    Replaces virtual dispatch with std::variant + std::visit for
    static polymorphism. Eliminates vtable lookups on store operations.

    Benefits:
    - No virtual dispatch overhead (~10-15 cycles saved per call)
    - Better inlining opportunities
    - Cache-friendly (no vtable indirection)
    - Compile-time type safety

    Reference: TICKET_024_RECURSIVE_TEMPLATE_DISPATCH.md
*/

#pragma once

#include <variant>
#include <span>
#include <optional>
#include <vector>
#include <string_view>
#include <cstdint>
#include <utility>

namespace nfx::store {

// Forward declarations - actual implementations included conditionally
class NullMessageStore;
class MemoryMessageStore;

// ============================================================================
// Store Type Enumeration
// ============================================================================

/// Available store implementation types
enum class StoreType {
    Null,       // No-op store for testing
    Memory,     // In-memory PMR-based store
    // Future: File, Mmap, etc.
};

// ============================================================================
// Null Store Implementation (Inline for variant)
// ============================================================================

/// Minimal no-op store implementation for variant usage
/// This is a self-contained implementation that doesn't require IMessageStore
class NullStore {
public:
    struct Stats {
        uint64_t messages_stored{0};
        uint64_t messages_retrieved{0};
        uint64_t bytes_stored{0};
        uint64_t store_failures{0};
    };

    explicit NullStore(std::string_view session_id = "NULL")
        : session_id_(session_id) {}

    [[nodiscard]] bool store(uint32_t, std::span<const char>) noexcept {
        return true;
    }

    [[nodiscard]] std::optional<std::vector<char>>
        retrieve(uint32_t) const noexcept {
        return std::nullopt;
    }

    [[nodiscard]] std::vector<std::vector<char>>
        retrieve_range(uint32_t, uint32_t) const noexcept {
        return {};
    }

    void set_next_sender_seq_num(uint32_t seq) noexcept {
        next_sender_seq_ = seq;
    }

    void set_next_target_seq_num(uint32_t seq) noexcept {
        next_target_seq_ = seq;
    }

    [[nodiscard]] uint32_t get_next_sender_seq_num() const noexcept {
        return next_sender_seq_;
    }

    [[nodiscard]] uint32_t get_next_target_seq_num() const noexcept {
        return next_target_seq_;
    }

    void reset() noexcept {
        next_sender_seq_ = 1;
        next_target_seq_ = 1;
    }

    void flush() noexcept {}

    [[nodiscard]] std::string_view session_id() const noexcept {
        return session_id_;
    }

    [[nodiscard]] Stats stats() const noexcept {
        return {};
    }

private:
    std::string session_id_;
    uint32_t next_sender_seq_{1};
    uint32_t next_target_seq_{1};
};

// ============================================================================
// Memory Store Implementation (Inline simplified version for variant)
// ============================================================================

/// Simplified in-memory store for variant usage
/// For full-featured PMR store, use MemoryMessageStore directly
class MemoryStore {
public:
    struct Stats {
        uint64_t messages_stored{0};
        uint64_t messages_retrieved{0};
        uint64_t bytes_stored{0};
        uint64_t store_failures{0};
    };

    struct Config {
        std::string session_id;
        size_t max_messages = 10000;
    };

    explicit MemoryStore(std::string_view session_id)
        : session_id_(session_id) {
        messages_.reserve(1000);
    }

    explicit MemoryStore(Config config)
        : session_id_(std::move(config.session_id))
        , max_messages_(config.max_messages) {
        messages_.reserve(std::min(max_messages_, size_t{1000}));
    }

    [[nodiscard]] bool store(uint32_t seq_num, std::span<const char> msg) noexcept {
        if (messages_.size() >= max_messages_) {
            ++stats_.store_failures;
            return false;
        }

        // Find insertion point or existing entry
        for (auto& [seq, data] : messages_) {
            if (seq == seq_num) {
                data.assign(msg.begin(), msg.end());
                return true;
            }
        }

        messages_.emplace_back(seq_num, std::vector<char>(msg.begin(), msg.end()));
        ++stats_.messages_stored;
        stats_.bytes_stored += msg.size();
        return true;
    }

    [[nodiscard]] std::optional<std::vector<char>>
        retrieve(uint32_t seq_num) const noexcept {
        for (const auto& [seq, data] : messages_) {
            if (seq == seq_num) {
                ++stats_.messages_retrieved;
                return data;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::vector<std::vector<char>>
        retrieve_range(uint32_t begin_seq, uint32_t end_seq) const noexcept {
        std::vector<std::vector<char>> result;
        uint32_t actual_end = (end_seq == 0) ? UINT32_MAX : end_seq;

        for (const auto& [seq, data] : messages_) {
            if (seq >= begin_seq && seq <= actual_end) {
                result.push_back(data);
                ++stats_.messages_retrieved;
            }
        }
        return result;
    }

    void set_next_sender_seq_num(uint32_t seq) noexcept {
        next_sender_seq_ = seq;
    }

    void set_next_target_seq_num(uint32_t seq) noexcept {
        next_target_seq_ = seq;
    }

    [[nodiscard]] uint32_t get_next_sender_seq_num() const noexcept {
        return next_sender_seq_;
    }

    [[nodiscard]] uint32_t get_next_target_seq_num() const noexcept {
        return next_target_seq_;
    }

    void reset() noexcept {
        messages_.clear();
        next_sender_seq_ = 1;
        next_target_seq_ = 1;
        stats_ = Stats{};
    }

    void flush() noexcept {}

    [[nodiscard]] std::string_view session_id() const noexcept {
        return session_id_;
    }

    [[nodiscard]] Stats stats() const noexcept {
        return stats_;
    }

private:
    std::string session_id_;
    std::vector<std::pair<uint32_t, std::vector<char>>> messages_;
    size_t max_messages_{10000};
    uint32_t next_sender_seq_{1};
    uint32_t next_target_seq_{1};
    mutable Stats stats_;
};

// ============================================================================
// Store Variant Type
// ============================================================================

/// Variant holding all store implementations
using StoreVariant = std::variant<NullStore, MemoryStore>;

// ============================================================================
// Unified Message Store (Variant-based)
// ============================================================================

/// Unified message store using std::variant for static polymorphism
/// Replaces virtual dispatch with std::visit
class MessageStore {
public:
    using Stats = MemoryStore::Stats;  // Use common stats type

    // ------------------------------------------------------------------------
    // Construction
    // ------------------------------------------------------------------------

    /// Create a null store
    MessageStore()
        : impl_(NullStore{}) {}

    /// Create a store of specified type
    explicit MessageStore(StoreType type, std::string_view session_id = "DEFAULT") {
        switch (type) {
            case StoreType::Null:
                impl_.emplace<NullStore>(session_id);
                break;
            case StoreType::Memory:
                impl_.emplace<MemoryStore>(session_id);
                break;
        }
    }

    /// Create from existing store implementation (move)
    template<typename StoreImpl>
        requires std::is_constructible_v<StoreVariant, StoreImpl>
    explicit MessageStore(StoreImpl&& store)
        : impl_(std::forward<StoreImpl>(store)) {}

    /// Create with in-place construction
    template<typename StoreImpl, typename... Args>
    explicit MessageStore(std::in_place_type_t<StoreImpl>, Args&&... args)
        : impl_(std::in_place_type<StoreImpl>, std::forward<Args>(args)...) {}

    // ------------------------------------------------------------------------
    // Store Operations (using std::visit)
    // ------------------------------------------------------------------------

    /// Store a message
    [[nodiscard]] bool store(uint32_t seq_num, std::span<const char> msg) noexcept {
        return std::visit([&](auto& s) { return s.store(seq_num, msg); }, impl_);
    }

    /// Retrieve a single message
    [[nodiscard]] std::optional<std::vector<char>>
        retrieve(uint32_t seq_num) const noexcept {
        return std::visit([&](const auto& s) { return s.retrieve(seq_num); }, impl_);
    }

    /// Retrieve a range of messages
    [[nodiscard]] std::vector<std::vector<char>>
        retrieve_range(uint32_t begin_seq, uint32_t end_seq) const noexcept {
        return std::visit([&](const auto& s) {
            return s.retrieve_range(begin_seq, end_seq);
        }, impl_);
    }

    // ------------------------------------------------------------------------
    // Sequence Number Management
    // ------------------------------------------------------------------------

    void set_next_sender_seq_num(uint32_t seq) noexcept {
        std::visit([&](auto& s) { s.set_next_sender_seq_num(seq); }, impl_);
    }

    void set_next_target_seq_num(uint32_t seq) noexcept {
        std::visit([&](auto& s) { s.set_next_target_seq_num(seq); }, impl_);
    }

    [[nodiscard]] uint32_t get_next_sender_seq_num() const noexcept {
        return std::visit([](const auto& s) {
            return s.get_next_sender_seq_num();
        }, impl_);
    }

    [[nodiscard]] uint32_t get_next_target_seq_num() const noexcept {
        return std::visit([](const auto& s) {
            return s.get_next_target_seq_num();
        }, impl_);
    }

    // ------------------------------------------------------------------------
    // Session Management
    // ------------------------------------------------------------------------

    void reset() noexcept {
        std::visit([](auto& s) { s.reset(); }, impl_);
    }

    void flush() noexcept {
        std::visit([](auto& s) { s.flush(); }, impl_);
    }

    [[nodiscard]] std::string_view session_id() const noexcept {
        return std::visit([](const auto& s) { return s.session_id(); }, impl_);
    }

    [[nodiscard]] Stats stats() const noexcept {
        return std::visit([](const auto& s) -> Stats {
            auto st = s.stats();
            return Stats{
                .messages_stored = st.messages_stored,
                .messages_retrieved = st.messages_retrieved,
                .bytes_stored = st.bytes_stored,
                .store_failures = st.store_failures
            };
        }, impl_);
    }

    // ------------------------------------------------------------------------
    // Type Inspection
    // ------------------------------------------------------------------------

    /// Get the current store type
    [[nodiscard]] StoreType type() const noexcept {
        return std::visit([](const auto& s) -> StoreType {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, NullStore>) {
                return StoreType::Null;
            } else if constexpr (std::is_same_v<T, MemoryStore>) {
                return StoreType::Memory;
            }
        }, impl_);
    }

    /// Check if this is a null store
    [[nodiscard]] bool is_null() const noexcept {
        return std::holds_alternative<NullStore>(impl_);
    }

    /// Check if this is a memory store
    [[nodiscard]] bool is_memory() const noexcept {
        return std::holds_alternative<MemoryStore>(impl_);
    }

    /// Get underlying variant (for advanced usage)
    [[nodiscard]] const StoreVariant& variant() const noexcept { return impl_; }
    [[nodiscard]] StoreVariant& variant() noexcept { return impl_; }

private:
    StoreVariant impl_;
};

// ============================================================================
// Factory Functions
// ============================================================================

/// Create a null message store
[[nodiscard]] inline MessageStore make_null_store(
    std::string_view session_id = "NULL") {
    return MessageStore{std::in_place_type<NullStore>, session_id};
}

/// Create a memory message store
[[nodiscard]] inline MessageStore make_memory_store(
    std::string_view session_id) {
    return MessageStore{std::in_place_type<MemoryStore>, session_id};
}

/// Create a memory message store with config
[[nodiscard]] inline MessageStore make_memory_store(
    MemoryStore::Config config) {
    return MessageStore{std::in_place_type<MemoryStore>, std::move(config)};
}

// ============================================================================
// Static Assertions
// ============================================================================

// Verify variant size is reasonable (no excessive padding)
static_assert(sizeof(StoreVariant) <= 256,
    "StoreVariant size exceeds expected maximum");

// Verify MessageStore is movable
static_assert(std::is_move_constructible_v<MessageStore>);
static_assert(std::is_move_assignable_v<MessageStore>);

} // namespace nfx::store
