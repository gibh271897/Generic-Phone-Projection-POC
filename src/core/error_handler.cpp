/**
 * @file error_handler.cpp
 * @brief Error handler implementation
 */

#include "core/interfaces.hpp"
#include "common/common.hpp"
#include <mutex>
#include <optional>

namespace phone_projection {
namespace core {

class ErrorHandlerImpl : public IErrorHandler {
public:
    ErrorHandlerImpl() = default;
    ~ErrorHandlerImpl() override = default;

    void register_error_callback(ErrorCallback callback) override {
        std::lock_guard<std::mutex> lock(mutex_);
        error_callback_ = std::move(callback);
    }

    void report_error(ErrorCode code, const std::string& context, const std::string& message) override {
        std::lock_guard<std::mutex> lock(mutex_);

        last_error_ = std::make_pair(code, context + ": " + message);

        PP_LOG_ERROR("Error [{}]: {} - {}", static_cast<int>(code), context, message);

        if (error_callback_) {
            error_callback_(code, context, message);
        }
    }

    std::optional<std::pair<ErrorCode, std::string>> get_last_error() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_error_;
    }

    void clear_error() override {
        std::lock_guard<std::mutex> lock(mutex_);
        last_error_.reset();
    }

private:
    mutable std::mutex mutex_;
    ErrorCallback error_callback_;
    std::optional<std::pair<ErrorCode, std::string>> last_error_;
};

std::unique_ptr<IErrorHandler> create_error_handler() {
    return std::make_unique<ErrorHandlerImpl>();
}

} // namespace core
} // namespace phone_projection