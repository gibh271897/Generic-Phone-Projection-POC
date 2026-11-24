/**
 * @file common.cpp
 * @brief Common utilities implementation
 */

#include "common/common.hpp"
#include <random>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>

namespace phone_projection {
namespace common {

std::string generate_uuid() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis(0, UINT64_MAX);

    uint64_t part1 = dis(gen);
    uint64_t part2 = dis(gen);

    // Version 4 (random) UUID format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    // y must be 8, 9, A, or B
    part2 = (part2 & 0x3FFFFFFFFFFFFFFF) | 0x8000000000000000;  // variant
    part1 = (part1 & 0xFFFFFFFFFFFF0FFF) | 0x0000000000004000;    // version 4

    std::stringstream ss;
    ss << std::hex << std::setfill('0')
       << std::setw(8) << (part1 >> 32)
       << "-" << std::setw(4) << ((part1 >> 16) & 0xFFFF)
       << "-" << std::setw(4) << (part1 & 0xFFFF)
       << "-" << std::setw(4) << (part2 >> 48)
       << "-" << std::setw(12) << (part2 & 0xFFFFFFFFFFFF);

    return ss.str();
}

std::string get_timestamp_iso8601() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::stringstream ss;
#if defined(_WIN32)
    std::tm tm_buf;
    gmtime_s(&tm_buf, &in_time_t);
    ss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");
#else
    ss << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%S");
#endif
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return ss.str();
}

int64_t get_timestamp_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void sleep_for(std::chrono::milliseconds duration) {
    std::this_thread::sleep_for(duration);
}

ThreadPool::ThreadPool(size_t threads) {
    for (size_t i = 0; i < threads; ++i) {
        workers_.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex_);
                    condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                    if (stop_ && tasks_.empty()) return;
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }
    condition_.notify_all();
    for (std::thread& worker : workers_) {
        worker.join();
    }
}

JsonConfig::JsonConfig(const std::string& json_string) {
    load_from_string(json_string);
}

JsonConfig::JsonConfig(const std::filesystem::path& file_path) {
    load_from_file(file_path);
}

bool JsonConfig::load_from_file(const std::filesystem::path& file_path) {
    try {
        std::ifstream file(file_path);
        if (!file.is_open()) return false;
        file >> data_;
        return true;
    } catch (...) {
        return false;
    }
}

bool JsonConfig::load_from_string(const std::string& json_string) {
    try {
        data_ = nlohmann::json::parse(json_string);
        return true;
    } catch (...) {
        return false;
    }
}

std::string JsonConfig::to_string(int indent) const {
    return data_.dump(indent);
}

void JsonConfig::save_to_file(const std::filesystem::path& file_path, int indent) const {
    std::ofstream file(file_path);
    file << data_.dump(indent);
}

JsonConfig JsonConfig::get_object(const std::string& key) const {
    JsonConfig result;
    auto it = data_.find(key);
    if (it != data_.end() && it->is_object()) {
        result.data_ = *it;
    }
    return result;
}

void JsonConfig::set_object(const std::string& key, const JsonConfig& value) {
    data_[key] = value.data_;
}

std::vector<JsonConfig> JsonConfig::get_array(const std::string& key) const {
    std::vector<JsonConfig> result;
    auto it = data_.find(key);
    if (it != data_.end() && it->is_array()) {
        for (const auto& item : *it) {
            JsonConfig config;
            config.data_ = item;
            result.push_back(std::move(config));
        }
    }
    return result;
}

void JsonConfig::set_array(const std::string& key, const std::vector<JsonConfig>& value) {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& item : value) {
        arr.push_back(item.data_);
    }
    data_[key] = arr;
}

} // namespace common
} // namespace phone_projection