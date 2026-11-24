/**
 * @file capability_provider.hpp
 * @brief Base capability provider interface implementation
 */

#pragma once

#include "core/interfaces.hpp"
#include <memory>

namespace phone_projection {
namespace platform {

using core::Result;

/**
 * @brief Abstract base class for capability providers
 *
 * Manages platform capabilities and negotiation logic.
 */
class CapabilityProviderBase : public core::ICapabilityProvider {
public:
    CapabilityProviderBase() = default;
    virtual ~CapabilityProviderBase() = default;

    CapabilityProviderBase(const CapabilityProviderBase&) = delete;
    CapabilityProviderBase& operator=(const CapabilityProviderBase&) = delete;

    // ICapabilityProvider interface
    const core::Capabilities& get_capabilities() const override {
        return capabilities_;
    }

    virtual Result<core::Capabilities> negotiate_capabilities(const core::Capabilities& requested) override {
        core::Capabilities negotiated;

        // Negotiate each capability
        negotiated.supports_touch = requested.supports_touch && capabilities_.supports_touch;
        negotiated.supports_rotary = requested.supports_rotary && capabilities_.supports_rotary;
        negotiated.supports_keyboard = requested.supports_keyboard && capabilities_.supports_keyboard;
        negotiated.supports_voice = requested.supports_voice && capabilities_.supports_voice;
        negotiated.supports_video = requested.supports_video && capabilities_.supports_video;
        negotiated.supports_audio = requested.supports_audio && capabilities_.supports_audio;
        negotiated.supports_navigation = requested.supports_navigation && capabilities_.supports_navigation;

        negotiated.max_video_width = std::min(requested.max_video_width, capabilities_.max_video_width);
        negotiated.max_video_height = std::min(requested.max_video_height, capabilities_.max_video_height);

        // Intersect codecs
        for (const auto& codec : requested.supported_codecs) {
            if (std::find(capabilities_.supported_codecs.begin(),
                         capabilities_.supported_codecs.end(), codec) != capabilities_.supported_codecs.end()) {
                negotiated.supported_codecs.push_back(codec);
            }
        }

        // Intersect templates
        for (const auto& tmpl : requested.supported_templates) {
            if (std::find(capabilities_.supported_templates.begin(),
                         capabilities_.supported_templates.end(), tmpl) != capabilities_.supported_templates.end()) {
                negotiated.supported_templates.push_back(tmpl);
            }
        }

        return Result<core::Capabilities>(std::move(negotiated));
    }

    bool supports(const std::string& capability_name) const override {
        if (capability_name == "touch") return capabilities_.supports_touch;
        if (capability_name == "rotary") return capabilities_.supports_rotary;
        if (capability_name == "keyboard") return capabilities_.supports_keyboard;
        if (capability_name == "voice") return capabilities_.supports_voice;
        if (capability_name == "video") return capabilities_.supports_video;
        if (capability_name == "audio") return capabilities_.supports_audio;
        if (capability_name == "navigation") return capabilities_.supports_navigation;
        return false;
    }

    void set_capabilities(const core::Capabilities& caps) {
        capabilities_ = caps;
    }

protected:
    core::Capabilities capabilities_;
};

} // namespace platform
} // namespace phone_projection