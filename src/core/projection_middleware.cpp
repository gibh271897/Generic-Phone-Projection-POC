/**
 * @file projection_middleware.cpp
 * @brief Main projection middleware facade implementation
 */

#include "core/interfaces.hpp"
#include "common/common.hpp"
#include <memory>

namespace phone_projection {
namespace core {

class ProjectionMiddlewareImpl : public IProjectionMiddleware {
public:
    ProjectionMiddlewareImpl() {
        session_manager_ = create_session_manager();
        lifecycle_manager_ = create_lifecycle_manager();
        event_router_ = create_event_router();
        config_manager_ = create_config_manager();
        error_handler_ = create_error_handler();

        // Wire up internal callbacks
        setup_callbacks();

        // Wire up connection manager provider after lifecycle is initialized
        setup_connection_manager_provider();
    }

    ~ProjectionMiddlewareImpl() override = default;

    ISessionManager* session_manager() override {
        return session_manager_.get();
    }

    ILifecycleManager* lifecycle_manager() override {
        return lifecycle_manager_.get();
    }

    IEventRouter* event_router() override {
        return event_router_.get();
    }

    IConfigurationManager* config_manager() override {
        return config_manager_.get();
    }

    IErrorHandler* error_handler() override {
        return error_handler_.get();
    }

    Result<std::string> create_and_connect_session(const VehicleProfile& profile) override {
        auto session_result = session_manager_->create_session(profile);
        if (session_result.is_error()) {
            return Result<std::string>(session_result.error());
        }

        std::string session_id = session_result.value();

        auto connect_result = session_manager_->connect(session_id);
        if (connect_result.is_error()) {
            session_manager_->destroy_session(session_id);
            return Result<std::string>(connect_result.error());
        }

        return Result<std::string>(std::move(session_id));
    }

    std::string get_version() const override {
        return "1.0.0";
    }

    // Public API for simulation/testing - uses virtual interface methods
    Result<void> simulate_input_event([[maybe_unused]] const std::string& session_id, const Event& event) override {
        auto* input_handler = lifecycle_manager_->get_input_handler();
        if (!input_handler) {
            return Result<void>(make_error_code(ErrorCode::AdapterNotFound));
        }

        return input_handler->send_input(event);
    }

    Result<void> render_template(IUiRenderer::TemplateType type, const std::string& json_payload) override {
        auto* ui_renderer = lifecycle_manager_->get_ui_renderer();
        if (!ui_renderer) {
            return Result<void>(make_error_code(ErrorCode::AdapterNotFound));
        }

        return ui_renderer->render_template(type, json_payload);
    }

    Result<IResourceManager::ResourceGrant> request_resource(
        IResourceManager::ResourceType type, const std::string& session_id) override {
        auto* resource_mgr = lifecycle_manager_->get_resource_manager();
        if (!resource_mgr) {
            return Result<IResourceManager::ResourceGrant>(make_error_code(ErrorCode::AdapterNotFound));
        }

        return resource_mgr->request_resource(type, session_id);
    }

    Result<void> release_resource(const std::string& token) override {
        auto* resource_mgr = lifecycle_manager_->get_resource_manager();
        if (!resource_mgr) {
            return Result<void>(make_error_code(ErrorCode::AdapterNotFound));
        }

        return resource_mgr->release_resource(token);
    }

private:
    void setup_callbacks() {
        // Session state changes -> event router
        session_manager_->register_state_callback([this](const std::string& session_id,
                                                         [[maybe_unused]] SessionState old_state,
                                                         SessionState new_state) {
            Event event(EventType::SessionCreated, session_id, "core");
            // Map state to event type
            switch (new_state) {
                case SessionState::Connecting:
                    event.type = EventType::SessionCreated;
                    break;
                case SessionState::Connected:
                    event.type = EventType::SessionConnected;
                    break;
                case SessionState::Disconnecting:
                case SessionState::Disconnected:
                    event.type = EventType::SessionDisconnected;
                    break;
                case SessionState::Error:
                    event.type = EventType::SessionError;
                    break;
            }
            event_router_->publish(event);
        });

        // Error handler -> event router
        error_handler_->register_error_callback([this]([[maybe_unused]] ErrorCode code,
                                                       [[maybe_unused]] const std::string& context,
                                                       [[maybe_unused]] const std::string& message) {
            Event event(EventType::SessionError, "", "core");
            event_router_->publish(event);
        });

        // Config changes -> event router
        config_manager_->register_config_callback([this]([[maybe_unused]] const std::string& key,
                                                         [[maybe_unused]] const std::string& value) {
            Event event(EventType::ConfigChanged, "", "core");
            event_router_->publish(event);
        });
    }

    void setup_connection_manager_provider() {
        // Use the interface method to set the provider
        session_manager_->set_connection_manager_provider([this]() {
            return lifecycle_manager_->get_connection_manager();
        });
    }

    std::unique_ptr<ISessionManager> session_manager_;
    std::unique_ptr<ILifecycleManager> lifecycle_manager_;
    std::unique_ptr<IEventRouter> event_router_;
    std::unique_ptr<IConfigurationManager> config_manager_;
    std::unique_ptr<IErrorHandler> error_handler_;
};

std::unique_ptr<IProjectionMiddleware> create_projection_middleware() {
    return std::make_unique<ProjectionMiddlewareImpl>();
}

} // namespace core
} // namespace phone_projection