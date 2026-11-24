/**
 * @file test_platform.cpp
 * @brief Platform abstraction layer unit tests
 */

#include <gtest/gtest.h>
#include "platform/platform_adapter.hpp"
#include "platform/connection_manager.hpp"
#include "platform/resource_manager.hpp"
#include "platform/input_handler.hpp"
#include "platform/ui_renderer.hpp"
#include "platform/capability_provider.hpp"
#include "core/interfaces.hpp"

using namespace phone_projection;
using namespace phone_projection::platform;
using namespace phone_projection::core;

class MockConnectionManager : public ConnectionManagerBase {
public:
    Result<void> connect([[maybe_unused]] const VehicleProfile& profile) override {
        set_state(IConnectionManager::State::Connecting);
        set_state(IConnectionManager::State::Connected);
        return Result<void>();
    }

    Result<void> disconnect() override {
        set_state(IConnectionManager::State::Disconnecting);
        set_state(IConnectionManager::State::Disconnected);
        return Result<void>();
    }

    Result<void> send_data(const std::vector<uint8_t>& data) override {
        last_sent_data_ = data;
        return Result<void>();
    }

    const std::vector<uint8_t>& last_sent_data() const { return last_sent_data_; }

private:
    std::vector<uint8_t> last_sent_data_;
};

class MockResourceManager : public ResourceManagerBase {
public:
    // Uses base implementation
};

class MockInputHandler : public InputHandlerBase {
public:
    Result<void> send_input(const Event& event) override {
        last_event_ = event;
        return Result<void>();
    }

    Result<void> set_focus(bool focused) override {
        focus_state_ = focused;
        return Result<void>();
    }

    const Event& last_event() const { return last_event_; }
    bool focus_state() const { return focus_state_; }

private:
    Event last_event_{EventType::SessionCreated};
    bool focus_state_ = false;
};

class MockUiRenderer : public UiRendererBase {
public:
    Result<void> render_template(IUiRenderer::TemplateType type, const std::string& json_payload) override {
        last_template_type_ = type;
        last_payload_ = json_payload;
        return Result<void>();
    }

    Result<void> invalidate() override {
        invalidate_count_++;
        return Result<void>();
    }

    IUiRenderer::TemplateType last_template_type() const { return last_template_type_; }
    const std::string& last_payload() const { return last_payload_; }
    int invalidate_count() const { return invalidate_count_; }

private:
    IUiRenderer::TemplateType last_template_type_ = IUiRenderer::TemplateType::Custom;
    std::string last_payload_;
    int invalidate_count_ = 0;
};

class MockCapabilityProvider : public CapabilityProviderBase {
public:
    MockCapabilityProvider() {
        capabilities_.supports_touch = true;
        capabilities_.supports_rotary = true;
        capabilities_.supports_video = true;
        capabilities_.max_video_width = 1920;
        capabilities_.max_video_height = 1080;
    }
};

TEST(ConnectionManagerTest, StateTransitions) {
    MockConnectionManager mgr;

    EXPECT_EQ(mgr.get_state(), IConnectionManager::State::Disconnected);

    VehicleProfile profile;
    auto result = mgr.connect(profile);
    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(mgr.get_state(), IConnectionManager::State::Connected);

    result = mgr.disconnect();
    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(mgr.get_state(), IConnectionManager::State::Disconnected);
}

TEST(ConnectionManagerTest, Callbacks) {
    MockConnectionManager mgr;

    std::vector<std::pair<IConnectionManager::State, IConnectionManager::State>> state_changes;

    mgr.register_callback([&state_changes](IConnectionManager::State old_state, IConnectionManager::State new_state, std::optional<ErrorCode>) {
        state_changes.emplace_back(old_state, new_state);
    });

    VehicleProfile profile;
    mgr.connect(profile);

    // Should have two state changes: Disconnected->Connecting, Connecting->Connected
    ASSERT_EQ(state_changes.size(), 2);
    EXPECT_EQ(state_changes[0].first, IConnectionManager::State::Disconnected);
    EXPECT_EQ(state_changes[0].second, IConnectionManager::State::Connecting);
    EXPECT_EQ(state_changes[1].first, IConnectionManager::State::Connecting);
    EXPECT_EQ(state_changes[1].second, IConnectionManager::State::Connected);
}

TEST(ConnectionManagerTest, DataCallbacks) {
    MockConnectionManager mgr;

    std::vector<uint8_t> received_data;
    mgr.register_data_callback([&received_data](const std::vector<uint8_t>& data) {
        received_data = data;
    });

    std::vector<uint8_t> test_data = {0x01, 0x02, 0x03, 0x04};
    mgr.notify_data_received(test_data);

    EXPECT_EQ(received_data, test_data);
}

TEST(ResourceManagerTest, RequestAndRelease) {
    MockResourceManager mgr;

    auto result = mgr.request_resource(IResourceManager::ResourceType::AudioFocus, "session1");
    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(result.value().granted);
    EXPECT_FALSE(result.value().token.empty());

    // Request again - should fail (busy)
    auto result2 = mgr.request_resource(IResourceManager::ResourceType::AudioFocus, "session2");
    EXPECT_TRUE(result2.is_error());
    EXPECT_EQ(result2.error(), make_error_code(ErrorCode::ResourceBusy));

    // Release
    auto release_result = mgr.release_resource(result.value().token);
    EXPECT_TRUE(release_result.is_ok());

    // Request again - should succeed now
    auto result3 = mgr.request_resource(IResourceManager::ResourceType::AudioFocus, "session3");
    EXPECT_TRUE(result3.is_ok());
}

TEST(ResourceManagerTest, ResourceCallbacks) {
    MockResourceManager mgr;

    IResourceManager::ResourceType last_type = IResourceManager::ResourceType::AudioFocus;
    bool last_state = false;

    mgr.register_callback([&last_type, &last_state](IResourceManager::ResourceType type, bool available) {
        last_type = type;
        last_state = available;
    });

    auto result = mgr.request_resource(IResourceManager::ResourceType::VideoSurface, "session1");
    ASSERT_TRUE(result.is_ok());

    EXPECT_EQ(last_type, IResourceManager::ResourceType::VideoSurface);
    EXPECT_TRUE(last_state);

    mgr.release_resource(result.value().token);
    EXPECT_FALSE(last_state);
}

TEST(InputHandlerTest, SendInput) {
    MockInputHandler handler;

    TouchEvent event(TouchEvent::Action::Down, 0.5f, 0.5f, "session1");
    auto result = handler.send_input(event);
    EXPECT_TRUE(result.is_ok());

    const Event& last = handler.last_event();
    EXPECT_EQ(last.type, EventType::TouchDown);
}

TEST(InputHandlerTest, Focus) {
    MockInputHandler handler;

    auto result = handler.set_focus(true);
    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(handler.focus_state());

    result = handler.set_focus(false);
    EXPECT_TRUE(result.is_ok());
    EXPECT_FALSE(handler.focus_state());
}

TEST(InputHandlerTest, SupportedTypes) {
    MockInputHandler handler;
    handler.add_supported_type(IInputHandler::InputType::Touch);
    handler.add_supported_type(IInputHandler::InputType::Rotary);

    auto types = handler.get_supported_input_types();
    EXPECT_EQ(types.size(), 2);
    EXPECT_TRUE(std::find(types.begin(), types.end(), IInputHandler::InputType::Touch) != types.end());
    EXPECT_TRUE(std::find(types.begin(), types.end(), IInputHandler::InputType::Rotary) != types.end());
}

TEST(UiRendererTest, RenderTemplate) {
    MockUiRenderer renderer;

    std::string payload = R"({"title": "Test"})";
    auto result = renderer.render_template(IUiRenderer::TemplateType::MediaTemplate, payload);
    EXPECT_TRUE(result.is_ok());

    EXPECT_EQ(renderer.last_template_type(), IUiRenderer::TemplateType::MediaTemplate);
    EXPECT_EQ(renderer.last_payload(), payload);
}

TEST(UiRendererTest, SurfaceSize) {
    MockUiRenderer renderer;

    auto result = renderer.set_surface_size(1280, 720);
    EXPECT_TRUE(result.is_ok());

    auto size = renderer.get_surface_size();
    EXPECT_EQ(size.first, 1280);
    EXPECT_EQ(size.second, 720);
}

TEST(UiRendererTest, Invalidate) {
    MockUiRenderer renderer;

    EXPECT_EQ(renderer.invalidate_count(), 0);

    renderer.invalidate();
    EXPECT_EQ(renderer.invalidate_count(), 1);

    renderer.invalidate();
    EXPECT_EQ(renderer.invalidate_count(), 2);
}

TEST(CapabilityProviderTest, GetCapabilities) {
    MockCapabilityProvider provider;

    const auto& caps = provider.get_capabilities();
    EXPECT_TRUE(caps.supports_touch);
    EXPECT_TRUE(caps.supports_rotary);
    EXPECT_TRUE(caps.supports_video);
    EXPECT_EQ(caps.max_video_width, 1920);
    EXPECT_EQ(caps.max_video_height, 1080);
}

TEST(CapabilityProviderTest, NegotiateCapabilities) {
    MockCapabilityProvider provider;

    Capabilities requested;
    requested.supports_touch = true;
    requested.supports_rotary = false;
    requested.supports_video = true;
    requested.max_video_width = 3840;  // Higher than supported
    requested.max_video_height = 2160;

    auto result = provider.negotiate_capabilities(requested);
    ASSERT_TRUE(result.is_ok());

    const auto& negotiated = result.value();
    EXPECT_TRUE(negotiated.supports_touch);
    EXPECT_FALSE(negotiated.supports_rotary);  // Not requested
    EXPECT_TRUE(negotiated.supports_video);
    EXPECT_EQ(negotiated.max_video_width, 1920);  // Capped to max
    EXPECT_EQ(negotiated.max_video_height, 1080);
}

TEST(CapabilityProviderTest, Supports) {
    MockCapabilityProvider provider;

    EXPECT_TRUE(provider.supports("touch"));
    EXPECT_TRUE(provider.supports("rotary"));
    EXPECT_TRUE(provider.supports("video"));
    EXPECT_FALSE(provider.supports("audio"));  // Not set
    EXPECT_FALSE(provider.supports("unknown"));
}