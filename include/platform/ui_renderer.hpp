/**
 * @file ui_renderer.hpp
 * @brief Base UI renderer interface implementation
 */

#pragma once

#include "core/interfaces.hpp"
#include <memory>
#include <functional>
#include <string>

namespace phone_projection {
namespace platform {

using core::Result;

/**
 * @brief Abstract base class for UI renderers
 *
 * Handles template rendering for projection UIs.
 * Platform-specific implementations handle actual rendering.
 */
class UiRendererBase : public core::IUiRenderer {
public:
    UiRendererBase() = default;
    virtual ~UiRendererBase() = default;

    UiRendererBase(const UiRendererBase&) = delete;
    UiRendererBase& operator=(const UiRendererBase&) = delete;

    // IUiRenderer interface
    Result<void> render_template(TemplateType type, const std::string& json_payload) override = 0;

    Result<void> invalidate() override = 0;

    Result<void> set_surface_size(uint32_t width, uint32_t height) override {
        surface_width_ = width;
        surface_height_ = height;
        return Result<void>();
    }

    std::pair<uint32_t, uint32_t> get_surface_size() const override {
        return {surface_width_, surface_height_};
    }

    void register_callback(TemplateCallback callback) override {
        callback_ = std::move(callback);
    }

    void notify_template_change(TemplateType type, const std::string& payload) {
        if (callback_) {
            callback_(type, payload);
        }
    }

protected:
    uint32_t surface_width_ = 800;
    uint32_t surface_height_ = 480;
    TemplateCallback callback_;
};

} // namespace platform
} // namespace phone_projection