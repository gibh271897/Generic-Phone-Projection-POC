/**
 * @file config_manager.cpp
 * @brief Configuration manager implementation
 */

#include "core/interfaces.hpp"
#include "common/common.hpp"
#include <unordered_map>
#include <mutex>
#include <fstream>
#include <filesystem>

namespace phone_projection {
namespace core {

class ConfigManagerImpl : public IConfigurationManager {
public:
    ConfigManagerImpl() = default;
    ~ConfigManagerImpl() override = default;

    Result<void> load_config(const std::string& config_path) override {
        std::lock_guard<std::mutex> lock(mutex_);

        std::filesystem::path path(config_path);
        if (!std::filesystem::exists(path)) {
            return Result<void>(make_error_code(ErrorCode::ConfigNotFound));
        }

        try {
            std::ifstream file(path);
            nlohmann::json json;
            file >> json;

            parse_config(json);
            config_file_path_ = config_path;

            PP_LOG_INFO("Loaded configuration from {}", config_path);
            return Result<void>();
        } catch (const std::exception& e) {
            PP_LOG_ERROR("Failed to parse config: {}", e.what());
            return Result<void>(make_error_code(ErrorCode::ConfigParseError));
        }
    }

    Result<VehicleProfile> get_vehicle_profile(const std::string& profile_id) const override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = profiles_.find(profile_id);
        if (it == profiles_.end()) {
            return Result<VehicleProfile>(make_error_code(ErrorCode::ConfigNotFound));
        }

        return Result<VehicleProfile>(it->second);
    }

    Result<void> set_active_profile(const std::string& profile_id) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (profiles_.find(profile_id) == profiles_.end()) {
            return Result<void>(make_error_code(ErrorCode::ConfigNotFound));
        }

        active_profile_ = profile_id;

        if (config_callback_) {
            config_callback_("active_profile", profile_id);
        }

        PP_LOG_INFO("Active profile set to: {}", profile_id);
        return Result<void>();
    }

    Result<VehicleProfile> get_active_profile() const override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!active_profile_) {
            return Result<VehicleProfile>(make_error_code(ErrorCode::ConfigNotFound));
        }

        auto it = profiles_.find(*active_profile_);
        if (it == profiles_.end()) {
            return Result<VehicleProfile>(make_error_code(ErrorCode::ConfigNotFound));
        }

        return Result<VehicleProfile>(it->second);
    }

    Result<std::string> get_platform_config(const std::string& platform, const std::string& key) const override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto platform_it = platform_configs_.find(platform);
        if (platform_it == platform_configs_.end()) {
            return Result<std::string>(make_error_code(ErrorCode::ConfigNotFound));
        }

        auto key_it = platform_it->second.find(key);
        if (key_it == platform_it->second.end()) {
            return Result<std::string>(make_error_code(ErrorCode::ConfigNotFound));
        }

        return Result<std::string>(key_it->second);
    }

    Result<void> set_platform_config(const std::string& platform, const std::string& key, const std::string& value) override {
        std::lock_guard<std::mutex> lock(mutex_);

        platform_configs_[platform][key] = value;

        if (config_callback_) {
            config_callback_(platform + "." + key, value);
        }

        return Result<void>();
    }

    void register_config_callback(ConfigChangeCallback callback) override {
        config_callback_ = std::move(callback);
    }

    Result<void> add_profile(const std::string& profile_id, const VehicleProfile& profile) override {
        std::lock_guard<std::mutex> lock(mutex_);
        profiles_[profile_id] = profile;
        if (!active_profile_) {
            active_profile_ = profile_id;
        }
        PP_LOG_INFO("Added vehicle profile: {}", profile_id);
        return Result<void>();
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, VehicleProfile> profiles_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> platform_configs_;
    std::optional<std::string> active_profile_;
    std::string config_file_path_;
    ConfigChangeCallback config_callback_;

    void parse_config(const nlohmann::json& json) {
        // Parse vehicle profiles
        if (json.contains("vehicle_profiles") && json["vehicle_profiles"].is_object()) {
            for (const auto& [id, profile_json] : json["vehicle_profiles"].items()) {
                VehicleProfile profile;
                profile.vehicle_id = profile_json.value("vehicle_id", id);
                profile.make = profile_json.value("make", "");
                profile.model = profile_json.value("model", "");
                profile.year = profile_json.value("year", "");
                profile.head_unit_model = profile_json.value("head_unit_model", "");
                profile.head_unit_firmware = profile_json.value("head_unit_firmware", "");
                profile.preferred_transport = profile_json.value("preferred_transport", "usb");
                profile.screen_width = profile_json.value("screen_width", 800);
                profile.screen_height = profile_json.value("screen_height", 480);
                profile.pixel_density = profile_json.value("pixel_density", 1.0f);
                profile.is_right_hand_drive = profile_json.value("is_right_hand_drive", false);

                // Parse capabilities
                if (profile_json.contains("capabilities")) {
                    const auto& caps = profile_json["capabilities"];
                    profile.capabilities.supports_touch = caps.value("supports_touch", false);
                    profile.capabilities.supports_rotary = caps.value("supports_rotary", false);
                    profile.capabilities.supports_keyboard = caps.value("supports_keyboard", false);
                    profile.capabilities.supports_voice = caps.value("supports_voice", false);
                    profile.capabilities.supports_video = caps.value("supports_video", false);
                    profile.capabilities.supports_audio = caps.value("supports_audio", false);
                    profile.capabilities.supports_navigation = caps.value("supports_navigation", false);
                    profile.capabilities.max_video_width = caps.value("max_video_width", 0);
                    profile.capabilities.max_video_height = caps.value("max_video_height", 0);
                    profile.capabilities.supported_codecs = caps.value("supported_codecs", std::vector<std::string>{});
                    profile.capabilities.supported_templates = caps.value("supported_templates", std::vector<std::string>{});
                }

                profiles_[id] = std::move(profile);
            }
        }

        // Parse platform configs
        if (json.contains("platform_configs") && json["platform_configs"].is_object()) {
            for (const auto& [platform, config] : json["platform_configs"].items()) {
                if (config.is_object()) {
                    for (const auto& [key, value] : config.items()) {
                        platform_configs_[platform][key] = value.is_string() ? value.get<std::string>() : value.dump();
                    }
                }
            }
        }

        // Parse active profile
        if (json.contains("active_profile") && json["active_profile"].is_string()) {
            active_profile_ = json["active_profile"].get<std::string>();
        }
    }
};

std::unique_ptr<IConfigurationManager> create_config_manager() {
    return std::make_unique<ConfigManagerImpl>();
}

} // namespace core
} // namespace phone_projection