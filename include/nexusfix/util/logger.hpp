/*
    NexusFIX High-Performance Logging

    Based on Quill - a low-latency asynchronous logging library
    Hot path latency: ~20ns

    Features:
    - Lock-free SPSC queue
    - Background thread for disk I/O
    - Zero heap allocation per log call
    - fmt-style formatting
*/

#pragma once

#ifdef NFX_HAS_LOGGING

#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>
#include <quill/Logger.h>
#include <quill/sinks/FileSink.h>
#include <quill/sinks/ConsoleSink.h>

#include <string>
#include <memory>
#include <filesystem>

namespace nfx::logging {

// Log levels matching Quill
enum class Level {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

// Logger configuration
struct LogConfig {
    std::string log_dir = "logs";
    std::string log_name = "nexusfix";
    bool console_output = true;
    bool file_output = true;
    Level min_level = Level::Info;
    size_t max_file_size = 10 * 1024 * 1024;  // 10MB
    uint32_t max_backup_files = 5;
};

// Initialize logging subsystem
inline void init(const LogConfig& config = {}) {
    // Start the backend logging thread
    quill::BackendOptions backend_options;
    backend_options.thread_name = "nfx_logger";
    quill::Backend::start(backend_options);

    // Create log directory if needed
    if (config.file_output) {
        std::filesystem::create_directories(config.log_dir);
    }

    // Setup sinks
    std::vector<std::shared_ptr<quill::Sink>> sinks;

    // Console sink
    if (config.console_output) {
        auto console_sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console");
        sinks.push_back(console_sink);
    }

    // File sink with rotation
    if (config.file_output) {
        std::string log_path = config.log_dir + "/" + config.log_name + ".log";

        quill::RotatingFileSinkConfig file_config;
        file_config.set_open_mode('a');
        file_config.set_filename_append_option(quill::FilenameAppendOption::StartDateTime);
        file_config.set_rotation_max_file_size(config.max_file_size);
        file_config.set_max_backup_files(config.max_backup_files);

        auto file_sink = quill::Frontend::create_or_get_sink<quill::RotatingFileSink>(
            log_path, file_config);
        sinks.push_back(file_sink);
    }

    // Create the logger
    quill::Logger* logger = quill::Frontend::create_or_get_logger("nfx", std::move(sinks));

    // Set log level
    switch (config.min_level) {
        case Level::Trace:    logger->set_log_level(quill::LogLevel::TraceL1); break;
        case Level::Debug:    logger->set_log_level(quill::LogLevel::Debug); break;
        case Level::Info:     logger->set_log_level(quill::LogLevel::Info); break;
        case Level::Warning:  logger->set_log_level(quill::LogLevel::Warning); break;
        case Level::Error:    logger->set_log_level(quill::LogLevel::Error); break;
        case Level::Critical: logger->set_log_level(quill::LogLevel::Critical); break;
    }
}

// Get the NexusFIX logger instance
inline quill::Logger* get() {
    return quill::Frontend::get_logger("nfx");
}

// Shutdown logging (call before exit)
inline void shutdown() {
    quill::Backend::stop();
}

// Flush all pending log messages
inline void flush() {
    if (auto* logger = get()) {
        logger->flush_log();
    }
}

} // namespace nfx::logging

// ============================================================================
// Convenience Macros
// ============================================================================

#define NFX_LOG_TRACE(fmt, ...)    LOG_TRACE_L1(nfx::logging::get(), fmt, ##__VA_ARGS__)
#define NFX_LOG_DEBUG(fmt, ...)    LOG_DEBUG(nfx::logging::get(), fmt, ##__VA_ARGS__)
#define NFX_LOG_INFO(fmt, ...)     LOG_INFO(nfx::logging::get(), fmt, ##__VA_ARGS__)
#define NFX_LOG_WARN(fmt, ...)     LOG_WARNING(nfx::logging::get(), fmt, ##__VA_ARGS__)
#define NFX_LOG_ERROR(fmt, ...)    LOG_ERROR(nfx::logging::get(), fmt, ##__VA_ARGS__)
#define NFX_LOG_CRITICAL(fmt, ...) LOG_CRITICAL(nfx::logging::get(), fmt, ##__VA_ARGS__)

// Conditional logging (only logs if condition is true)
#define NFX_LOG_INFO_IF(cond, fmt, ...) \
    do { if (cond) NFX_LOG_INFO(fmt, ##__VA_ARGS__); } while(0)

#define NFX_LOG_WARN_IF(cond, fmt, ...) \
    do { if (cond) NFX_LOG_WARN(fmt, ##__VA_ARGS__); } while(0)

#define NFX_LOG_ERROR_IF(cond, fmt, ...) \
    do { if (cond) NFX_LOG_ERROR(fmt, ##__VA_ARGS__); } while(0)

#else // NFX_HAS_LOGGING not defined

// ============================================================================
// No-op stubs when logging is disabled
// ============================================================================

namespace nfx::logging {

enum class Level { Trace, Debug, Info, Warning, Error, Critical };

struct LogConfig {
    std::string log_dir = "logs";
    std::string log_name = "nexusfix";
    bool console_output = true;
    bool file_output = true;
    Level min_level = Level::Info;
    size_t max_file_size = 10 * 1024 * 1024;
    uint32_t max_backup_files = 5;
};

inline void init(const LogConfig& = {}) {}
inline void* get() { return nullptr; }
inline void shutdown() {}
inline void flush() {}

} // namespace nfx::logging

#define NFX_LOG_TRACE(fmt, ...)    ((void)0)
#define NFX_LOG_DEBUG(fmt, ...)    ((void)0)
#define NFX_LOG_INFO(fmt, ...)     ((void)0)
#define NFX_LOG_WARN(fmt, ...)     ((void)0)
#define NFX_LOG_ERROR(fmt, ...)    ((void)0)
#define NFX_LOG_CRITICAL(fmt, ...) ((void)0)

#define NFX_LOG_INFO_IF(cond, fmt, ...)  ((void)0)
#define NFX_LOG_WARN_IF(cond, fmt, ...)  ((void)0)
#define NFX_LOG_ERROR_IF(cond, fmt, ...) ((void)0)

#endif // NFX_HAS_LOGGING
