/*
    NexusFIX In-Memory Message Store

    High-performance in-memory message store for FIX message persistence.
    Suitable for:
    - Development and testing
    - Short-lived sessions
    - Scenarios where durability is not critical

    For production with durability requirements, use MmapMessageStore.
*/

#pragma once

#include "nexusfix/store/i_message_store.hpp"

#include <unordered_map>
#include <shared_mutex>
#include <algorithm>

namespace nfx::store {

// ============================================================================
// Memory Message Store
// ============================================================================

/// In-memory message store with optional size limits
class MemoryMessageStore final : public IMessageStore {
public:
    /// Configuration for the memory store
    struct Config {
        std::string session_id;
        size_t max_messages = 10000;      // Maximum messages to retain
        size_t max_bytes = 100'000'000;   // 100MB max
        bool evict_oldest = true;         // Evict oldest when full
    };

    explicit MemoryMessageStore(Config config)
        : config_(std::move(config)) {}

    explicit MemoryMessageStore(std::string_view session_id)
        : config_{.session_id = std::string(session_id)} {}

    // ========================================================================
    // Message Storage
    // ========================================================================

    [[nodiscard]] bool store(uint32_t seq_num,
                            std::span<const char> msg) noexcept override {
        std::unique_lock lock(mutex_);

        // Check capacity
        if (messages_.size() >= config_.max_messages ||
            total_bytes_ + msg.size() > config_.max_bytes) {
            if (config_.evict_oldest && !messages_.empty()) {
                evict_oldest_locked();
            } else {
                ++stats_.store_failures;
                return false;
            }
        }

        // Store the message
        auto [it, inserted] = messages_.try_emplace(
            seq_num,
            std::vector<char>(msg.begin(), msg.end())
        );

        if (inserted) {
            total_bytes_ += msg.size();
            ++stats_.messages_stored;
            stats_.bytes_stored += msg.size();

            // Track min/max sequence for efficient range queries
            if (seq_num < min_seq_) min_seq_ = seq_num;
            if (seq_num > max_seq_) max_seq_ = seq_num;
        }

        return inserted;
    }

    [[nodiscard]] std::optional<std::vector<char>>
        retrieve(uint32_t seq_num) const noexcept override {
        std::shared_lock lock(mutex_);

        auto it = messages_.find(seq_num);
        if (it != messages_.end()) {
            ++stats_.messages_retrieved;
            return it->second;
        }
        return std::nullopt;
    }

    [[nodiscard]] std::vector<std::vector<char>>
        retrieve_range(uint32_t begin_seq, uint32_t end_seq) const noexcept override {
        std::shared_lock lock(mutex_);

        std::vector<std::vector<char>> result;

        // End sequence 0 means "to infinity"
        uint32_t actual_end = (end_seq == 0) ? max_seq_ : end_seq;

        for (uint32_t seq = begin_seq; seq <= actual_end; ++seq) {
            auto it = messages_.find(seq);
            if (it != messages_.end()) {
                result.push_back(it->second);
                ++stats_.messages_retrieved;
            }
        }

        return result;
    }

    // ========================================================================
    // Sequence Number Persistence
    // ========================================================================

    void set_next_sender_seq_num(uint32_t seq) noexcept override {
        next_sender_seq_.store(seq, std::memory_order_release);
    }

    void set_next_target_seq_num(uint32_t seq) noexcept override {
        next_target_seq_.store(seq, std::memory_order_release);
    }

    [[nodiscard]] uint32_t get_next_sender_seq_num() const noexcept override {
        return next_sender_seq_.load(std::memory_order_acquire);
    }

    [[nodiscard]] uint32_t get_next_target_seq_num() const noexcept override {
        return next_target_seq_.load(std::memory_order_acquire);
    }

    // ========================================================================
    // Session Management
    // ========================================================================

    void reset() noexcept override {
        std::unique_lock lock(mutex_);
        messages_.clear();
        total_bytes_ = 0;
        min_seq_ = UINT32_MAX;
        max_seq_ = 0;
        next_sender_seq_.store(1, std::memory_order_release);
        next_target_seq_.store(1, std::memory_order_release);
        stats_ = Stats{};
    }

    void flush() noexcept override {
        // No-op for memory store
    }

    [[nodiscard]] std::string_view session_id() const noexcept override {
        return config_.session_id;
    }

    [[nodiscard]] Stats stats() const noexcept override {
        return stats_;
    }

    // ========================================================================
    // Additional Methods
    // ========================================================================

    /// Get current message count
    [[nodiscard]] size_t message_count() const noexcept {
        std::shared_lock lock(mutex_);
        return messages_.size();
    }

    /// Get total bytes stored
    [[nodiscard]] size_t bytes_used() const noexcept {
        std::shared_lock lock(mutex_);
        return total_bytes_;
    }

    /// Check if a sequence number exists
    [[nodiscard]] bool contains(uint32_t seq_num) const noexcept {
        std::shared_lock lock(mutex_);
        return messages_.contains(seq_num);
    }

private:
    void evict_oldest_locked() noexcept {
        // Find and remove the oldest message (smallest sequence number)
        if (messages_.empty()) return;

        auto it = messages_.find(min_seq_);
        if (it != messages_.end()) {
            total_bytes_ -= it->second.size();
            messages_.erase(it);

            // Update min_seq_
            if (messages_.empty()) {
                min_seq_ = UINT32_MAX;
            } else {
                // Find new minimum
                min_seq_ = UINT32_MAX;
                for (const auto& [seq, _] : messages_) {
                    if (seq < min_seq_) min_seq_ = seq;
                }
            }
        }
    }

    Config config_;
    std::unordered_map<uint32_t, std::vector<char>> messages_;
    size_t total_bytes_{0};
    uint32_t min_seq_{UINT32_MAX};
    uint32_t max_seq_{0};

    std::atomic<uint32_t> next_sender_seq_{1};
    std::atomic<uint32_t> next_target_seq_{1};

    mutable std::shared_mutex mutex_;
    mutable Stats stats_;
};

} // namespace nfx::store
