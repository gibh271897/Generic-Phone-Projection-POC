/**
 * @file connection_manager.hpp
 * @brief Base connection manager interface implementation
 */

#pragma once

#include "core/interfaces.hpp"
#include <memory>
#include <functional>

namespace phone_projection {
namespace platform {

using core::Result;

/**
 * @brief Abstract base class for connection managers
 *
 * Provides default implementations for common functionality while
 * requiring platform-specific implementations for transport details.
 */
class ConnectionManagerBase : public core::IConnectionManager {
public:
    ConnectionManagerBase() = default;
    virtual ~ConnectionManagerBase() = default;

    // Non-copyable, non-movable
    ConnectionManagerBase(const ConnectionManagerBase&) = delete;
    ConnectionManagerBase& operator=(const ConnectionManagerBase&) = delete;

    // IConnectionManager interface
    State get_state() const override { return state_.load(); }

    void register_callback(ConnectionCallback callback) override {
        callback_ = std::move(callback);
    }

    void register_connection_complete_callback(ConnectionCompleteCallback callback) override {
        connection_complete_callback_ = std::move(callback);
    }

    void register_data_callback(DataCallback callback) override {
        data_callback_ = std::move(callback);
    }

    // Public method for testing - simulates receiving data
    void notify_data_received(const std::vector<uint8_t>& data) {
        if (data_callback_) {
            data_callback_(data);
        }
    }

    virtual Result<void> send_data(const std::vector<uint8_t>& data) override = 0;

protected:
    std::atomic<State> state_{State::Disconnected};
    ConnectionCallback callback_;
    ConnectionCompleteCallback connection_complete_callback_;
    DataCallback data_callback_;

    void set_state(State new_state, std::optional<core::ErrorCode> error = std::nullopt) {
        State old_state = state_.exchange(new_state);
        if (old_state != new_state && callback_) {
            callback_(old_state, new_state, error);
        }

        // If transitioning to Connected, notify connection complete callback
        if (new_state == State::Connected && connection_complete_callback_) {
            // Derived classes should call notify_connection_complete() with negotiated capabilities
        }
    }

    /** Notify that connection is complete with negotiated capabilities */
    void notify_connection_complete(const core::Capabilities& caps) {
        if (connection_complete_callback_) {
            connection_complete_callback_(caps);
        }
    }
};

} // namespace platform
} // namespace phone_projection