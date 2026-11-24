/**
 * @file common.hpp
 * @brief Common utilities, types, and helpers
 */

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <functional>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <optional>
#include <variant>
#include <filesystem>
#include <future>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

namespace phone_projection {
namespace common {

// ============================================================================
// Logging
// ============================================================================

enum class LogLevel {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
    Critical = 5,
    Off = 6
};

class Logger {
public:
    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    void set_level(LogLevel level) { level_.store(level); }
    LogLevel get_level() const { return level_.load(); }

    void set_pattern(const std::string& pattern) { pattern_ = pattern; }

    template<typename... Args>
    void log(LogLevel level, const char* file, int line, [[maybe_unused]] const char* func, fmt::format_string<Args...> fmt, Args&&... args) {
        if (level < level_.load()) return;

        std::string message = fmt::format(fmt, std::forward<Args>(args)...);
        std::string prefix = fmt::format("[{}] [{}:{}] ", level_to_string(level),
            std::filesystem::path(file).filename().string(), line);

        std::lock_guard<std::mutex> lock(mutex_);
        fmt::print("{}{}\n", prefix, message);
    }

private:
    Logger() = default;
    std::atomic<LogLevel> level_{LogLevel::Info};
    std::string pattern_;
    std::mutex mutex_;

    static const char* level_to_string(LogLevel level) {
        switch (level) {
            case LogLevel::Trace: return "TRC";
            case LogLevel::Debug: return "DBG";
            case LogLevel::Info: return "INF";
            case LogLevel::Warn: return "WRN";
            case LogLevel::Error: return "ERR";
            case LogLevel::Critical: return "CRT";
            default: return "???";
        }
    }
};

// Logging macros
#define PP_LOG_TRACE(...)  phone_projection::common::Logger::instance().log(phone_projection::common::LogLevel::Trace, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define PP_LOG_DEBUG(...)  phone_projection::common::Logger::instance().log(phone_projection::common::LogLevel::Debug, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define PP_LOG_INFO(...)   phone_projection::common::Logger::instance().log(phone_projection::common::LogLevel::Info, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define PP_LOG_WARN(...)   phone_projection::common::Logger::instance().log(phone_projection::common::LogLevel::Warn, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define PP_LOG_ERROR(...)  phone_projection::common::Logger::instance().log(phone_projection::common::LogLevel::Error, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define PP_LOG_CRITICAL(...) phone_projection::common::Logger::instance().log(phone_projection::common::LogLevel::Critical, __FILE__, __LINE__, __func__, __VA_ARGS__)

// ============================================================================
// Utility Functions
// ============================================================================

/** Generate a UUID v4 */
std::string generate_uuid();

/** Get current timestamp as ISO 8601 string */
std::string get_timestamp_iso8601();

/** Get current timestamp as milliseconds since epoch */
int64_t get_timestamp_ms();

/** Sleep for specified duration */
void sleep_for(std::chrono::milliseconds duration);

/** Thread-safe queue for producer/consumer */
template<typename T>
class ConcurrentQueue {
public:
    void push(T&& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(item));
        cv_.notify_one();
    }

    bool try_pop(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return false;
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    void wait_pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !queue_.empty(); });
        item = std::move(queue_.front());
        queue_.pop();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
};

/** Simple thread pool */
class ThreadPool {
public:
    explicit ThreadPool(size_t threads = std::thread::hardware_concurrency());
    ~ThreadPool();

    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
        using ReturnType = decltype(f(args...));
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<ReturnType> result = task->get_future();
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (stop_) throw std::runtime_error("ThreadPool stopped");
            tasks_.emplace([task]() { (*task)(); });
        }
        condition_.notify_one();
        return result;
    }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    bool stop_ = false;
};

/** RAII timer for measuring elapsed time */
class ScopedTimer {
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    using Duration = Clock::duration;

    explicit ScopedTimer(const std::string& name, LogLevel level = LogLevel::Debug)
        : name_(name), level_(level), start_(Clock::now()) {}

    ~ScopedTimer() {
        auto end = Clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start_);
        PP_LOG_DEBUG("{} took {} us", name_, elapsed.count());
    }

    Duration elapsed() const {
        return Clock::now() - start_;
    }

private:
    std::string name_;
    LogLevel level_;
    TimePoint start_;
};

// ============================================================================
// Configuration Helpers
// ============================================================================

/** Simple JSON-based configuration (uses nlohmann/json) */
class JsonConfig {
public:
    JsonConfig() = default;
    explicit JsonConfig(const std::string& json_string);
    explicit JsonConfig(const std::filesystem::path& file_path);

    bool load_from_file(const std::filesystem::path& file_path);
    bool load_from_string(const std::string& json_string);
    std::string to_string(int indent = 2) const;
    void save_to_file(const std::filesystem::path& file_path, int indent = 2) const;

    // Getters with defaults
    template<typename T>
    T get(const std::string& key, const T& default_value = T{}) const {
        auto it = data_.find(key);
        if (it == data_.end()) return default_value;
        try {
            return it->get<T>();
        } catch (...) {
            return default_value;
        }
    }

    template<typename T>
    void set(const std::string& key, const T& value) {
        data_[key] = value;
    }

    bool has(const std::string& key) const { return data_.contains(key); }
    void remove(const std::string& key) { data_.erase(key); }
    void clear() { data_.clear(); }

    // Nested object access
    JsonConfig get_object(const std::string& key) const;
    void set_object(const std::string& key, const JsonConfig& value);

    // Array access
    std::vector<JsonConfig> get_array(const std::string& key) const;
    void set_array(const std::string& key, const std::vector<JsonConfig>& value);

private:
    nlohmann::json data_;
};

} // namespace common
} // namespace phone_projection