/**
 * @file resource_manager.hpp
 * @brief Base resource manager interface implementation
 */

#pragma once

#include "core/interfaces.hpp"
#include <memory>
#include <functional>
#include <unordered_map>
#include <mutex>

namespace phone_projection {
namespace platform {

using core::Result;

/**
 * @brief Abstract base class for resource managers
 *
 * Manages shared resources like audio focus, video surfaces, etc.
 * Provides thread-safe resource tracking and callbacks.
 */
class ResourceManagerBase : public core::IResourceManager {
public:
    ResourceManagerBase() = default;
    virtual ~ResourceManagerBase() = default;

    ResourceManagerBase(const ResourceManagerBase&) = delete;
    ResourceManagerBase& operator=(const ResourceManagerBase&) = delete;

    // IResourceManager interface
    Result<ResourceGrant> request_resource(ResourceType type, const std::string& session_id) override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = resource_state_.find(type);
        bool available = (it == resource_state_.end() || !it->second.held);

        if (!available) {
            return Result<ResourceGrant>(core::make_error_code(core::ErrorCode::ResourceBusy));
        }

        std::string token = generate_token();
        resource_state_[type] = {true, session_id, token};

        ResourceGrant grant;
        grant.type = type;
        grant.granted = true;
        grant.token = token;
        grant.timeout = std::chrono::milliseconds(0);

        if (callback_) {
            callback_(type, true);
        }

        return Result<ResourceGrant>(std::move(grant));
    }

    Result<void> release_resource(const std::string& token) override {
        std::lock_guard<std::mutex> lock(mutex_);

        for (auto& [type, state] : resource_state_) {
            if (state.held && state.token == token) {
                state.held = false;
                state.session_id.clear();
                state.token.clear();

                if (callback_) {
                    callback_(type, false);
                }
                return Result<void>();
            }
        }

        return Result<void>(core::make_error_code(core::ErrorCode::InvalidArgument));
    }

    bool is_resource_available(ResourceType type) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = resource_state_.find(type);
        return it == resource_state_.end() || !it->second.held;
    }

    void register_callback(ResourceCallback callback) override {
        callback_ = std::move(callback);
    }

protected:
    struct ResourceState {
        bool held = false;
        std::string session_id;
        std::string token;
    };

    mutable std::mutex mutex_;
    std::unordered_map<ResourceType, ResourceState> resource_state_;
    ResourceCallback callback_;

    static std::string generate_token() {
        static std::atomic<uint64_t> counter{0};
        return "res_" + std::to_string(counter.fetch_add(1));
    }
};

} // namespace platform
} // namespace phone_projection