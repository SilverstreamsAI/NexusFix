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

#include <mutex>
#include <shared_mutex>
#include <algorithm>
#include <memory_resource>

// Use Abseil flat_hash_map for ~3x faster lookups (Swiss Tables with SIMD probing)
#if defined(NFX_HAS_ABSEIL) && NFX_HAS_ABSEIL
    #include <absl/container/flat_hash_map.h>
    #define NFX_USE_ABSEIL_HASH_MAP 1
#else
    #include <unordered_map>
    #define NFX_USE_ABSEIL_HASH_MAP 0
#endif

namespace nfx::store {

// ============================================================================
// Hash Map Type Selection
// ============================================================================

/// Hash map type - absl::flat_hash_map when available, std::unordered_map otherwise
template<typename K, typename V>
#if NFX_USE_ABSEIL_HASH_MAP
using HashMap = absl::flat_hash_map<K, V>;
#else
using HashMap = std::unordered_map<K, V>;
#endif

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
        size_t pool_size_bytes = 64 * 1024 * 1024;  // 64MB PMR pool
        std::pmr::memory_resource* upstream_resource = nullptr;  // Optional: mimalloc SessionHeap
    };

    /// PMR pool metrics for monitoring
    struct PoolMetrics {
        size_t pool_capacity{0};          // Total pool size in bytes
        size_t bytes_allocated{0};        // Current bytes allocated from pool
        size_t peak_usage{0};             // High water mark
        size_t reset_count{0};            // Number of pool resets
    };

    explicit MemoryMessageStore(Config config)
        : config_(std::move(config))
        , pool_storage_(config_.upstream_resource ? 0 : config_.pool_size_bytes)
        , pool_(pool_storage_.data(), pool_storage_.size(),
                config_.upstream_resource ? config_.upstream_resource
                                         : std::pmr::null_memory_resource())
        , pool_metrics_{.pool_capacity = config_.pool_size_bytes} {}

    explicit MemoryMessageStore(std::string_view session_id)
        : MemoryMessageStore(Config{.session_id = std::string(session_id)}) {}

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

        // Allocate message buffer from PMR pool (O(1), no syscall)
        std::pmr::polymorphic_allocator<char> alloc(&pool_);
        PmrVector pmr_msg(alloc);
        try {
            pmr_msg.assign(msg.begin(), msg.end());
        } catch (const std::bad_alloc&) {
            // Pool exhausted
            ++stats_.store_failures;
            return false;
        }

        // Store the message
        auto [it, inserted] = messages_.try_emplace(
            seq_num,
            std::move(pmr_msg)
        );

        if (inserted) {
            total_bytes_ += msg.size();
            ++stats_.messages_stored;
            stats_.bytes_stored += msg.size();

            // Update pool metrics
            pool_metrics_.bytes_allocated += msg.size();
            if (pool_metrics_.bytes_allocated > pool_metrics_.peak_usage) {
                pool_metrics_.peak_usage = pool_metrics_.bytes_allocated;
            }

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
            // Copy from PMR storage to regular vector for return
            return std::vector<char>(it->second.begin(), it->second.end());
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
                // Copy from PMR storage to regular vector
                result.emplace_back(it->second.begin(), it->second.end());
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

        // Release PMR pool - O(1) memory reclamation
        pool_.release();
        ++pool_metrics_.reset_count;
        pool_metrics_.bytes_allocated = 0;

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

    /// Get PMR pool metrics for monitoring
    [[nodiscard]] PoolMetrics pool_metrics() const noexcept {
        std::shared_lock lock(mutex_);
        return pool_metrics_;
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
    /// PMR-allocated vector type for message storage
    using PmrVector = std::pmr::vector<char>;

    void evict_oldest_locked() noexcept {
        // Find and remove the oldest message (smallest sequence number)
        if (messages_.empty()) return;

        auto it = messages_.find(min_seq_);
        if (it != messages_.end()) {
            size_t msg_size = it->second.size();
            total_bytes_ -= msg_size;
            // Note: PMR monotonic buffer doesn't reclaim on erase,
            // memory is reclaimed on pool reset()
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

    // PMR pool for zero-syscall message allocation
    std::vector<char> pool_storage_;              // Backing storage
    std::pmr::monotonic_buffer_resource pool_;    // PMR resource

    HashMap<uint32_t, PmrVector> messages_;       // Swiss Tables when Abseil available
    size_t total_bytes_{0};
    uint32_t min_seq_{UINT32_MAX};
    uint32_t max_seq_{0};

    std::atomic<uint32_t> next_sender_seq_{1};
    std::atomic<uint32_t> next_target_seq_{1};

    mutable std::shared_mutex mutex_;
    mutable Stats stats_;
    mutable PoolMetrics pool_metrics_;
};

} // namespace nfx::store
