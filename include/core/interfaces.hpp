/**
 * @file interfaces.hpp
 * @brief Platform-independent interface definitions for Phone Projection Middleware
 *
 * This header defines all pure virtual interfaces that form the contract between
 * the middleware core and platform-specific adapters.
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <optional>
#include <variant>
#include <cstdint>

namespace phone_projection {
namespace core {

// ============================================================================
// Common Types
// ============================================================================

/** Result type for operations that can fail */
template<typename T>
class Result {
public:
    Result(T&& value) : value_(std::move(value)), has_value_(true) {}
    Result(const T& value) : value_(value), has_value_(true) {}
    Result(std::error_code error) : error_(error), has_value_(false) {}

    bool is_ok() const { return has_value_; }
    bool is_error() const { return !has_value_; }

    T& value() {
        if (!has_value_) throw std::bad_optional_access();
        return value_;
    }
    const T& value() const {
        if (!has_value_) throw std::bad_optional_access();
        return value_;
    }
    std::error_code error() const { return error_; }

private:
    T value_;
    std::error_code error_;
    bool has_value_ = false;
};

/** Specialization for void */
template<>
class Result<void> {
public:
    Result() : has_value_(true) {}
    Result(std::error_code error) : error_(error), has_value_(false) {}

    bool is_ok() const { return has_value_; }
    bool is_error() const { return !has_value_; }

    void value() const {
        if (!has_value_) throw std::bad_optional_access();
    }
    std::error_code error() const { return error_; }

private:
    std::error_code error_;
    bool has_value_ = false;
};

/** Error codes for the middleware */
enum class ErrorCode : int {
    Success = 0,
    // Session errors
    SessionNotFound = 1000,
    SessionAlreadyExists,
    SessionInvalidState,
    SessionTimeout,
    // Connection errors
    ConnectionFailed = 2000,
    ConnectionLost,
    ConnectionRejected,
    TransportUnavailable,
    // Resource errors
    ResourceUnavailable = 3000,
    ResourceBusy,
    ResourceDenied,
    // Configuration errors
    ConfigNotFound = 4000,
    ConfigInvalid,
    ConfigParseError,
    // Adapter errors
    AdapterNotFound = 5000,
    AdapterLoadFailed,
    AdapterIncompatible,
    // Generic
    NotImplemented = 9000,
    InternalError,
    InvalidArgument,
    OperationCancelled
};

inline std::error_code make_error_code(ErrorCode e) {
    return std::error_code(static_cast<int>(e), std::generic_category());
}

/** Session states */
enum class SessionState {
    Disconnected,
    Connecting,
    Connected,
    Disconnecting,
    Error
};

/** Projection capabilities negotiated during session establishment */
struct Capabilities {
    bool supports_touch = false;
    bool supports_rotary = false;
    bool supports_keyboard = false;
    bool supports_voice = false;
    bool supports_video = false;
    bool supports_audio = false;
    bool supports_navigation = false;
    uint32_t max_video_width = 0;
    uint32_t max_video_height = 0;
    std::vector<std::string> supported_codecs;
    std::vector<std::string> supported_templates;
};

/** Vehicle profile configuration */
struct VehicleProfile {
    std::string vehicle_id;
    std::string make;
    std::string model;
    std::string year;
    std::string head_unit_model;
    std::string head_unit_firmware;
    Capabilities capabilities;
    std::string preferred_transport;  // "usb", "bluetooth", "wifi"
    uint32_t screen_width = 800;
    uint32_t screen_height = 480;
    float pixel_density = 1.0f;
    bool is_right_hand_drive = false;
};

/** Event types for the event system */
enum class EventType {
    // Session events
    SessionCreated,
    SessionConnected,
    SessionDisconnected,
    SessionError,
    SessionCapabilityChanged,
    // Input events
    TouchDown,
    TouchUp,
    TouchMove,
    RotaryClockwise,
    RotaryCounterClockwise,
    RotaryPress,
    KeyPress,
    KeyRelease,
    VoiceCommand,
    // System events
    AudioFocusGained,
    AudioFocusLost,
    VideoFocusGained,
    VideoFocusLost,
    NavigationFocusGained,
    NavigationFocusLost,
    // Configuration events
    ConfigChanged,
    ProfileChanged,
    // Adapter events
    AdapterLoaded,
    AdapterUnloaded,
    AdapterError
};

/** Base event structure */
struct Event {
    EventType type;
    std::chrono::steady_clock::time_point timestamp;
    std::string session_id;
    std::string source;  // "core", "adapter", "application"

    Event(EventType t, const std::string& sid = "", const std::string& src = "core")
        : type(t), timestamp(std::chrono::steady_clock::now()), session_id(sid), source(src) {}
    virtual ~Event() = default;
};

/** Touch event */
struct TouchEvent : Event {
    enum class Action { Down, Up, Move, Cancel };
    Action action;
    float x, y;  // Normalized 0.0-1.0
    uint32_t pointer_id = 0;
    float pressure = 1.0f;

    TouchEvent(Action a, float x_, float y_, const std::string& sid = "")
        : Event(EventType::TouchDown, sid), action(a), x(x_), y(y_) {
        type = (a == Action::Down) ? EventType::TouchDown :
               (a == Action::Up) ? EventType::TouchUp : EventType::TouchMove;
    }
};

/** Rotary event */
struct RotaryEvent : Event {
    enum class Action { Clockwise, CounterClockwise, Press, Release };
    Action action;
    int32_t steps = 1;

    RotaryEvent(Action a, int32_t s = 1, const std::string& sid = "")
        : Event(EventType::RotaryClockwise, sid), action(a), steps(s) {
        type = (a == Action::Clockwise) ? EventType::RotaryClockwise :
               (a == Action::CounterClockwise) ? EventType::RotaryCounterClockwise :
               (a == Action::Press) ? EventType::RotaryPress : EventType::KeyRelease;
    }
};

/** Key event */
struct KeyEvent : Event {
    enum class Action { Press, Release };
    Action action;
    uint32_t key_code;  // Android key codes
    uint32_t modifiers = 0;

    KeyEvent(Action a, uint32_t code, const std::string& sid = "")
        : Event(EventType::KeyPress, sid), action(a), key_code(code) {
        type = (a == Action::Press) ? EventType::KeyPress : EventType::KeyRelease;
    }
};

/** Voice event */
struct VoiceEvent : Event {
    std::string transcript;
    float confidence = 0.0f;
    bool is_final = false;

    VoiceEvent(const std::string& text, float conf, bool final, const std::string& sid = "")
        : Event(EventType::VoiceCommand, sid), transcript(text), confidence(conf), is_final(final) {}
};

// ============================================================================
// Core Interfaces (Middleware Facade)
// ============================================================================

/** Forward declarations */
class IPlatformAdapter;
class IConnectionManager;
class IResourceManager;
class IInputHandler;
class IUiRenderer;
class ICapabilityProvider;

/** Interface for session management */
class ISessionManager {
public:
    virtual ~ISessionManager() = default;

    /** Create a new projection session */
    virtual Result<std::string> create_session(const VehicleProfile& profile) = 0;

    /** Destroy a session */
    virtual Result<void> destroy_session(const std::string& session_id) = 0;

    /** Get current session state */
    virtual Result<SessionState> get_session_state(const std::string& session_id) const = 0;

    /** Get negotiated capabilities for a session */
    virtual Result<Capabilities> get_capabilities(const std::string& session_id) const = 0;

    /** Initiate connection for a session */
    virtual Result<void> connect(const std::string& session_id) = 0;

    /** Reconnect a session (from Disconnected or Error state) */
    virtual Result<void> reconnect(const std::string& session_id) = 0;

    /** Disconnect a session */
    virtual Result<void> disconnect(const std::string& session_id) = 0;

    /** Register session state change callback */
    using SessionStateCallback = std::function<void(const std::string&, SessionState, SessionState)>;
    virtual void register_state_callback(SessionStateCallback callback) = 0;

    /** Set the connection manager provider for wiring up connection callbacks */
    virtual void set_connection_manager_provider(std::function<IConnectionManager*()> provider) = 0;
};

/** Interface for lifecycle management */
class ILifecycleManager {
public:
    virtual ~ILifecycleManager() = default;

    /** Initialize the middleware */
    virtual Result<void> initialize() = 0;

    /** Shutdown the middleware */
    virtual Result<void> shutdown() = 0;

    /** Check if middleware is initialized */
    virtual bool is_initialized() const = 0;

    /** Load a platform adapter */
    virtual Result<void> load_adapter(const std::string& adapter_name) = 0;

    /** Unload a platform adapter */
    virtual Result<void> unload_adapter(const std::string& adapter_name) = 0;

    /** Get list of loaded adapters */
    virtual std::vector<std::string> get_loaded_adapters() const = 0;

    /** Get active adapter name */
    virtual std::optional<std::string> get_active_adapter() const = 0;

    /** Set active adapter */
    virtual Result<void> set_active_adapter(const std::string& adapter_name) = 0;

    /** Get active adapter instance (for simulation/testing) */
    virtual IPlatformAdapter* get_active_adapter_instance() = 0;

    /** Get connection manager from active adapter */
    virtual IConnectionManager* get_connection_manager() = 0;

    /** Get resource manager from active adapter */
    virtual IResourceManager* get_resource_manager() = 0;

    /** Get input handler from active adapter */
    virtual IInputHandler* get_input_handler() = 0;

    /** Get UI renderer from active adapter */
    virtual IUiRenderer* get_ui_renderer() = 0;

    /** Get capability provider from active adapter */
    virtual ICapabilityProvider* get_capability_provider() = 0;
};

/** Interface for event routing */
class IEventRouter {
public:
    virtual ~IEventRouter() = default;

    /** Event handler type */
    using EventHandler = std::function<void(const Event&)>;

    /** Subscription token for managing subscriptions */
    using SubscriptionToken = uint64_t;

    /** Subscribe to event type - returns token for later unsubscription */
    virtual Result<SubscriptionToken> subscribe(EventType type, EventHandler handler) = 0;

    /** Unsubscribe using token returned from subscribe() */
    virtual Result<void> unsubscribe(SubscriptionToken token) = 0;

    /** Publish event to all subscribers */
    virtual Result<void> publish(const Event& event) = 0;

    /** Publish event asynchronously */
    virtual Result<void> publish_async(std::unique_ptr<Event> event) = 0;
};

/** Interface for configuration management */
class IConfigurationManager {
public:
    virtual ~IConfigurationManager() = default;

    /** Load configuration from file */
    virtual Result<void> load_config(const std::string& config_path) = 0;

    /** Get vehicle profile */
    virtual Result<VehicleProfile> get_vehicle_profile(const std::string& profile_id) const = 0;

    /** Set active vehicle profile */
    virtual Result<void> set_active_profile(const std::string& profile_id) = 0;

    /** Get active vehicle profile */
    virtual Result<VehicleProfile> get_active_profile() const = 0;

    /** Add a vehicle profile */
    virtual Result<void> add_profile(const std::string& profile_id, const VehicleProfile& profile) = 0;

    /** Get platform-specific configuration */
    virtual Result<std::string> get_platform_config(const std::string& platform, const std::string& key) const = 0;

    /** Set platform-specific configuration */
    virtual Result<void> set_platform_config(const std::string& platform, const std::string& key, const std::string& value) = 0;

    /** Register configuration change callback */
    using ConfigChangeCallback = std::function<void(const std::string&, const std::string&)>;
    virtual void register_config_callback(ConfigChangeCallback callback) = 0;
};

/** Interface for error handling */
class IErrorHandler {
public:
    virtual ~IErrorHandler() = default;

    /** Error handler callback */
    using ErrorCallback = std::function<void(ErrorCode, const std::string&, const std::string&)>;

    /** Register error callback */
    virtual void register_error_callback(ErrorCallback callback) = 0;

    /** Report an error */
    virtual void report_error(ErrorCode code, const std::string& context, const std::string& message) = 0;

    /** Get last error */
    virtual std::optional<std::pair<ErrorCode, std::string>> get_last_error() const = 0;

    /** Clear error state */
    virtual void clear_error() = 0;
};

// ============================================================================
// Platform Adapter Interfaces
// ============================================================================

/** Base interface for all platform adapters */
class IPlatformAdapter {
public:
    virtual ~IPlatformAdapter() = default;

    /** Adapter metadata */
    struct Metadata {
        std::string name;
        std::string version;
        std::string description;
        std::vector<std::string> supported_transports;
        Capabilities max_capabilities;
    };

    /** Get adapter metadata */
    virtual const Metadata& get_metadata() const = 0;

    /** Initialize adapter with configuration */
    virtual Result<void> initialize(const std::string& config) = 0;

    /** Shutdown adapter */
    virtual Result<void> shutdown() = 0;

    /** Get connection manager */
    virtual class IConnectionManager* get_connection_manager() = 0;

    /** Get resource manager */
    virtual class IResourceManager* get_resource_manager() = 0;

    /** Get input handler */
    virtual class IInputHandler* get_input_handler() = 0;

    /** Get UI renderer */
    virtual class IUiRenderer* get_ui_renderer() = 0;

    /** Get capability provider */
    virtual class ICapabilityProvider* get_capability_provider() = 0;
};

/** Connection management interface */
class IConnectionManager {
public:
    virtual ~IConnectionManager() = default;

    /** Connection state */
    enum class State {
        Disconnected,
        Connecting,
        Connected,
        Disconnecting,
        Error
    };

    /** Connection callback */
    using ConnectionCallback = std::function<void(State, State, std::optional<ErrorCode>)>;

    /** Connection completion callback - called when connection is established with negotiated capabilities */
    using ConnectionCompleteCallback = std::function<void(const Capabilities& negotiated_caps)>;

    /** Establish connection */
    virtual Result<void> connect(const VehicleProfile& profile) = 0;

    /** Disconnect */
    virtual Result<void> disconnect() = 0;

    /** Get current connection state */
    virtual State get_state() const = 0;

    /** Register connection state callback */
    virtual void register_callback(ConnectionCallback callback) = 0;

    /** Register connection completion callback */
    virtual void register_connection_complete_callback(ConnectionCompleteCallback callback) = 0;

    /** Send raw data over connection */
    virtual Result<void> send_data(const std::vector<uint8_t>& data) = 0;

    /** Register data received callback */
    using DataCallback = std::function<void(const std::vector<uint8_t>&)>;
    virtual void register_data_callback(DataCallback callback) = 0;
};

/** Resource management interface */
class IResourceManager {
public:
    virtual ~IResourceManager() = default;

    /** Resource types */
    enum class ResourceType {
        AudioFocus,
        VideoSurface,
        NavigationFocus,
        InputFocus,
        SensorAccess
    };

    /** Resource request result */
    struct ResourceGrant {
        ResourceType type;
        bool granted;
        std::string token;  // Opaque token for release
        std::chrono::milliseconds timeout;  // 0 = no timeout
    };

    /** Request resource */
    virtual Result<ResourceGrant> request_resource(ResourceType type, const std::string& session_id) = 0;

    /** Release resource */
    virtual Result<void> release_resource(const std::string& token) = 0;

    /** Check resource availability */
    virtual bool is_resource_available(ResourceType type) const = 0;

    /** Register resource state change callback */
    using ResourceCallback = std::function<void(ResourceType, bool)>;
    virtual void register_callback(ResourceCallback callback) = 0;
};

/** Input handling interface */
class IInputHandler {
public:
    virtual ~IInputHandler() = default;

    /** Input event types */
    enum class InputType {
        Touch,
        Rotary,
        Keyboard,
        Voice,
        HardwareKey
    };

    /** Send input event to platform */
    virtual Result<void> send_input(const Event& event) = 0;

    /** Register input event callback (from platform to middleware) */
    using InputCallback = std::function<void(const Event&)>;
    virtual void register_callback(InputCallback callback) = 0;

    /** Set input focus */
    virtual Result<void> set_focus(bool focused) = 0;

    /** Get supported input types */
    virtual std::vector<InputType> get_supported_input_types() const = 0;
};

/** UI rendering interface */
class IUiRenderer {
public:
    virtual ~IUiRenderer() = default;

    /** Template types (Android Auto Car App Library style) */
    enum class TemplateType {
        NavigationTemplate,
        MediaTemplate,
        MessageTemplate,
        SignInTemplate,
        LongMessageTemplate,
        ListTemplate,
        GridTemplate,
        PaneTemplate,
        SearchTemplate,
        Custom
    };

    /** Render a template */
    virtual Result<void> render_template(TemplateType type, const std::string& json_payload) = 0;

    /** Invalidate/refresh current template */
    virtual Result<void> invalidate() = 0;

    /** Set surface dimensions */
    virtual Result<void> set_surface_size(uint32_t width, uint32_t height) = 0;

    /** Get current surface size */
    virtual std::pair<uint32_t, uint32_t> get_surface_size() const = 0;

    /** Register template callback */
    using TemplateCallback = std::function<void(TemplateType, const std::string&)>;
    virtual void register_callback(TemplateCallback callback) = 0;
};

/** Capability provider interface */
class ICapabilityProvider {
public:
    virtual ~ICapabilityProvider() = default;

    /** Get platform capabilities */
    virtual const Capabilities& get_capabilities() const = 0;

    /** Negotiate capabilities with platform */
    virtual Result<Capabilities> negotiate_capabilities(const Capabilities& requested) = 0;

    /** Check if capability is supported */
    virtual bool supports(const std::string& capability_name) const = 0;
};

// ============================================================================
// Main Middleware entry
// ============================================================================

/** Main middleware entry - single entry point for applications */
class IProjectionMiddleware {
public:
    virtual ~IProjectionMiddleware() = default;

    /** Get session manager */
    virtual ISessionManager* session_manager() = 0;

    /** Get lifecycle manager */
    virtual ILifecycleManager* lifecycle_manager() = 0;

    /** Get event router */
    virtual IEventRouter* event_router() = 0;

    /** Get configuration manager */
    virtual IConfigurationManager* config_manager() = 0;

    /** Get error handler */
    virtual IErrorHandler* error_handler() = 0;

    /** Create and connect a session in one call (convenience) */
    virtual Result<std::string> create_and_connect_session(const VehicleProfile& profile) = 0;

    /** Get middleware version */
    virtual std::string get_version() const = 0;

    // Simulation/testing APIs (avoids dynamic_cast in demo applications)
    /** Simulate input event from platform */
    virtual Result<void> simulate_input_event(const std::string& session_id, const Event& event) = 0;

    /** Render a UI template */
    virtual Result<void> render_template(IUiRenderer::TemplateType type, const std::string& json_payload) = 0;

    /** Request a resource */
    virtual Result<IResourceManager::ResourceGrant> request_resource(
        IResourceManager::ResourceType type, const std::string& session_id) = 0;

    /** Release a resource */
    virtual Result<void> release_resource(const std::string& token) = 0;
};

// Factory functions for creating core components
std::unique_ptr<ISessionManager> create_session_manager();
std::unique_ptr<ILifecycleManager> create_lifecycle_manager();
std::unique_ptr<IEventRouter> create_event_router();
std::unique_ptr<IConfigurationManager> create_config_manager();
std::unique_ptr<IErrorHandler> create_error_handler();
std::unique_ptr<IProjectionMiddleware> create_projection_middleware();

} // namespace core
} // namespace phone_projection

// Specialize std::error_code for our ErrorCode
namespace std {
template<>
struct is_error_code_enum<phone_projection::core::ErrorCode> : true_type {};
}