/*
    NexusFIX Message Store Interface

    Required for FIX compliance:
    - Store outgoing messages for potential resend
    - Persist sequence numbers across sessions
    - Support ResendRequest (35=2) handling
*/

#pragma once

#include <cstdint>
#include <span>
#include <optional>
#include <vector>
#include <string_view>

namespace nfx::store {

// ============================================================================
// Message Store Interface
// ============================================================================

/// Interface for FIX message persistence
class IMessageStore {
public:
    virtual ~IMessageStore() = default;

    // ========================================================================
    // Message Storage
    // ========================================================================

    /// Store an outgoing message for potential resend
    /// @param seq_num The message sequence number
    /// @param msg The raw FIX message bytes
    /// @return true if stored successfully
    [[nodiscard]] virtual bool store(uint32_t seq_num,
                                     std::span<const char> msg) noexcept = 0;

    /// Retrieve a stored message by sequence number
    /// @param seq_num The sequence number to retrieve
    /// @return The message bytes, or empty optional if not found
    [[nodiscard]] virtual std::optional<std::vector<char>>
        retrieve(uint32_t seq_num) const noexcept = 0;

    /// Retrieve a range of messages for resend
    /// @param begin_seq Start sequence number (inclusive)
    /// @param end_seq End sequence number (inclusive, 0 = infinity)
    /// @return Vector of messages in order
    [[nodiscard]] virtual std::vector<std::vector<char>>
        retrieve_range(uint32_t begin_seq, uint32_t end_seq) const noexcept = 0;

    // ========================================================================
    // Sequence Number Persistence
    // ========================================================================

    /// Set the next expected sender (outbound) sequence number
    virtual void set_next_sender_seq_num(uint32_t seq) noexcept = 0;

    /// Set the next expected target (inbound) sequence number
    virtual void set_next_target_seq_num(uint32_t seq) noexcept = 0;

    /// Get the next sender sequence number
    [[nodiscard]] virtual uint32_t get_next_sender_seq_num() const noexcept = 0;

    /// Get the next target sequence number
    [[nodiscard]] virtual uint32_t get_next_target_seq_num() const noexcept = 0;

    // ========================================================================
    // Session Management
    // ========================================================================

    /// Reset the store for a new session (clears all messages)
    virtual void reset() noexcept = 0;

    /// Flush any pending writes to persistent storage
    virtual void flush() noexcept = 0;

    /// Get the session identifier
    [[nodiscard]] virtual std::string_view session_id() const noexcept = 0;

    // ========================================================================
    // Store Statistics
    // ========================================================================

    struct Stats {
        uint64_t messages_stored{0};
        uint64_t messages_retrieved{0};
        uint64_t bytes_stored{0};
        uint64_t store_failures{0};
    };

    /// Get store statistics
    [[nodiscard]] virtual Stats stats() const noexcept = 0;
};

// ============================================================================
// Null Message Store (No-op implementation)
// ============================================================================

/// No-op message store for testing or when persistence is not required
class NullMessageStore final : public IMessageStore {
public:
    explicit NullMessageStore(std::string_view session_id = "NULL")
        : session_id_(session_id) {}

    [[nodiscard]] bool store(uint32_t, std::span<const char>) noexcept override {
        return true;
    }

    [[nodiscard]] std::optional<std::vector<char>>
        retrieve(uint32_t) const noexcept override {
        return std::nullopt;
    }

    [[nodiscard]] std::vector<std::vector<char>>
        retrieve_range(uint32_t, uint32_t) const noexcept override {
        return {};
    }

    void set_next_sender_seq_num(uint32_t seq) noexcept override {
        next_sender_seq_ = seq;
    }

    void set_next_target_seq_num(uint32_t seq) noexcept override {
        next_target_seq_ = seq;
    }

    [[nodiscard]] uint32_t get_next_sender_seq_num() const noexcept override {
        return next_sender_seq_;
    }

    [[nodiscard]] uint32_t get_next_target_seq_num() const noexcept override {
        return next_target_seq_;
    }

    void reset() noexcept override {
        next_sender_seq_ = 1;
        next_target_seq_ = 1;
    }

    void flush() noexcept override {}

    [[nodiscard]] std::string_view session_id() const noexcept override {
        return session_id_;
    }

    [[nodiscard]] Stats stats() const noexcept override {
        return {};
    }

private:
    std::string session_id_;
    uint32_t next_sender_seq_{1};
    uint32_t next_target_seq_{1};
};

} // namespace nfx::store
