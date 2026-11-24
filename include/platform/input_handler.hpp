/**
 * @file input_handler.hpp
 * @brief Base input handler interface implementation
 */

#pragma once

#include "core/interfaces.hpp"
#include <memory>
#include <functional>
#include <vector>

namespace phone_projection {
namespace platform {

using core::Result;

/**
 * @brief Abstract base class for input handlers
 *
 * Handles routing of input events between middleware and platform.
 * Provides default callback management.
 */
class InputHandlerBase : public core::IInputHandler {
public:
    InputHandlerBase() = default;
    virtual ~InputHandlerBase() = default;

    InputHandlerBase(const InputHandlerBase&) = delete;
    InputHandlerBase& operator=(const InputHandlerBase&) = delete;

    // IInputHandler interface
    Result<void> send_input(const core::Event& event) override = 0;

    void register_callback(InputCallback callback) override {
        callback_ = std::move(callback);
    }

    Result<void> set_focus(bool focused) override = 0;

    std::vector<InputType> get_supported_input_types() const override {
        return supported_types_;
    }

    void add_supported_type(InputType type) {
        if (std::find(supported_types_.begin(), supported_types_.end(), type) == supported_types_.end()) {
            supported_types_.push_back(type);
        }
    }

    void notify_input_received(const core::Event& event) {
        if (callback_) {
            callback_(event);
        }
    }

protected:
    std::vector<InputType> supported_types_;
    InputCallback callback_;
};

} // namespace platform
} // namespace phone_projection