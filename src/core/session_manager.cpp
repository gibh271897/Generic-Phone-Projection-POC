/**
 * @file session_manager.cpp
 * @brief Session manager implementation
 */

#include "core/interfaces.hpp"
#include "common/common.hpp"
#include "platform/connection_manager.hpp"
#include <unordered_map>
#include <mutex>
#include <functional>
#include <memory>

namespace phone_projection {
namespace core {

class SessionManagerImpl : public ISessionManager {
public:
    SessionManagerImpl() = default;
    ~SessionManagerImpl() override = default;

    /** Set the connection manager factory for wiring up connection callbacks */
    void set_connection_manager_provider(std::function<core::IConnectionManager*()> provider) {
        connection_manager_provider_ = std::move(provider);
    }

    Result<std::string> create_session(const VehicleProfile& profile) override {
        std::lock_guard<std::mutex> lock(mutex_);

        std::string session_id = common::generate_uuid();

        SessionInfo info;
        info.profile = profile;
        info.state = SessionState::Disconnected;
        info.capabilities = profile.capabilities;  // Start with vehicle capabilities

        sessions_[session_id] = std::move(info);

        PP_LOG_INFO("Created session {} for vehicle {}", session_id, profile.vehicle_id);

        return Result<std::string>(std::move(session_id));
    }

    Result<void> destroy_session(const std::string& session_id) override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = sessions_.find(session_id);
        if (it == sessions_.end()) {
            return Result<void>(make_error_code(ErrorCode::SessionNotFound));
        }

        if (it->second.state != SessionState::Disconnected) {
            return Result<void>(make_error_code(ErrorCode::SessionInvalidState));
        }

        sessions_.erase(it);
        PP_LOG_INFO("Destroyed session {}", session_id);

        return Result<void>();
    }

    Result<SessionState> get_session_state(const std::string& session_id) const override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = sessions_.find(session_id);
        if (it == sessions_.end()) {
            return Result<SessionState>(make_error_code(ErrorCode::SessionNotFound));
        }

        return Result<SessionState>(it->second.state);
    }

    Result<Capabilities> get_capabilities(const std::string& session_id) const override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = sessions_.find(session_id);
        if (it == sessions_.end()) {
            return Result<Capabilities>(make_error_code(ErrorCode::SessionNotFound));
        }

        return Result<Capabilities>(it->second.capabilities);
    }

    Result<void> connect(const std::string& session_id) override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = sessions_.find(session_id);
        if (it == sessions_.end()) {
            return Result<void>(make_error_code(ErrorCode::SessionNotFound));
        }

        if (it->second.state != SessionState::Disconnected) {
            return Result<void>(make_error_code(ErrorCode::SessionInvalidState));
        }

        it->second.state = SessionState::Connecting;
        notify_state_change(session_id, SessionState::Disconnected, SessionState::Connecting);

        PP_LOG_INFO("Session {} connecting...", session_id);

        // Wire up connection completion callback
        if (connection_manager_provider_) {
            auto* conn_mgr = connection_manager_provider_();
            if (conn_mgr) {
                conn_mgr->register_connection_complete_callback(
                    [this, session_id](const Capabilities& caps) {
                        set_session_connected(session_id, caps);
                    }
                );
                // Actually initiate the connection
                auto result = conn_mgr->connect(it->second.profile);
                if (result.is_error()) {
                    it->second.state = SessionState::Error;
                    notify_state_change(session_id, SessionState::Connecting, SessionState::Error);
                    return result;
                }
            }
        }

        return Result<void>();
    }

    Result<void> disconnect(const std::string& session_id) override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = sessions_.find(session_id);
        if (it == sessions_.end()) {
            return Result<void>(make_error_code(ErrorCode::SessionNotFound));
        }

        if (it->second.state == SessionState::Disconnected) {
            return Result<void>();  // Already disconnected
        }

        SessionState old_state = it->second.state;
        it->second.state = SessionState::Disconnecting;
        notify_state_change(session_id, old_state, SessionState::Disconnecting);

        PP_LOG_INFO("Session {} disconnecting...Need to close connect mngr", session_id);

        // Disconnect the underlying connection
        if (connection_manager_provider_) {
            auto* conn_mgr = connection_manager_provider_();
            if (conn_mgr) {
                conn_mgr->disconnect();
            }
        }

        // After disconnection, setting up session state to Disconnected for reset state
        it->second.state = SessionState::Disconnected;
        notify_state_change(session_id, SessionState::Disconnecting, SessionState::Disconnected);
        PP_LOG_INFO("Session {} disconnected", session_id);

        return Result<void>();
    }

    Result<void> reconnect(const std::string& session_id) override {
        // Check if session exists and is in valid state ( but no holding of lock during connect, can cause deadloac)
        {
            std::lock_guard<std::mutex> lock(mutex_);

            auto it = sessions_.find(session_id);
            if (it == sessions_.end()) {
                return Result<void>(make_error_code(ErrorCode::SessionNotFound));
            }

            // Only allow reconnect from Disconnected or Error state
            if (it->second.state != SessionState::Disconnected &&
                it->second.state != SessionState::Error) {
                return Result<void>(make_error_code(ErrorCode::SessionInvalidState));
            }
        }

        PP_LOG_INFO("Session {} reconnecting...", session_id);
        return connect(session_id);
    }

    void register_state_callback(SessionStateCallback callback) override {
        state_callback_ = std::move(callback);
    }

    // Internal methods for adapter to call
    void set_session_connected(const std::string& session_id, const Capabilities& negotiated_caps) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            SessionState old_state = it->second.state;
            it->second.state = SessionState::Connected;
            it->second.capabilities = negotiated_caps;
            notify_state_change(session_id, old_state, SessionState::Connected);
            PP_LOG_INFO("Session {} connected with negotiated capabilities", session_id);
        }
    }

    void set_session_disconnected(const std::string& session_id) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            SessionState old_state = it->second.state;
            it->second.state = SessionState::Disconnected;
            notify_state_change(session_id, old_state, SessionState::Disconnected);
            PP_LOG_INFO("Session {} disconnected", session_id);
        }
    }

    void set_session_error(const std::string& session_id, ErrorCode error) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            SessionState old_state = it->second.state;
            it->second.state = SessionState::Error;
            notify_state_change(session_id, old_state, SessionState::Error);
            PP_LOG_ERROR("Session {} error: {}", session_id, static_cast<int>(error));
        }
    }

private:
    struct SessionInfo {
        VehicleProfile profile;
        SessionState state = SessionState::Disconnected;
        Capabilities capabilities;
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, SessionInfo> sessions_;
    SessionStateCallback state_callback_;
    std::function<core::IConnectionManager*()> connection_manager_provider_;

    void notify_state_change(const std::string& session_id, SessionState old_state, SessionState new_state) {
        if (state_callback_) {
            state_callback_(session_id, old_state, new_state);
        }
    }
};

std::unique_ptr<ISessionManager> create_session_manager() {
    return std::make_unique<SessionManagerImpl>();
}

} // namespace core
} // namespace phone_projection