/**
 * @file platform_adapter.hpp
 * @brief Platform adapter interfaces and base classes
 */

#pragma once

#include "core/interfaces.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace phone_projection {
namespace platform {

using core::Result;

/**
 * @brief Base class for platform adapters
 *
 * All platform-specific implementations must inherit from this class
 * and provide concrete implementations for all pure virtual methods.
 */
class PlatformAdapterBase : public core::IPlatformAdapter {
public:
    PlatformAdapterBase() = default;
    virtual ~PlatformAdapterBase() = default;

    // Non-copyable, non-movable
    PlatformAdapterBase(const PlatformAdapterBase&) = delete;
    PlatformAdapterBase& operator=(const PlatformAdapterBase&) = delete;
    PlatformAdapterBase(PlatformAdapterBase&&) = delete;
    PlatformAdapterBase& operator=(PlatformAdapterBase&&) = delete;

    // IPlatformAdapter interface
    const Metadata& get_metadata() const override { return metadata_; }

    virtual Result<void> initialize(const std::string& config) override = 0;
    virtual Result<void> shutdown() override = 0;

    core::IConnectionManager* get_connection_manager() override { return connection_manager_.get(); }
    core::IResourceManager* get_resource_manager() override { return resource_manager_.get(); }
    core::IInputHandler* get_input_handler() override { return input_handler_.get(); }
    core::IUiRenderer* get_ui_renderer() override { return ui_renderer_.get(); }
    core::ICapabilityProvider* get_capability_provider() override { return capability_provider_.get(); }

protected:
    Metadata metadata_;
    std::unique_ptr<core::IConnectionManager> connection_manager_;
    std::unique_ptr<core::IResourceManager> resource_manager_;
    std::unique_ptr<core::IInputHandler> input_handler_;
    std::unique_ptr<core::IUiRenderer> ui_renderer_;
    std::unique_ptr<core::ICapabilityProvider> capability_provider_;
};

/**
 * @brief Adapter factory for creating platform adapters
 */
class AdapterFactory {
public:
    using CreatorFunc = std::function<std::unique_ptr<core::IPlatformAdapter>()>;

    static AdapterFactory& instance() {
        static AdapterFactory factory;
        return factory;
    }

    void register_adapter(const std::string& name, CreatorFunc creator) {
        creators_[name] = std::move(creator);
    }

    std::unique_ptr<core::IPlatformAdapter> create(const std::string& name) {
        auto it = creators_.find(name);
        if (it != creators_.end()) {
            return it->second();
        }
        return nullptr;
    }

    std::vector<std::string> get_registered_adapters() const {
        std::vector<std::string> names;
        names.reserve(creators_.size());
        for (const auto& [name, _] : creators_) {
            names.push_back(name);
        }
        return names;
    }

    bool is_registered(const std::string& name) const {
        return creators_.find(name) != creators_.end();
    }

private:
    AdapterFactory() = default;
    std::unordered_map<std::string, CreatorFunc> creators_;
};

/**
 * @brief Registration helper for automatic adapter registration
 */
template<typename AdapterType>
class AdapterRegistrar {
public:
    AdapterRegistrar(const std::string& name) {
        AdapterFactory::instance().register_adapter(name, []() {
            return std::make_unique<AdapterType>();
        });
    }
};

#define REGISTER_ADAPTER(AdapterType, name) \
    static phone_projection::platform::AdapterRegistrar<AdapterType> \
        adapter_registrar_##name(#name)

} // namespace platform
} // namespace phone_projection