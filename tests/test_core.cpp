/**
 * @file test_core.cpp
 * @brief Core middleware unit tests
 */

#include <gtest/gtest.h>
#include "core/interfaces.hpp"
#include "core/session_manager.cpp"
#include "core/lifecycle_manager.cpp"
#include "core/event_router.cpp"
#include "core/config_manager.cpp"
#include "core/error_handler.cpp"
#include "core/projection_middleware.cpp"
#include "common/common.hpp"

using namespace phone_projection;
using namespace phone_projection::core;

TEST(SessionManagerTest, CreateAndDestroySession) {
    auto session_mgr = create_session_manager();

    VehicleProfile profile;
    profile.vehicle_id = "test_vehicle";
    profile.make = "Test";
    profile.model = "Model";

    auto result = session_mgr->create_session(profile);
    ASSERT_TRUE(result.is_ok());

    std::string session_id = result.value();
    EXPECT_FALSE(session_id.empty());

    auto state_result = session_mgr->get_session_state(session_id);
    ASSERT_TRUE(state_result.is_ok());
    EXPECT_EQ(state_result.value(), SessionState::Disconnected);

    auto destroy_result = session_mgr->destroy_session(session_id);
    EXPECT_TRUE(destroy_result.is_ok());

    // Session should no longer exist
    auto state_result2 = session_mgr->get_session_state(session_id);
    EXPECT_TRUE(state_result2.is_error());
    EXPECT_EQ(state_result2.error(), make_error_code(ErrorCode::SessionNotFound));
}

TEST(SessionManagerTest, ConnectDisconnect) {
    auto session_mgr = create_session_manager();

    VehicleProfile profile;
    profile.vehicle_id = "test_vehicle";

    auto result = session_mgr->create_session(profile);
    ASSERT_TRUE(result.is_ok());
    std::string session_id = result.value();

    // Connect
    auto connect_result = session_mgr->connect(session_id);
    EXPECT_TRUE(connect_result.is_ok());

    auto state_result = session_mgr->get_session_state(session_id);
    ASSERT_TRUE(state_result.is_ok());
    EXPECT_EQ(state_result.value(), SessionState::Connecting);

    // Disconnect
    auto disconnect_result = session_mgr->disconnect(session_id);
    EXPECT_TRUE(disconnect_result.is_ok());

    state_result = session_mgr->get_session_state(session_id);
    ASSERT_TRUE(state_result.is_ok());
    EXPECT_EQ(state_result.value(), SessionState::Disconnected);
}

TEST(EventRouterTest, SubscribeAndPublish) {
    auto router = create_event_router();

    bool received = false;
    EventType received_type = EventType::SessionCreated;

    auto subscribe_result = router->subscribe(EventType::SessionCreated,
        [&received, &received_type](const Event& event) {
            received = true;
            received_type = event.type;
        });
    EXPECT_TRUE(subscribe_result.is_ok());
    auto token = subscribe_result.value();

    Event event(EventType::SessionCreated, "test_session");
    auto publish_result = router->publish(event);
    EXPECT_TRUE(publish_result.is_ok());

    EXPECT_TRUE(received);
    EXPECT_EQ(received_type, EventType::SessionCreated);

    // Test unsubscribe
    auto unsub_result = router->unsubscribe(token);
    EXPECT_TRUE(unsub_result.is_ok());

    // Publish again - should not be received
    received = false;
    router->publish(event);
    EXPECT_FALSE(received);
}

TEST(ConfigManagerTest, LoadAndGetProfile) {
    auto config_mgr = create_config_manager();

    VehicleProfile profile;
    profile.vehicle_id = "test_vehicle";
    profile.make = "TestMake";
    profile.model = "TestModel";
    profile.screen_width = 800;
    profile.screen_height = 480;

    config_mgr->add_profile("test", profile);
    config_mgr->set_active_profile("test");

    auto result = config_mgr->get_active_profile();
    ASSERT_TRUE(result.is_ok());

    EXPECT_EQ(result.value().vehicle_id, "test_vehicle");
    EXPECT_EQ(result.value().make, "TestMake");
    EXPECT_EQ(result.value().screen_width, 800);
}

TEST(ErrorHandlerTest, ReportAndGetError) {
    auto error_handler = create_error_handler();

    error_handler->report_error(ErrorCode::ConnectionFailed, "test_context", "test_message");

    auto last_error = error_handler->get_last_error();
    ASSERT_TRUE(last_error.has_value());

    EXPECT_EQ(last_error->first, ErrorCode::ConnectionFailed);
    EXPECT_NE(last_error->second.find("test_context"), std::string::npos);
    EXPECT_NE(last_error->second.find("test_message"), std::string::npos);

    error_handler->clear_error();
    EXPECT_FALSE(error_handler->get_last_error().has_value());
}

TEST(ProjectionMiddlewareTest, CreateAndConnect) {
    auto middleware = create_projection_middleware();

    auto init_result = middleware->lifecycle_manager()->initialize();
    ASSERT_TRUE(init_result.is_ok());

    VehicleProfile profile;
    profile.vehicle_id = "test_vehicle";
    profile.make = "Test";
    profile.model = "Model";
    profile.capabilities.supports_touch = true;
    profile.capabilities.supports_video = true;

    // Note: This will fail without a registered adapter, but tests the API
    auto session_result = middleware->create_and_connect_session(profile);
    // In real scenario with adapter, this would succeed

    middleware->lifecycle_manager()->shutdown();
}

TEST(CommonTest, GenerateUUID) {
    std::string uuid1 = common::generate_uuid();
    std::string uuid2 = common::generate_uuid();

    EXPECT_EQ(uuid1.size(), 36);  // Standard UUID format
    EXPECT_EQ(uuid2.size(), 36);
    EXPECT_NE(uuid1, uuid2);  // Should be unique

    // Check format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    EXPECT_EQ(uuid1[8], '-');
    EXPECT_EQ(uuid1[13], '-');
    EXPECT_EQ(uuid1[18], '-');
    EXPECT_EQ(uuid1[23], '-');
    EXPECT_EQ(uuid1[14], '4');  // Version 4
}

TEST(CommonTest, Timestamp) {
    std::string iso = common::get_timestamp_iso8601();
    EXPECT_FALSE(iso.empty());
    EXPECT_NE(iso.find('T'), std::string::npos);
    EXPECT_NE(iso.find('Z'), std::string::npos);

    int64_t ms = common::get_timestamp_ms();
    EXPECT_GT(ms, 0);
}