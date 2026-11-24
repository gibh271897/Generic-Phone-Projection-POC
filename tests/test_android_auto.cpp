/**
 * @file test_android_auto.cpp
 * @brief Android Auto adapter unit tests
 */

#include <gtest/gtest.h>
#include "android_auto/android_auto_adapter.hpp"
#include "core/interfaces.hpp"
#include "platform/platform_adapter.hpp"

using namespace phone_projection;
using namespace phone_projection::android_auto;
using namespace phone_projection::core;

TEST(AndroidAutoAdapterTest, Metadata) {
    AndroidAutoAdapter adapter;

    const auto& metadata = adapter.get_metadata();
    EXPECT_EQ(metadata.name, "android_auto");
    EXPECT_EQ(metadata.version, "1.0.0");
    EXPECT_FALSE(metadata.description.empty());
    EXPECT_TRUE(std::find(metadata.supported_transports.begin(),
                         metadata.supported_transports.end(), "usb") != metadata.supported_transports.end());
    EXPECT_TRUE(std::find(metadata.supported_transports.begin(),
                         metadata.supported_transports.end(), "bluetooth") != metadata.supported_transports.end());
}

TEST(AndroidAutoAdapterTest, InitializeAndShutdown) {
    AndroidAutoAdapter adapter;

    auto result = adapter.initialize("{}");
    EXPECT_TRUE(result.is_ok());

    result = adapter.shutdown();
    EXPECT_TRUE(result.is_ok());
}

TEST(AndroidAutoAdapterTest, ComponentsExist) {
    AndroidAutoAdapter adapter;
    adapter.initialize("{}");

    EXPECT_NE(adapter.get_connection_manager(), nullptr);
    EXPECT_NE(adapter.get_resource_manager(), nullptr);
    EXPECT_NE(adapter.get_input_handler(), nullptr);
    EXPECT_NE(adapter.get_ui_renderer(), nullptr);
    EXPECT_NE(adapter.get_capability_provider(), nullptr);

    adapter.shutdown();
}

TEST(AndroidAutoConnectionManagerTest, ConnectDisconnect) {
    AndroidAutoConnectionManager mgr;

    VehicleProfile profile;
    profile.vehicle_id = "test_vehicle";

    auto result = mgr.connect(profile);
    EXPECT_TRUE(result.is_ok());

    // Wait for connection simulation
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    EXPECT_EQ(mgr.get_state(), IConnectionManager::State::Connected);

    result = mgr.disconnect();
    EXPECT_TRUE(result.is_ok());

    EXPECT_EQ(mgr.get_state(), IConnectionManager::State::Disconnected);
}

TEST(AndroidAutoConnectionManagerTest, SendData) {
    AndroidAutoConnectionManager mgr;

    VehicleProfile profile;
    mgr.connect(profile);
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    std::vector<uint8_t> data = {0xAA, 0xBB, 0xCC};
    auto result = mgr.send_data(data);
    EXPECT_TRUE(result.is_ok());

    mgr.disconnect();
}

TEST(AndroidAutoResourceManagerTest, RequestResources) {
    AndroidAutoResourceManager mgr;

    // Request all resource types
    auto audio = mgr.request_resource(IResourceManager::ResourceType::AudioFocus, "session1");
    EXPECT_TRUE(audio.is_ok());
    EXPECT_TRUE(audio.value().granted);

    auto video = mgr.request_resource(IResourceManager::ResourceType::VideoSurface, "session1");
    EXPECT_TRUE(video.is_ok());
    EXPECT_TRUE(video.value().granted);

    auto nav = mgr.request_resource(IResourceManager::ResourceType::NavigationFocus, "session1");
    EXPECT_TRUE(nav.is_ok());
    EXPECT_TRUE(nav.value().granted);

    auto input = mgr.request_resource(IResourceManager::ResourceType::InputFocus, "session1");
    EXPECT_TRUE(input.is_ok());
    EXPECT_TRUE(input.value().granted);

    // Release all
    mgr.release_resource(audio.value().token);
    mgr.release_resource(video.value().token);
    mgr.release_resource(nav.value().token);
    mgr.release_resource(input.value().token);
}

TEST(AndroidAutoInputHandlerTest, SupportedTypes) {
    AndroidAutoInputHandler handler;

    auto types = handler.get_supported_input_types();
    EXPECT_EQ(types.size(), 5);  // Touch, Rotary, Keyboard, Voice, HardwareKey

    EXPECT_TRUE(std::find(types.begin(), types.end(), IInputHandler::InputType::Touch) != types.end());
    EXPECT_TRUE(std::find(types.begin(), types.end(), IInputHandler::InputType::Rotary) != types.end());
    EXPECT_TRUE(std::find(types.begin(), types.end(), IInputHandler::InputType::Keyboard) != types.end());
    EXPECT_TRUE(std::find(types.begin(), types.end(), IInputHandler::InputType::Voice) != types.end());
    EXPECT_TRUE(std::find(types.begin(), types.end(), IInputHandler::InputType::HardwareKey) != types.end());
}

TEST(AndroidAutoInputHandlerTest, SimulateInput) {
    AndroidAutoInputHandler handler;

    bool received = false;
    EventType received_type = EventType::SessionCreated;

    handler.register_callback([&received, &received_type](const Event& event) {
        received = true;
        received_type = event.type;
    });

    // Simulate touch
    handler.simulate_touch(0.5f, 0.5f, TouchEvent::Action::Down, "session1");

    EXPECT_TRUE(received);
    EXPECT_EQ(received_type, EventType::TouchDown);
}

TEST(AndroidAutoInputHandlerTest, SimulateRotary) {
    AndroidAutoInputHandler handler;

    bool received = false;
    int steps = 0;

    handler.register_callback([&received, &steps](const Event& event) {
        received = true;
        if (auto* re = dynamic_cast<const RotaryEvent*>(&event)) {
            steps = re->steps;
        }
    });

    handler.simulate_rotary(RotaryEvent::Action::Clockwise, 3, "session1");

    EXPECT_TRUE(received);
    EXPECT_EQ(steps, 3);
}

TEST(AndroidAutoInputHandlerTest, SimulateVoice) {
    AndroidAutoInputHandler handler;

    std::string received_text;
    float received_confidence = 0.0f;

    handler.register_callback([&received_text, &received_confidence](const Event& event) {
        if (auto* ve = dynamic_cast<const VoiceEvent*>(&event)) {
            received_text = ve->transcript;
            received_confidence = ve->confidence;
        }
    });

    handler.simulate_voice("Navigate home", 0.95f, true, "session1");

    EXPECT_EQ(received_text, "Navigate home");
    EXPECT_FLOAT_EQ(received_confidence, 0.95f);
}

TEST(AndroidAutoUiRendererTest, RenderTemplate) {
    AndroidAutoUiRenderer renderer;

    std::string payload = R"({"title": "Test Media"})";
    auto result = renderer.render_template(IUiRenderer::TemplateType::MediaTemplate, payload);
    EXPECT_TRUE(result.is_ok());

    EXPECT_EQ(renderer.get_last_template_type(), IUiRenderer::TemplateType::MediaTemplate);
    EXPECT_EQ(renderer.get_last_template_payload(), payload);
}

TEST(AndroidAutoUiRendererTest, SurfaceSize) {
    AndroidAutoUiRenderer renderer;

    auto result = renderer.set_surface_size(1920, 1080);
    EXPECT_TRUE(result.is_ok());

    auto size = renderer.get_surface_size();
    EXPECT_EQ(size.first, 1920);
    EXPECT_EQ(size.second, 1080);
}

TEST(AndroidAutoCapabilityProviderTest, Capabilities) {
    AndroidAutoCapabilityProvider provider;

    const auto& caps = provider.get_capabilities();
    EXPECT_TRUE(caps.supports_touch);
    EXPECT_TRUE(caps.supports_rotary);
    EXPECT_TRUE(caps.supports_keyboard);
    EXPECT_TRUE(caps.supports_voice);
    EXPECT_TRUE(caps.supports_video);
    EXPECT_TRUE(caps.supports_audio);
    EXPECT_TRUE(caps.supports_navigation);
    EXPECT_EQ(caps.max_video_width, 1920);
    EXPECT_EQ(caps.max_video_height, 1080);
    EXPECT_EQ(caps.supported_codecs.size(), 3);
    EXPECT_EQ(caps.supported_templates.size(), 9);
}

TEST(AndroidAutoCapabilityProviderTest, Negotiation) {
    AndroidAutoCapabilityProvider provider;

    Capabilities requested;
    requested.supports_touch = true;
    requested.supports_rotary = true;
    requested.supports_video = true;
    requested.max_video_width = 3840;
    requested.max_video_height = 2160;
    requested.supported_codecs = {"H264", "VP8", "VP9", "AV1"};
    requested.supported_templates = {"MediaTemplate", "ListTemplate", "CustomTemplate"};

    auto result = provider.negotiate_capabilities(requested);
    ASSERT_TRUE(result.is_ok());

    const auto& negotiated = result.value();
    EXPECT_TRUE(negotiated.supports_touch);
    EXPECT_TRUE(negotiated.supports_rotary);
    EXPECT_TRUE(negotiated.supports_video);
    EXPECT_EQ(negotiated.max_video_width, 1920);  // Capped
    EXPECT_EQ(negotiated.max_video_height, 1080);  // Capped
    EXPECT_EQ(negotiated.supported_codecs.size(), 3);  // Only common
    EXPECT_EQ(negotiated.supported_templates.size(), 2);  // Only common
}

TEST(AndroidAutoAdapterFactoryTest, Registration) {
    // Check that adapter is registered
    auto& factory = platform::AdapterFactory::instance();
    auto adapters = factory.get_registered_adapters();

    EXPECT_TRUE(std::find(adapters.begin(), adapters.end(), "android_auto") != adapters.end());

    // Create instance
    auto adapter = factory.create("android_auto");
    ASSERT_NE(adapter, nullptr);
    EXPECT_EQ(adapter->get_metadata().name, "android_auto");
}