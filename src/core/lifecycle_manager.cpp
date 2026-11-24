/**
 * @file lifecycle_manager.cpp
 * @brief Lifecycle manager implementation
 */

#include "core/interfaces.hpp"
#include "common/common.hpp"
#include "platform/platform_adapter.hpp"
#include "android_auto/android_auto_adapter.hpp"
#include <unordered_map>
#include <mutex>
#include <memory>

namespace phone_projection {
namespace core {

class LifecycleManagerImpl : public ILifecycleManager {
public:
    LifecycleManagerImpl() = default;
    ~LifecycleManagerImpl() override {
        if (initialized_) {
            shutdown();
        }
    }

    Result<void> initialize() override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (initialized_) {
            return Result<void>();
        }

        // Explicitly register Android Auto adapter (handles static library linkage)
        android_auto::register_android_auto_adapter();

        // Initialize default adapter if available
        auto adapters = platform::AdapterFactory::instance().get_registered_adapters();
        if (!adapters.empty()) {
            auto adapter = platform::AdapterFactory::instance().create(adapters[0]);
            if (adapter) {
                std::string config = "{}";
                auto result = adapter->initialize(config);
                if (result.is_ok()) {
                    loaded_adapters_[adapters[0]] = std::move(adapter);
                    active_adapter_ = adapters[0];
                    PP_LOG_INFO("Auto-loaded adapter: {}", adapters[0]);
                } else {
                    PP_LOG_WARN("Failed to auto-load adapter {}: {}", adapters[0], result.error().message());
                }
            }
        }

        initialized_ = true;
        PP_LOG_INFO("Middleware initialized");

        return Result<void>();
    }

    Result<void> shutdown() override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!initialized_) {
            return Result<void>();
        }

        for (auto& [name, adapter] : loaded_adapters_) {
            adapter->shutdown();
            PP_LOG_INFO("Shutdown adapter: {}", name);
        }
        loaded_adapters_.clear();
        active_adapter_.reset();

        initialized_ = false;
        PP_LOG_INFO("Middleware shutdown");

        return Result<void>();
    }

    bool is_initialized() const override {
        return initialized_;
    }

    Result<void> load_adapter(const std::string& adapter_name) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!initialized_) {
            return Result<void>(make_error_code(ErrorCode::InternalError));
        }

        if (loaded_adapters_.find(adapter_name) != loaded_adapters_.end()) {
            return Result<void>();
        }

        if (!platform::AdapterFactory::instance().is_registered(adapter_name)) {
            return Result<void>(make_error_code(ErrorCode::AdapterNotFound));
        }

        auto adapter = platform::AdapterFactory::instance().create(adapter_name);
        if (!adapter) {
            return Result<void>(make_error_code(ErrorCode::AdapterLoadFailed));
        }

        std::string config = "{}";
        auto result = adapter->initialize(config);
        if (result.is_error()) {
            return result;
        }

        loaded_adapters_[adapter_name] = std::move(adapter);

        if (!active_adapter_) {
            active_adapter_ = adapter_name;
        }

        PP_LOG_INFO("Loaded adapter: {}", adapter_name);

        return Result<void>();
    }

    Result<void> unload_adapter(const std::string& adapter_name) override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = loaded_adapters_.find(adapter_name);
        if (it == loaded_adapters_.end()) {
            return Result<void>(make_error_code(ErrorCode::AdapterNotFound));
        }

        it->second->shutdown();
        loaded_adapters_.erase(it);

        if (active_adapter_ == adapter_name) {
            active_adapter_.reset();
            if (!loaded_adapters_.empty()) {
                active_adapter_ = loaded_adapters_.begin()->first;
            }
        }

        PP_LOG_INFO("Unloaded adapter: {}", adapter_name);

        return Result<void>();
    }

    std::vector<std::string> get_loaded_adapters() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> result;
        result.reserve(loaded_adapters_.size());
        for (const auto& [name, _] : loaded_adapters_) {
            result.push_back(name);
        }
        return result;
    }

    std::optional<std::string> get_active_adapter() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return active_adapter_;
    }

    Result<void> set_active_adapter(const std::string& adapter_name) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (loaded_adapters_.find(adapter_name) == loaded_adapters_.end()) {
            return Result<void>(make_error_code(ErrorCode::AdapterNotFound));
        }

        active_adapter_ = adapter_name;
        PP_LOG_INFO("Active adapter set to: {}", adapter_name);

        return Result<void>();
    }

    core::IPlatformAdapter* get_active_adapter_instance() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (active_adapter_) {
            auto it = loaded_adapters_.find(*active_adapter_);
            if (it != loaded_adapters_.end()) {
                return it->second.get();
            }
        }
        return nullptr;
    }

    core::IConnectionManager* get_connection_manager() override {
        auto* adapter = get_active_adapter_instance();
        return adapter ? adapter->get_connection_manager() : nullptr;
    }

    core::IResourceManager* get_resource_manager() override {
        auto* adapter = get_active_adapter_instance();
        return adapter ? adapter->get_resource_manager() : nullptr;
    }

    core::IInputHandler* get_input_handler() override {
        auto* adapter = get_active_adapter_instance();
        return adapter ? adapter->get_input_handler() : nullptr;
    }

    core::IUiRenderer* get_ui_renderer() override {
        auto* adapter = get_active_adapter_instance();
        return adapter ? adapter->get_ui_renderer() : nullptr;
    }

    core::ICapabilityProvider* get_capability_provider() override {
        auto* adapter = get_active_adapter_instance();
        return adapter ? adapter->get_capability_provider() : nullptr;
    }

private:
    mutable std::mutex mutex_;
    bool initialized_ = false;
    std::unordered_map<std::string, std::unique_ptr<core::IPlatformAdapter>> loaded_adapters_;
    std::optional<std::string> active_adapter_;
};

std::unique_ptr<ILifecycleManager> create_lifecycle_manager() {
    return std::make_unique<LifecycleManagerImpl>();
}

} // namespace core
} // namespace phone_projection