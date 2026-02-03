/*
    Modern C++23 Multi-Channel Logging Module for QuantNexus
    Adapted from license-server logdump.hpp

    Features:
    - Three-channel logging: OPS (operational), TRADE (trading), PLUGIN (plugin events)
    - Type-safe logging with C++20 Concepts
    - std::format / std::source_location for context
    - std::print / std::println (C++23) with fallback for console output
    - Async logging with lock-free queue
    - Daily log rotation with YYYYMMDD naming
    - JSON structured logging for trade channel
    - Thread-safe, high-performance design
*/

#pragma once

#include <string>
#include <string_view>
#include <format>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <atomic>
#include <thread>
#include <queue>
#include <condition_variable>
#include <source_location>
#include <functional>
#include <memory>
#include <concepts>
#include <type_traits>
#include <optional>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>

// ============================================================================
// C++23 <print> Header Detection
// ============================================================================
#if __has_include(<print>)
    #include <print>
    #if defined(__cpp_lib_print) && __cpp_lib_print >= 202207L
        #define LOGDUMP_HAS_STD_PRINT 1
    #else
        #define LOGDUMP_HAS_STD_PRINT 0
    #endif
#else
    #define LOGDUMP_HAS_STD_PRINT 0
#endif

namespace quantnexus::logging {

// ============================================================================
// Log Levels
// ============================================================================

enum class Level : uint8_t {
    DEBUG = 0,
    INFO  = 1,
    WARN  = 2,
    ERROR = 3,
    NONE  = 255
};

[[nodiscard]] constexpr std::string_view level_to_string(Level level) noexcept {
    switch (level) {
        case Level::DEBUG: return "DBG";
        case Level::INFO:  return "INF";
        case Level::WARN:  return "WRN";
        case Level::ERROR: return "ERR";
        default:           return "???";
    }
}

// ============================================================================
// Log Channels (adapted for QuantNexus)
// ============================================================================

enum class Channel : uint8_t {
    OPS = 0,      // Operational logs (server health, gRPC, plugin lifecycle)
    TRADE = 1,    // Trading logs (backtest results, orders, positions)
    PLUGIN = 2    // Plugin events (register, heartbeat, errors)
};

// ============================================================================
// Plugin Event (for PLUGIN channel)
// ============================================================================

struct LogPluginEvent {
    std::string plugin_id;
    std::string action;      // "REGISTER", "UNREGISTER", "HEARTBEAT", "ERROR"
    std::string state;       // "starting", "ready", "busy", "error", "stopping"
    std::string details;
    std::chrono::microseconds duration{0};
};

// ============================================================================
// Trade Event (for TRADE channel)
// ============================================================================

struct TradeEvent {
    std::string task_id;
    std::string strategy_id;
    std::string symbol;
    std::string action;      // "BUY", "SELL", "BACKTEST_START", "BACKTEST_END"
    double price{0.0};
    double quantity{0.0};
    double pnl{0.0};
    std::chrono::microseconds duration{0};
};

// ============================================================================
// Concepts for Type-Safe Serialization
// ============================================================================

template<typename T>
concept Arithmetic = std::is_arithmetic_v<T> && !std::is_same_v<std::decay_t<T>, bool>
                     && !std::is_same_v<std::decay_t<T>, char>;

template<typename T>
concept StringLike = std::is_convertible_v<T, std::string_view>;

template<typename T>
concept Container = requires(T t) {
    std::begin(t);
    std::end(t);
    typename T::value_type;
} && !StringLike<T>;

template<typename T>
concept MapLike = Container<T> && requires {
    typename T::key_type;
    typename T::mapped_type;
};

template<typename T>
concept SmartPointer = requires(T t) {
    { t.get() } -> std::convertible_to<typename T::element_type*>;
    { static_cast<bool>(t) };
};

template<typename T>
concept OptionalLike = requires(T t) {
    { t.has_value() } -> std::same_as<bool>;
    { *t };
};

// ============================================================================
// Type-Safe Serialization (dumps)
// ============================================================================

// Forward declaration
template<typename T>
std::string dumps(const T& value);

// Arithmetic types
template<Arithmetic T>
std::string dumps(const T& value) {
    if constexpr (std::is_floating_point_v<T>) {
        return std::format("{:.6g}", value);
    } else {
        return std::to_string(value);
    }
}

// Bool
inline std::string dumps(bool value) {
    return value ? "true" : "false";
}

// Char
inline std::string dumps(char value) {
    return std::string(1, value);
}

// String-like types
template<StringLike T>
std::string dumps(const T& value) {
    return std::string(value);
}

// nullptr
inline std::string dumps(std::nullptr_t) {
    return "null";
}

// Raw pointers
template<typename T>
std::string dumps(const T* ptr) requires (!std::is_same_v<T, char>) {
    if (ptr) {
        return std::format("ptr({})", dumps(*ptr));
    }
    return "nullptr";
}

// Smart pointers
template<SmartPointer T>
std::string dumps(const T& ptr) {
    if (ptr) {
        return std::format("ptr({})", dumps(*ptr));
    }
    return "nullptr";
}

// Optional-like types
template<OptionalLike T>
std::string dumps(const T& opt) {
    if (opt.has_value()) {
        return dumps(*opt);
    }
    return "nullopt";
}

// Map-like containers
template<MapLike T>
std::string dumps(const T& container) {
    std::string result = "{";
    bool first = true;
    for (const auto& [key, value] : container) {
        if (!first) result += ", ";
        result += std::format("{}: {}", dumps(key), dumps(value));
        first = false;
    }
    result += "}";
    return result;
}

// Other containers (vector, set, etc.)
template<Container T>
std::string dumps(const T& container) requires (!MapLike<T>) {
    std::string result = "[";
    bool first = true;
    for (const auto& item : container) {
        if (!first) result += ", ";
        result += dumps(item);
        first = false;
    }
    result += "]";
    return result;
}

// Pairs
template<typename T1, typename T2>
std::string dumps(const std::pair<T1, T2>& p) {
    return std::format("({}, {})", dumps(p.first), dumps(p.second));
}

// ============================================================================
// Utility Functions
// ============================================================================

// Escape JSON special characters
[[nodiscard]] inline std::string escape_json(std::string_view str) {
    std::string result;
    result.reserve(str.size());
    for (char c : str) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:   result += c; break;
        }
    }
    return result;
}

// Get current date string (YYYYMMDD)
[[nodiscard]] inline std::string get_date_string() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &time_t);
#else
    localtime_r(&time_t, &tm_buf);
#endif
    return std::format("{:04d}{:02d}{:02d}",
        tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday);
}

// Get current timestamp string
[[nodiscard]] inline std::string get_timestamp_string() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &time_t);
#else
    localtime_r(&time_t, &tm_buf);
#endif
    return std::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}.{:03d}",
        tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
        tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
        static_cast<int>(ms.count()));
}

// Get ISO8601 timestamp for JSON
[[nodiscard]] inline std::string get_iso_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &time_t);
#else
    localtime_r(&time_t, &tm_buf);
#endif
    return std::format("{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{:03d}Z",
        tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
        tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
        static_cast<int>(ms.count()));
}

// ============================================================================
// Channel Writer (handles one log channel)
// ============================================================================

class ChannelWriter {
public:
    ChannelWriter(std::string_view prefix, const std::filesystem::path& dir)
        : prefix_(prefix), dir_(dir) {
        std::filesystem::create_directories(dir_);
    }

    void write(std::string_view content) {
        std::lock_guard lock(mutex_);

        // Check if we need to rotate (new day)
        std::string today = get_date_string();
        if (today != current_date_) {
            current_date_ = today;
            // Close old file, new one will be opened on next write
        }

        std::filesystem::path log_path = dir_ / std::format("{}_{}.log", prefix_, current_date_);
        std::ofstream file(log_path, std::ios::app);
        if (file) {
            file << content << '\n';
        }
    }

private:
    std::string prefix_;
    std::filesystem::path dir_;
    std::string current_date_;
    std::mutex mutex_;
};

// ============================================================================
// Log Entry (for OPS channel)
// ============================================================================

struct LogEntry {
    Level level;
    std::string message;
    std::string file;
    std::string function;
    uint32_t line;
    std::chrono::system_clock::time_point timestamp;
    std::thread::id thread_id;

    [[nodiscard]] std::string format() const {
        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp.time_since_epoch()) % 1000;

        std::tm tm_buf{};
#if defined(_WIN32)
        localtime_s(&tm_buf, &time_t);
#else
        localtime_r(&time_t, &tm_buf);
#endif

        // Extract just filename from path
        std::string_view filename = file;
        if (auto pos = filename.rfind('/'); pos != std::string_view::npos) {
            filename = filename.substr(pos + 1);
        } else if (auto pos2 = filename.rfind('\\'); pos2 != std::string_view::npos) {
            filename = filename.substr(pos2 + 1);
        }

        return std::format("{} | {:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}.{:03d} | {:>20}:{:<5} | {}",
            level_to_string(level),
            tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
            tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec,
            static_cast<int>(ms.count()),
            filename, line,
            message);
    }
};

// ============================================================================
// MultiChannelLogger (Singleton, Thread-Safe, Async)
// ============================================================================

class MultiChannelLogger {
public:
    // Singleton access
    static MultiChannelLogger& instance() {
        static MultiChannelLogger logger;
        return logger;
    }

    // Configuration
    void set_level(Level level) noexcept { min_level_ = level; }
    [[nodiscard]] Level level() const noexcept { return min_level_; }

    void set_console_output(bool enabled) noexcept { console_output_ = enabled; }

    void set_ops_directory(const std::filesystem::path& dir) {
        std::lock_guard lock(config_mutex_);
        ops_dir_ = dir;
        ops_writer_ = std::make_unique<ChannelWriter>("core", dir);
    }

    void set_trade_directory(const std::filesystem::path& dir) {
        std::lock_guard lock(config_mutex_);
        trade_dir_ = dir;
        trade_writer_ = std::make_unique<ChannelWriter>("trade", dir);
    }

    void set_plugin_directory(const std::filesystem::path& dir) {
        std::lock_guard lock(config_mutex_);
        plugin_dir_ = dir;
        plugin_writer_ = std::make_unique<ChannelWriter>("plugin", dir);
    }

    void enable_ops(bool enabled) noexcept { ops_enabled_ = enabled; }
    void enable_trade(bool enabled) noexcept { trade_enabled_ = enabled; }
    void enable_plugin(bool enabled) noexcept { plugin_enabled_ = enabled; }

    // Start/stop async logging
    void start() {
        if (running_.exchange(true)) return;
        writer_thread_ = std::jthread([this](std::stop_token st) {
            write_loop(st);
        });
    }

    void stop() {
        if (!running_.exchange(false)) return;
        queue_cv_.notify_all();
        if (writer_thread_.joinable()) {
            writer_thread_.request_stop();
            writer_thread_.join();
        }
        flush();
    }

    // Synchronous flush
    void flush() {
        std::lock_guard lock(queue_mutex_);
        while (!ops_queue_.empty()) {
            write_ops_entry(ops_queue_.front());
            ops_queue_.pop();
        }
    }

    // ========================================================================
    // OPS Channel - Operational logging
    // ========================================================================

    void log_ops(Level level, std::string message,
                 const std::source_location& loc = std::source_location::current()) {
        if (!ops_enabled_ || level < min_level_) return;

        LogEntry entry{
            .level = level,
            .message = std::move(message),
            .file = loc.file_name(),
            .function = loc.function_name(),
            .line = loc.line(),
            .timestamp = std::chrono::system_clock::now(),
            .thread_id = std::this_thread::get_id()
        };

        if (running_) {
            std::lock_guard lock(queue_mutex_);
            ops_queue_.push(std::move(entry));
            queue_cv_.notify_one();
        } else {
            write_ops_entry(entry);
        }
    }

    template<typename... Args>
    void log_ops(Level level, const std::source_location& loc, Args&&... args) {
        if (!ops_enabled_ || level < min_level_) return;

        std::string message;
        if constexpr (sizeof...(args) > 0) {
            message = ((dumps(std::forward<Args>(args)) + " ") + ...);
            if (!message.empty()) message.pop_back();
        }

        log_ops(level, std::move(message), loc);
    }

    // ========================================================================
    // TRADE Channel - Trading/Backtest logging (JSON format)
    // ========================================================================

    void log_trade(const TradeEvent& event) {
        if (!trade_enabled_) return;

        std::string json = std::format(
            R"({{"ts":"{}","task":"{}","strategy":"{}","symbol":"{}","action":"{}","price":{},"qty":{},"pnl":{},"duration_us":{}}})",
            get_iso_timestamp(),
            escape_json(event.task_id),
            escape_json(event.strategy_id),
            escape_json(event.symbol),
            escape_json(event.action),
            event.price,
            event.quantity,
            event.pnl,
            event.duration.count()
        );

        if (console_output_) {
            std::lock_guard lock(console_mutex_);
#if LOGDUMP_HAS_STD_PRINT
            std::println("[TRADE] {}", json);
#else
            std::cout << "[TRADE] " << json << '\n';
#endif
        }

        if (trade_writer_) {
            trade_writer_->write(json);
        }
    }

    // ========================================================================
    // PLUGIN Channel - Plugin events (JSON format)
    // ========================================================================

    void log_plugin(const LogPluginEvent& event) {
        if (!plugin_enabled_) return;

        std::string json = std::format(
            R"({{"ts":"{}","plugin":"{}","action":"{}","state":"{}","details":"{}","duration_us":{}}})",
            get_iso_timestamp(),
            escape_json(event.plugin_id),
            escape_json(event.action),
            escape_json(event.state),
            escape_json(event.details),
            event.duration.count()
        );

        if (console_output_) {
            std::lock_guard lock(console_mutex_);
#if LOGDUMP_HAS_STD_PRINT
            if (event.action == "ERROR") {
                std::println(stderr, "[PLUGIN] {}", json);
            } else {
                std::println("[PLUGIN] {}", json);
            }
#else
            if (event.action == "ERROR") {
                std::cerr << "[PLUGIN] " << json << '\n';
            } else {
                std::cout << "[PLUGIN] " << json << '\n';
            }
#endif
        }

        if (plugin_writer_) {
            plugin_writer_->write(json);
        }
    }

    // Disable copy/move
    MultiChannelLogger(const MultiChannelLogger&) = delete;
    MultiChannelLogger& operator=(const MultiChannelLogger&) = delete;
    MultiChannelLogger(MultiChannelLogger&&) = delete;
    MultiChannelLogger& operator=(MultiChannelLogger&&) = delete;

private:
    MultiChannelLogger() {
        // Initialize default writers
        ops_writer_ = std::make_unique<ChannelWriter>("core", ops_dir_);
        trade_writer_ = std::make_unique<ChannelWriter>("trade", trade_dir_);
        plugin_writer_ = std::make_unique<ChannelWriter>("plugin", plugin_dir_);
    }

    ~MultiChannelLogger() {
        stop();
    }

    void write_loop(std::stop_token st) {
        while (!st.stop_requested() || !ops_queue_.empty()) {
            std::unique_lock lock(queue_mutex_);
            queue_cv_.wait(lock, [this, &st] {
                return st.stop_requested() || !ops_queue_.empty();
            });

            while (!ops_queue_.empty()) {
                auto entry = std::move(ops_queue_.front());
                ops_queue_.pop();
                lock.unlock();
                write_ops_entry(entry);
                lock.lock();
            }
        }
    }

    void write_ops_entry(const LogEntry& entry) {
        std::string formatted = entry.format();

        if (console_output_) {
            std::lock_guard lock(console_mutex_);
#if LOGDUMP_HAS_STD_PRINT
            if (entry.level >= Level::WARN) {
                std::println(stderr, "{}", formatted);
            } else {
                std::println("{}", formatted);
            }
#else
            if (entry.level >= Level::WARN) {
                std::cerr << formatted << '\n';
            } else {
                std::cout << formatted << '\n';
            }
#endif
        }

        if (ops_writer_) {
            ops_writer_->write(formatted);
        }
    }

    // Configuration
    std::atomic<Level> min_level_{Level::INFO};
    std::atomic<bool> console_output_{true};
    std::atomic<bool> ops_enabled_{true};
    std::atomic<bool> trade_enabled_{true};
    std::atomic<bool> plugin_enabled_{true};

    std::filesystem::path ops_dir_{"logs"};
    std::filesystem::path trade_dir_{"logs/trade"};
    std::filesystem::path plugin_dir_{"logs/plugin"};
    std::mutex config_mutex_;

    // Channel writers
    std::unique_ptr<ChannelWriter> ops_writer_;
    std::unique_ptr<ChannelWriter> trade_writer_;
    std::unique_ptr<ChannelWriter> plugin_writer_;

    // Async queue (for OPS channel)
    std::atomic<bool> running_{false};
    std::queue<LogEntry> ops_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::jthread writer_thread_;

    // Console mutex
    std::mutex console_mutex_;
};

// ============================================================================
// Backward Compatibility: Logger alias
// ============================================================================

using Logger = MultiChannelLogger;

// ============================================================================
// OPS Channel Macros (Operational Logging)
// ============================================================================

#define LOG_OPS_DEBUG(...) \
    ::quantnexus::logging::MultiChannelLogger::instance().log_ops(::quantnexus::logging::Level::DEBUG, std::source_location::current() __VA_OPT__(,) __VA_ARGS__)

#define LOG_OPS_INFO(...) \
    ::quantnexus::logging::MultiChannelLogger::instance().log_ops(::quantnexus::logging::Level::INFO, std::source_location::current() __VA_OPT__(,) __VA_ARGS__)

#define LOG_OPS_WARN(...) \
    ::quantnexus::logging::MultiChannelLogger::instance().log_ops(::quantnexus::logging::Level::WARN, std::source_location::current() __VA_OPT__(,) __VA_ARGS__)

#define LOG_OPS_ERROR(...) \
    ::quantnexus::logging::MultiChannelLogger::instance().log_ops(::quantnexus::logging::Level::ERROR, std::source_location::current() __VA_OPT__(,) __VA_ARGS__)

// Format-style OPS logging
#define LOGF_OPS_DEBUG(fmt, ...) \
    ::quantnexus::logging::MultiChannelLogger::instance().log_ops(::quantnexus::logging::Level::DEBUG, std::format(fmt __VA_OPT__(,) __VA_ARGS__))

#define LOGF_OPS_INFO(fmt, ...) \
    ::quantnexus::logging::MultiChannelLogger::instance().log_ops(::quantnexus::logging::Level::INFO, std::format(fmt __VA_OPT__(,) __VA_ARGS__))

#define LOGF_OPS_WARN(fmt, ...) \
    ::quantnexus::logging::MultiChannelLogger::instance().log_ops(::quantnexus::logging::Level::WARN, std::format(fmt __VA_OPT__(,) __VA_ARGS__))

#define LOGF_OPS_ERROR(fmt, ...) \
    ::quantnexus::logging::MultiChannelLogger::instance().log_ops(::quantnexus::logging::Level::ERROR, std::format(fmt __VA_OPT__(,) __VA_ARGS__))

// ============================================================================
// Backward Compatibility Macros (map to OPS channel)
// ============================================================================

#define LOG_DEBUG(...) LOG_OPS_DEBUG(__VA_ARGS__)
#define LOG_INFO(...)  LOG_OPS_INFO(__VA_ARGS__)
#define LOG_WARN(...)  LOG_OPS_WARN(__VA_ARGS__)
#define LOG_ERROR(...) LOG_OPS_ERROR(__VA_ARGS__)

#define LOGF_DEBUG(fmt, ...) LOGF_OPS_DEBUG(fmt __VA_OPT__(,) __VA_ARGS__)
#define LOGF_INFO(fmt, ...)  LOGF_OPS_INFO(fmt __VA_OPT__(,) __VA_ARGS__)
#define LOGF_WARN(fmt, ...)  LOGF_OPS_WARN(fmt __VA_OPT__(,) __VA_ARGS__)
#define LOGF_ERROR(fmt, ...) LOGF_OPS_ERROR(fmt __VA_OPT__(,) __VA_ARGS__)

// ============================================================================
// TRADE Channel Macro
// ============================================================================

#define LOG_TRADE(event) \
    ::quantnexus::logging::MultiChannelLogger::instance().log_trade(event)

// ============================================================================
// PLUGIN Channel Macro
// ============================================================================

#define LOG_PLUGIN(event) \
    ::quantnexus::logging::MultiChannelLogger::instance().log_plugin(event)

// ============================================================================
// Backtest Timer (RAII for timing backtests)
// ============================================================================

class BacktestTimer {
public:
    explicit BacktestTimer(TradeEvent& event)
        : event_(event), start_(std::chrono::steady_clock::now()) {}

    ~BacktestTimer() {
        auto end = std::chrono::steady_clock::now();
        event_.duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_);
        LOG_TRADE(event_);
    }

    // Disable copy/move
    BacktestTimer(const BacktestTimer&) = delete;
    BacktestTimer& operator=(const BacktestTimer&) = delete;

private:
    TradeEvent& event_;
    std::chrono::steady_clock::time_point start_;
};

} // namespace quantnexus::logging
