/**
 * @file test_integration.cpp
 * @brief Integration tests for end-to-end session flow
 */

#include <gtest/gtest.h>
#include "core/interfaces.hpp"
#include "platform/platform_adapter.hpp"
#include "common/common.hpp"
#include <thread>
#include <chrono>

using namespace phone_projection;
using namespace phone_projection::core;

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        middleware_ = create_projection_middleware();
        auto result = middleware_->lifecycle_manager()->initialize();
        ASSERT_TRUE(result.is_ok());
    }

    void TearDown() override {
        if (middleware_) {
            middleware_->lifecycle_manager()->shutdown();
        }
    }

    std::unique_ptr<IProjectionMiddleware> middleware_;
};

TEST_F(IntegrationTest, FullSessionLifecycle) {
    // Create vehicle profile
    VehicleProfile profile;
    profile.vehicle_id = "test_vehicle";
    profile.make = "TestMake";
    profile.model = "TestModel";
    profile.year = "2024";
    profile.preferred_transport = "usb";
    profile.screen_width = 1280;
    profile.screen_height = 720;
    profile.capabilities.supports_touch = true;
    profile.capabilities.supports_rotary = true;
    profile.capabilities.supports_keyboard = true;
    profile.capabilities.supports_voice = true;
    profile.capabilities.supports_video = true;
    profile.capabilities.supports_audio = true;
    profile.capabilities.supports_navigation = true;
    profile.capabilities.max_video_width = 1920;
    profile.capabilities.max_video_height = 1080;
    profile.capabilities.supported_codecs = {"H264", "VP8"};
    profile.capabilities.supported_templates = {"MediaTemplate", "NavigationTemplate"};

    // Add profile to config
    middleware_->config_manager()->add_profile("test_vehicle", profile);
    middleware_->config_manager()->set_active_profile("test_vehicle");

    // Create and connect session
    auto session_result = middleware_->create_and_connect_session(profile);
    ASSERT_TRUE(session_result.is_ok()) << "Failed to create session: " << session_result.error().message();

    std::string session_id = session_result.value();
    EXPECT_FALSE(session_id.empty());

    // Wait for connection to complete (async)
    bool connected = false;
    auto token_result = middleware_->event_router()->subscribe(EventType::SessionConnected,
        [&connected, &session_id](const Event& e) {
            if (e.session_id == session_id) {
                connected = true;
            }
        });
    ASSERT_TRUE(token_result.is_ok());
    auto token = token_result.value();

    // Wait for connection with timeout
    int wait_count = 0;
    while (!connected && wait_count < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        wait_count++;
    }

    EXPECT_TRUE(connected) << "Session did not connect within timeout";

    // Verify session state
    auto state_result = middleware_->session_manager()->get_session_state(session_id);
    ASSERT_TRUE(state_result.is_ok());
    EXPECT_EQ(state_result.value(), SessionState::Connected);

    // Verify negotiated capabilities
    auto caps_result = middleware_->session_manager()->get_capabilities(session_id);
    ASSERT_TRUE(caps_result.is_ok());
    const auto& caps = caps_result.value();
    EXPECT_TRUE(caps.supports_touch);
    EXPECT_TRUE(caps.supports_rotary);
    EXPECT_TRUE(caps.supports_voice);
    EXPECT_TRUE(caps.supports_video);
    EXPECT_TRUE(caps.supports_audio);
    EXPECT_TRUE(caps.supports_navigation);

    // Test resource management
    auto audio_result = middleware_->request_resource(
        IResourceManager::ResourceType::AudioFocus, session_id);
    ASSERT_TRUE(audio_result.is_ok());
    EXPECT_TRUE(audio_result.value().granted);
    EXPECT_FALSE(audio_result.value().token.empty());

    auto video_result = middleware_->request_resource(
        IResourceManager::ResourceType::VideoSurface, session_id);
    ASSERT_TRUE(video_result.is_ok());
    EXPECT_TRUE(video_result.value().granted);

    // Test input simulation
    TouchEvent touch(TouchEvent::Action::Down, 0.5f, 0.5f, session_id);
    auto input_result = middleware_->simulate_input_event(session_id, touch);
    EXPECT_TRUE(input_result.is_ok());

    // Test UI rendering
    std::string template_json = R"({"title": "Test"})";
    auto render_result = middleware_->render_template(
        IUiRenderer::TemplateType::MediaTemplate, template_json);
    EXPECT_TRUE(render_result.is_ok());

    // Release resources
    middleware_->release_resource(audio_result.value().token);
    middleware_->release_resource(video_result.value().token);

    // Disconnect
    auto disconnect_result = middleware_->session_manager()->disconnect(session_id);
    EXPECT_TRUE(disconnect_result.is_ok());

    // Wait for disconnect
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Verify disconnected state
    auto final_state = middleware_->session_manager()->get_session_state(session_id);
    ASSERT_TRUE(final_state.is_ok());
    EXPECT_EQ(final_state.value(), SessionState::Disconnected);

    // Cleanup unsubscribe
    middleware_->event_router()->unsubscribe(token);
}

TEST_F(IntegrationTest, ReconnectAfterDisconnect) {
    VehicleProfile profile;
    profile.vehicle_id = "reconnect_test";
    profile.make = "Test";
    profile.model = "Reconnect";
    profile.capabilities.supports_touch = true;
    profile.capabilities.supports_audio = true;

    middleware_->config_manager()->add_profile("reconnect_test", profile);
    middleware_->config_manager()->set_active_profile("reconnect_test");

    // Create and connect
    auto session_result = middleware_->create_and_connect_session(profile);
    ASSERT_TRUE(session_result.is_ok());
    std::string session_id = session_result.value();

    // Wait for connection
    bool connected = false;
    auto token_result = middleware_->event_router()->subscribe(EventType::SessionConnected,
        [&connected, &session_id](const Event& e) {
            if (e.session_id == session_id) connected = true;
        });
    ASSERT_TRUE(token_result.is_ok());
    auto token = token_result.value();

    int wait_count = 0;
    while (!connected && wait_count < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        wait_count++;
    }
    EXPECT_TRUE(connected);

    // Disconnect
    middleware_->session_manager()->disconnect(session_id);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Reconnect
    auto reconnect_result = middleware_->session_manager()->reconnect(session_id);
    EXPECT_TRUE(reconnect_result.is_ok());

    // Wait for reconnection
    connected = false;
    wait_count = 0;
    while (!connected && wait_count < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        wait_count++;
    }
    EXPECT_TRUE(connected);

    // Cleanup
    middleware_->session_manager()->disconnect(session_id);
    middleware_->event_router()->unsubscribe(token);
}

TEST_F(IntegrationTest, AndroidAutoAdapterRegistration) {
    // Verify Android Auto adapter is registered
    auto& factory = platform::AdapterFactory::instance();
    auto adapters = factory.get_registered_adapters();

    EXPECT_TRUE(std::find(adapters.begin(), adapters.end(), "android_auto") != adapters.end())
        << "Android Auto adapter not registered";

    // Create instance
    auto adapter = factory.create("android_auto");
    ASSERT_NE(adapter, nullptr);
    EXPECT_EQ(adapter->get_metadata().name, "android_auto");
    EXPECT_EQ(adapter->get_metadata().version, "1.0.0");
    EXPECT_TRUE(std::find(adapter->get_metadata().supported_transports.begin(),
                          adapter->get_metadata().supported_transports.end(), "usb")
            != adapter->get_metadata().supported_transports.end());
    EXPECT_TRUE(std::find(adapter->get_metadata().supported_transports.begin(),
                          adapter->get_metadata().supported_transports.end(), "bluetooth")
            != adapter->get_metadata().supported_transports.end());

    // Verify components exist
    EXPECT_NE(adapter->get_connection_manager(), nullptr);
    EXPECT_NE(adapter->get_resource_manager(), nullptr);
    EXPECT_NE(adapter->get_input_handler(), nullptr);
    EXPECT_NE(adapter->get_ui_renderer(), nullptr);
    EXPECT_NE(adapter->get_capability_provider(), nullptr);

    // Test capability provider
    auto* cap_provider = adapter->get_capability_provider();
    const auto& caps = cap_provider->get_capabilities();
    EXPECT_TRUE(caps.supports_touch);
    EXPECT_TRUE(caps.supports_rotary);
    EXPECT_TRUE(caps.supports_keyboard);
    EXPECT_TRUE(caps.supports_voice);
    EXPECT_EQ(caps.supported_codecs.size(), 3);  // H264, VP8, VP9
    EXPECT_EQ(caps.supported_templates.size(), 9);
}

TEST_F(IntegrationTest, EventRouterTokenBasedSubscribe) {
    auto& router = *middleware_->event_router();

    int event_count = 0;
    std::string last_session_id;

    // Subscribe and get token
    auto token_result = router.subscribe(EventType::SessionCreated,
        [&event_count, &last_session_id](const Event& e) {
            event_count++;
            last_session_id = e.session_id;
        });
    ASSERT_TRUE(token_result.is_ok());
    auto token = token_result.value();

    // Publish events
    router.publish(Event(EventType::SessionCreated, "session1"));
    router.publish(Event(EventType::SessionCreated, "session2"));
    router.publish(Event(EventType::SessionConnected, "session1"));  // Different type

    EXPECT_EQ(event_count, 2);
    EXPECT_EQ(last_session_id, "session2");

    // Unsubscribe using token
    auto unsub_result = router.unsubscribe(token);
    EXPECT_TRUE(unsub_result.is_ok());

    // Publish again - should not be received
    router.publish(Event(EventType::SessionCreated, "session3"));
    EXPECT_EQ(event_count, 2);

    // Try to unsubscribe again - should fail
    auto unsub_result2 = router.unsubscribe(token);
    EXPECT_TRUE(unsub_result2.is_error());
    EXPECT_EQ(unsub_result2.error(), make_error_code(ErrorCode::InvalidArgument));
}

TEST_F(IntegrationTest, MultiSessionManagement) {
    VehicleProfile profile;
    profile.vehicle_id = "multi_test";
    profile.capabilities.supports_touch = true;
    profile.capabilities.supports_audio = true;

    middleware_->config_manager()->add_profile("multi_test", profile);
    middleware_->config_manager()->set_active_profile("multi_test");

    // Create multiple sessions (but connect sequentially - one connection at a time per adapter)
    auto session1 = middleware_->session_manager()->create_session(profile);
    auto session2 = middleware_->session_manager()->create_session(profile);
    auto session3 = middleware_->session_manager()->create_session(profile);

    ASSERT_TRUE(session1.is_ok());
    ASSERT_TRUE(session2.is_ok());
    ASSERT_TRUE(session3.is_ok());

    EXPECT_NE(session1.value(), session2.value());
    EXPECT_NE(session2.value(), session3.value());
    EXPECT_NE(session1.value(), session3.value());

    // Connect session1
    auto connect1 = middleware_->session_manager()->connect(session1.value());
    ASSERT_TRUE(connect1.is_ok());

    // Wait for session1 to connect
    bool connected1 = false;
    auto token1_result = middleware_->event_router()->subscribe(EventType::SessionConnected,
        [&connected1, &session1](const Event& e) {
            if (e.session_id == session1.value()) connected1 = true;
        });
    ASSERT_TRUE(token1_result.is_ok());
    auto token1 = token1_result.value();
    int wait_count = 0;
    while (!connected1 && wait_count < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        wait_count++;
    }
    EXPECT_TRUE(connected1);
    middleware_->event_router()->unsubscribe(token1);

    // Verify session1 is connected
    auto state1 = middleware_->session_manager()->get_session_state(session1.value());
    ASSERT_TRUE(state1.is_ok());
    EXPECT_EQ(state1.value(), SessionState::Connected);

    // Disconnect session1 before connecting session2
    middleware_->session_manager()->disconnect(session1.value());
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Connect session2
    auto connect2 = middleware_->session_manager()->connect(session2.value());
    ASSERT_TRUE(connect2.is_ok());

    // Wait for session2 to connect
    bool connected2 = false;
    auto token2_result = middleware_->event_router()->subscribe(EventType::SessionConnected,
        [&connected2, &session2](const Event& e) {
            if (e.session_id == session2.value()) connected2 = true;
        });
    ASSERT_TRUE(token2_result.is_ok());
    auto token2 = token2_result.value();
    wait_count = 0;
    while (!connected2 && wait_count < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        wait_count++;
    }
    EXPECT_TRUE(connected2);
    middleware_->event_router()->unsubscribe(token2);

    // Verify session2 is connected
    auto state2 = middleware_->session_manager()->get_session_state(session2.value());
    ASSERT_TRUE(state2.is_ok());
    EXPECT_EQ(state2.value(), SessionState::Connected);

    // Disconnect session2 before connecting session3
    middleware_->session_manager()->disconnect(session2.value());
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Connect session3
    auto connect3 = middleware_->session_manager()->connect(session3.value());
    ASSERT_TRUE(connect3.is_ok());

    // Wait for session3 to connect
    bool connected3 = false;
    auto token3_result = middleware_->event_router()->subscribe(EventType::SessionConnected,
        [&connected3, &session3](const Event& e) {
            if (e.session_id == session3.value()) connected3 = true;
        });
    ASSERT_TRUE(token3_result.is_ok());
    auto token3 = token3_result.value();
    wait_count = 0;
    while (!connected3 && wait_count < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        wait_count++;
    }
    EXPECT_TRUE(connected3);
    middleware_->event_router()->unsubscribe(token3);

    // Verify session3 is connected
    auto state3 = middleware_->session_manager()->get_session_state(session3.value());
    ASSERT_TRUE(state3.is_ok());
    EXPECT_EQ(state3.value(), SessionState::Connected);

    // Disconnect all
    middleware_->session_manager()->disconnect(session1.value());
    middleware_->session_manager()->disconnect(session2.value());
    middleware_->session_manager()->disconnect(session3.value());

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}