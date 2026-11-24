/**
 * @file demo_app.cpp
 * @brief Interactive demo application for Phone Projection Middleware
 */

#include "core/interfaces.hpp"
#include "common/common.hpp"
#include "command_parser.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <string>
#include <optional>
#include <vector>
#include <csignal>

using namespace phone_projection;
using namespace phone_projection::core;
using namespace phone_projection::examples;

// ============================================================================
// Thread-safe output handler for async events
// ============================================================================

class AsyncOutput {
public:
    struct Line {
        std::string text;
        bool is_prompt = false;
    };

    void print(const std::string& text) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(Line{text, false});
        cv_.notify_one();
    }

    void print_prompt() {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(Line{"pp> ", true});
        cv_.notify_one();
    }

    void run() {
        while (running_) {
            Line line;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] { return !queue_.empty() || !running_; });
                if (!running_ && queue_.empty()) break;
                line = queue_.front();
                queue_.pop();
            }
            if (line.is_prompt) {
                std::cout << line.text << std::flush;
            } else {
                // If we're in the middle of a prompt, move to new line first
                std::cout << "\n" << line.text << "\n";
                std::cout << "pp> " << std::flush;
            }
        }
    }

    void stop() {
        running_ = false;
        cv_.notify_one();
    }

private:
    std::queue<Line> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> running_{true};
};

// ============================================================================
// Demo Controller - thin layer between parser and middleware
// ============================================================================

class DemoController {
public:
    DemoController(std::unique_ptr<IProjectionMiddleware> middleware, AsyncOutput& output)
        : middleware_(std::move(middleware)), output_(output), session_connected_(false) {}

    bool initialize() {
        auto result = middleware_->lifecycle_manager()->initialize();
        if (result.is_error()) {
            output_.print("[error] Failed to initialize middleware: " + result.error().message());
            return false;
        }
        setup_event_handlers();
        setup_default_config();
        return true;
    }

    void shutdown() {
        if (session_id_.has_value()) {
            disconnect_session_internal(true);
        }
        middleware_->lifecycle_manager()->shutdown();
    }

    // Command handlers
    ParsedCommand handle_start_session(const ParsedCommand& cmd) {
        if (session_id_.has_value()) {
            return make_error(cmd, "Session already active: " + *session_id_ + ". Use 'disconnect session' first.");
        }

        auto profile = get_active_profile();
        auto session_result = middleware_->create_and_connect_session(profile);
        if (session_result.is_error()) {
            return make_error(cmd, "Failed to create session: " + session_result.error().message());
        }

        session_id_ = session_result.value();
        session_connected_ = false;
        audio_held_ = false;
        video_held_ = false;
        audio_token_.reset();
        video_token_.reset();

        output_.print("[info] Session created: " + *session_id_);
        output_.print("[info] State: Disconnected -> Connecting");

        // Wait for connection to complete
        wait_for_connection();

        if (session_connected_) {
            output_.print("[info] State: Connecting -> Active");
        } else {
            output_.print("[error] Connection failed or timed out");
            session_id_.reset();
        }

        return make_success(cmd);
    }

    ParsedCommand handle_disconnect_session(const ParsedCommand& cmd) {
        if (!session_id_.has_value()) {
            return make_error(cmd, "No active session. Type 'start session' first.");
        }
        return disconnect_session_internal(false);
    }

    ParsedCommand handle_render_video(const ParsedCommand& cmd) {
        if (!session_id_.has_value()) {
            return make_error(cmd, "No active session. Type 'start session' first.");
        }
        if (!session_connected_) {
            return make_error(cmd, "Session not connected.");
        }
        if (video_held_) {
            return make_error(cmd, "Video already rendering. Use 'disconnect video' first.");
        }

        auto result = middleware_->request_resource(IResourceManager::ResourceType::VideoSurface, *session_id_);
        if (result.is_error()) {
            return make_error(cmd, "Failed to request video surface: " + result.error().message());
        }

        video_held_ = true;
        video_token_ = result.value().token;
        output_.print("[event] Video surface granted (token: " + video_token_.value() + ")");

        // Render a video template
        std::string video_template = R"({
            "template": "VideoTemplate",
            "title": "Video Playback",
            "source": "http://example.com/video.mp4",
            "controls": ["play", "pause", "seek"]
        })";
        auto render_result = middleware_->render_template(IUiRenderer::TemplateType::Custom, video_template);
        if (render_result.is_error()) {
            output_.print("[warn] Failed to render video template: " + render_result.error().message());
        } else {
            output_.print("[event] Video template rendered");
        }

        return make_success(cmd);
    }

    ParsedCommand handle_render_audio(const ParsedCommand& cmd) {
        if (!session_id_.has_value()) {
            return make_error(cmd, "No active session. Type 'start session' first.");
        }
        if (!session_connected_) {
            return make_error(cmd, "Session not connected.");
        }
        if (audio_held_) {
            return make_error(cmd, "Audio already rendering. Use 'disconnect audio' first.");
        }

        auto result = middleware_->request_resource(IResourceManager::ResourceType::AudioFocus, *session_id_);
        if (result.is_error()) {
            return make_error(cmd, "Failed to request audio focus: " + result.error().message());
        }

        audio_held_ = true;
        audio_token_ = result.value().token;
        output_.print("[event] Audio focus granted (token: " + audio_token_.value() + ")");

        return make_success(cmd);
    }

    ParsedCommand handle_disconnect_video(const ParsedCommand& cmd) {
        if (!session_id_.has_value()) {
            return make_error(cmd, "No active session. Type 'start session' first.");
        }
        if (!video_held_) {
            return make_error(cmd, "Video not currently rendering.");
        }
        if (!video_token_.has_value()) {
            return make_error(cmd, "No video token available.");
        }

        auto result = middleware_->release_resource(video_token_.value());
        if (result.is_error()) {
            return make_error(cmd, "Failed to release video surface: " + result.error().message());
        }

        video_held_ = false;
        video_token_.reset();
        output_.print("[event] Video surface released");

        return make_success(cmd);
    }

    ParsedCommand handle_disconnect_audio(const ParsedCommand& cmd) {
        if (!session_id_.has_value()) {
            return make_error(cmd, "No active session. Type 'start session' first.");
        }
        if (!audio_held_) {
            return make_error(cmd, "Audio not currently rendering.");
        }
        if (!audio_token_.has_value()) {
            return make_error(cmd, "No audio token available.");
        }

        auto result = middleware_->release_resource(audio_token_.value());
        if (result.is_error()) {
            return make_error(cmd, "Failed to release audio focus: " + result.error().message());
        }

        audio_held_ = false;
        audio_token_.reset();
        output_.print("[event] Audio focus released");

        return make_success(cmd);
    }

    ParsedCommand handle_send_nav(const ParsedCommand& cmd) {
        if (!session_id_.has_value()) {
            return make_error(cmd, "No active session. Type 'start session' first.");
        }
        if (!session_connected_) {
            return make_error(cmd, "Session not connected.");
        }

        std::string nav_template = R"({
            "template": "NavigationTemplate",
            "title": "Navigation",
            "destination": "Home",
            "eta": "15 min",
            "distance": "8.2 km",
            "maneuver": "Turn right in 200m",
            "actions": ["mute", "overview", "cancel"]
        })";
        auto result = middleware_->render_template(IUiRenderer::TemplateType::NavigationTemplate, nav_template);
        if (result.is_error()) {
            return make_error(cmd, "Failed to send navigation: " + result.error().message());
        }
        output_.print("[event] Navigation template sent");

        return make_success(cmd);
    }

    ParsedCommand handle_send_media(const ParsedCommand& cmd) {
        if (!session_id_.has_value()) {
            return make_error(cmd, "No active session. Type 'start session' first.");
        }
        if (!session_connected_) {
            return make_error(cmd, "Session not connected.");
        }

        std::string media_template = R"({
            "template": "MediaTemplate",
            "title": "Now Playing",
            "artist": "Demo Artist",
            "album": "Demo Album",
            "duration": 180,
            "position": 45,
            "actions": ["play", "pause", "next", "previous"]
        })";
        auto result = middleware_->render_template(IUiRenderer::TemplateType::MediaTemplate, media_template);
        if (result.is_error()) {
            return make_error(cmd, "Failed to send media: " + result.error().message());
        }
        output_.print("[event] Media template sent");

        return make_success(cmd);
    }

    ParsedCommand handle_send_input(const ParsedCommand& cmd) {
        if (!session_id_.has_value()) {
            return make_error(cmd, "No active session. Type 'start session' first.");
        }
        if (!session_connected_) {
            return make_error(cmd, "Session not connected.");
        }

        // Simulate touch down
        TouchEvent touch_down(TouchEvent::Action::Down, 0.5f, 0.5f, *session_id_);
        auto result = middleware_->simulate_input_event(*session_id_, touch_down);
        if (result.is_error()) {
            return make_error(cmd, "Failed to send touch down: " + result.error().message());
        }

        // Simulate touch up
        TouchEvent touch_up(TouchEvent::Action::Up, 0.5f, 0.5f, *session_id_);
        result = middleware_->simulate_input_event(*session_id_, touch_up);
        if (result.is_error()) {
            return make_error(cmd, "Failed to send touch up: " + result.error().message());
        }

        // Simulate rotary
        RotaryEvent rotary(RotaryEvent::Action::Clockwise, 3, *session_id_);
        result = middleware_->simulate_input_event(*session_id_, rotary);
        if (result.is_error()) {
            return make_error(cmd, "Failed to send rotary: " + result.error().message());
        }

        // Simulate voice
        VoiceEvent voice("Navigate home", 0.95f, true, *session_id_);
        result = middleware_->simulate_input_event(*session_id_, voice);
        if (result.is_error()) {
            return make_error(cmd, "Failed to send voice: " + result.error().message());
        }

        // Simulate key press (Enter = 66)
        KeyEvent key_press(KeyEvent::Action::Press, 66, *session_id_);
        result = middleware_->simulate_input_event(*session_id_, key_press);
        if (result.is_error()) {
            return make_error(cmd, "Failed to send key press: " + result.error().message());
        }

        KeyEvent key_release(KeyEvent::Action::Release, 66, *session_id_);
        result = middleware_->simulate_input_event(*session_id_, key_release);
        if (result.is_error()) {
            return make_error(cmd, "Failed to send key release: " + result.error().message());
        }

        output_.print("[event] Input events sent (touch, rotary, voice, key)");

        return make_success(cmd);
    }

    ParsedCommand handle_status(const ParsedCommand& cmd) {
        if (!session_id_.has_value()) {
            output_.print("[info] No active session");
            output_.print("[info] Target: " + get_target_name());
            return make_success(cmd);
        }

        auto state_result = middleware_->session_manager()->get_session_state(*session_id_);
        std::string state_str = "Unknown";
        if (state_result.is_ok()) {
            switch (state_result.value()) {
                case SessionState::Disconnected: state_str = "Disconnected"; break;
                case SessionState::Connecting: state_str = "Connecting"; break;
                case SessionState::Connected: state_str = "Active"; break;
                case SessionState::Disconnecting: state_str = "Stopping"; break;
                case SessionState::Error: state_str = "Error"; break;
            }
        }

        output_.print("[info] Session ID: " + *session_id_);
        output_.print("[info] Session State: " + state_str);
        output_.print("[info] Audio Focus: " + std::string(audio_held_ ? "HELD" : "Free"));
        output_.print("[info] Video Surface: " + std::string(video_held_ ? "HELD" : "Free"));
        output_.print("[info] Target: " + get_target_name());

        return make_success(cmd);
    }

    ParsedCommand handle_set_log_level(const ParsedCommand& cmd) {
        if (cmd.args.empty()) {
            return make_error(cmd, "Usage: log level <debug|info|warn|error>");
        }
        std::string level = cmd.args[0];
        common::LogLevel ll = common::LogLevel::Warn;
        if (level == "debug") ll = common::LogLevel::Debug;
        else if (level == "info") ll = common::LogLevel::Info;
        else if (level == "warn") ll = common::LogLevel::Warn;
        else if (level == "error") ll = common::LogLevel::Error;

        common::Logger::instance().set_level(ll);
        output_.print("[info] Log level set to " + level);
        return make_success(cmd);
    }

    ParsedCommand handle_help(const ParsedCommand& cmd) {
        output_.print(get_help_text());
        return make_success(cmd);
    }

    ParsedCommand handle_quit(const ParsedCommand& cmd) {
        should_quit_ = true;
        return make_success(cmd);
    }

    bool should_quit() const { return should_quit_; }

    void set_log_level(common::LogLevel level) {
        common::Logger::instance().set_level(level);
    }

    void set_target(const std::string& target) {
        target_name_ = target;
    }

    std::string get_help_text() const {
        return parser_.get_help_text();
    }

    ParsedCommand parse_command(const std::string& line) {
        return parser_.parse(line);
    }

private:
    std::unique_ptr<IProjectionMiddleware> middleware_;
    AsyncOutput& output_;
    CommandParser parser_;

    std::optional<std::string> session_id_;
    std::optional<std::string> audio_token_;
    std::optional<std::string> video_token_;
    std::atomic<bool> session_connected_{false};
    std::atomic<bool> audio_held_{false};
    std::atomic<bool> video_held_{false};
    std::atomic<bool> should_quit_{false};
    std::string target_name_ = "default";

    // For waiting on connection
    std::mutex conn_mutex_;
    std::condition_variable conn_cv_;
    bool connection_completed_ = false;
    bool connection_failed_ = false;

    void setup_default_config() {
        VehicleProfile profile;
        profile.vehicle_id = "demo_vehicle";
        profile.make = "Generic";
        profile.model = "IVI System";
        profile.year = "2024";
        profile.head_unit_model = "HU-2024";
        profile.head_unit_firmware = "1.0.0";
        profile.preferred_transport = "usb";
        profile.screen_width = 1280;
        profile.screen_height = 720;
        profile.pixel_density = 1.5f;
        profile.capabilities.supports_touch = true;
        profile.capabilities.supports_rotary = true;
        profile.capabilities.supports_keyboard = true;
        profile.capabilities.supports_voice = true;
        profile.capabilities.supports_video = true;
        profile.capabilities.supports_audio = true;
        profile.capabilities.supports_navigation = true;
        profile.capabilities.max_video_width = 1920;
        profile.capabilities.max_video_height = 1080;
        profile.capabilities.supported_codecs = {"H264", "VP8", "VP9"};
        profile.capabilities.supported_templates = {
            "NavigationTemplate", "MediaTemplate", "ListTemplate", "GridTemplate",
            "MessageTemplate", "SignInTemplate", "LongMessageTemplate", "PaneTemplate", "SearchTemplate"
        };

        middleware_->config_manager()->add_profile("demo_vehicle", profile);
        middleware_->config_manager()->set_active_profile("demo_vehicle");
    }

    VehicleProfile get_active_profile() {
        auto result = middleware_->config_manager()->get_active_profile();
        if (result.is_ok()) {
            return result.value();
        }
        VehicleProfile profile;
        profile.vehicle_id = "fallback";
        return profile;
    }

    std::string get_target_name() const {
        return target_name_;
    }

    void setup_event_handlers() {
        auto& router = *middleware_->event_router();

        // Session connected
        router.subscribe(EventType::SessionConnected, [this](const Event& e) {
            if (session_id_.has_value() && e.session_id == *session_id_) {
                session_connected_ = true;
                connection_completed_ = true;
                connection_failed_ = false;
                conn_cv_.notify_one();
                output_.print("[event] Session connected");
            }
        });

        // Session disconnected
        router.subscribe(EventType::SessionDisconnected, [this](const Event& e) {
            if (session_id_.has_value() && e.session_id == *session_id_) {
                session_connected_ = false;
                audio_held_ = false;
                video_held_ = false;
                audio_token_.reset();
                video_token_.reset();
                output_.print("[event] Session disconnected");
            }
        });

        // Session error
        router.subscribe(EventType::SessionError, [this](const Event& e) {
            if (session_id_.has_value() && e.session_id == *session_id_) {
                connection_completed_ = true;
                connection_failed_ = true;
                conn_cv_.notify_one();
                output_.print("[event] Session error");
            }
        });

        // Resource events
        router.subscribe(EventType::AudioFocusGained, [this](const Event& e) {
            if (session_id_.has_value() && e.session_id == *session_id_) {
                output_.print("[event] Audio focus gained");
            }
        });

        router.subscribe(EventType::AudioFocusLost, [this](const Event& e) {
            if (session_id_.has_value() && e.session_id == *session_id_) {
                output_.print("[event] Audio focus lost");
            }
        });

        router.subscribe(EventType::VideoFocusGained, [this](const Event& e) {
            if (session_id_.has_value() && e.session_id == *session_id_) {
                output_.print("[event] Video focus gained");
            }
        });

        router.subscribe(EventType::VideoFocusLost, [this](const Event& e) {
            if (session_id_.has_value() && e.session_id == *session_id_) {
                output_.print("[event] Video focus lost");
            }
        });

        // Input events
        router.subscribe(EventType::TouchDown, [this](const Event& e) {
            if (auto* te = dynamic_cast<const TouchEvent*>(&e)) {
                output_.print("[event] Touch down at (" + std::to_string(te->x) + ", " + std::to_string(te->y) + ")");
            }
        });

        router.subscribe(EventType::TouchUp, [this](const Event& e) {
            if (auto* te = dynamic_cast<const TouchEvent*>(&e)) {
                output_.print("[event] Touch up at (" + std::to_string(te->x) + ", " + std::to_string(te->y) + ")");
            }
        });

        router.subscribe(EventType::RotaryClockwise, [this](const Event& e) {
            if (auto* re = dynamic_cast<const RotaryEvent*>(&e)) {
                output_.print("[event] Rotary clockwise " + std::to_string(re->steps) + " steps");
            }
        });

        router.subscribe(EventType::RotaryCounterClockwise, [this](const Event& e) {
            if (auto* re = dynamic_cast<const RotaryEvent*>(&e)) {
                output_.print("[event] Rotary counter-clockwise " + std::to_string(re->steps) + " steps");
            }
        });

        router.subscribe(EventType::VoiceCommand, [this](const Event& e) {
            if (auto* ve = dynamic_cast<const VoiceEvent*>(&e)) {
                output_.print("[event] Voice: '" + ve->transcript + "' (confidence: " + std::to_string(ve->confidence) + ")");
            }
        });

        router.subscribe(EventType::KeyPress, [this](const Event& e) {
            if (auto* ke = dynamic_cast<const KeyEvent*>(&e)) {
                output_.print("[event] Key press: code=" + std::to_string(ke->key_code));
            }
        });
    }

    void wait_for_connection() {
        std::unique_lock<std::mutex> lock(conn_mutex_);
        connection_completed_ = false;
        connection_failed_ = false;

        // Wait up to 10 seconds for connection
        auto result = conn_cv_.wait_for(lock, std::chrono::seconds(10), [this] {
            return connection_completed_;
        });

        if (!result || connection_failed_) {
            session_connected_ = false;
        }
    }

    ParsedCommand disconnect_session_internal(bool is_shutdown) {
        if (!session_id_.has_value()) {
            return make_error(ParsedCommand{}, "No active session");
        }

        // Release resources first if held
        if (audio_held_ && audio_token_.has_value()) {
            middleware_->release_resource(audio_token_.value());
            audio_held_ = false;
            audio_token_.reset();
            if (!is_shutdown) output_.print("[event] Audio focus released (auto)");
        }
        if (video_held_ && video_token_.has_value()) {
            middleware_->release_resource(video_token_.value());
            video_held_ = false;
            video_token_.reset();
            if (!is_shutdown) output_.print("[event] Video surface released (auto)");
        }

        output_.print("[info] State: Active -> Stopping");
        auto result = middleware_->session_manager()->disconnect(*session_id_);
        if (result.is_error()) {
            output_.print("[warn] Disconnect returned error: " + result.error().message());
        }

        // Wait for disconnect event
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        session_id_.reset();
        session_connected_ = false;

        if (!is_shutdown) {
            output_.print("[info] State: Stopping -> Disconnected");
            output_.print("[info] Session ended");
        }

        return make_success(ParsedCommand{});
    }

    ParsedCommand make_success(const ParsedCommand& cmd) {
        ParsedCommand result = cmd;
        result.error_message.clear();
        return result;
    }

    ParsedCommand make_error(const ParsedCommand& cmd, const std::string& msg) {
        ParsedCommand result = cmd;
        result.error_message = msg;
        return result;
    }
};

// ============================================================================
// Command line argument parsing
// ============================================================================

struct CliOptions {
    common::LogLevel log_level = common::LogLevel::Warn;
    bool quiet = false;
    std::string target = "default";
    bool help = false;
};

CliOptions parse_args(int argc, char* argv[]) {
    CliOptions opts;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            opts.help = true;
        } else if (arg == "--quiet" || arg == "-q") {
            opts.quiet = true;
        } else if (arg == "--log-level" && i + 1 < argc) {
            std::string level = argv[++i];
            if (level == "debug") opts.log_level = common::LogLevel::Debug;
            else if (level == "info") opts.log_level = common::LogLevel::Info;
            else if (level == "warn") opts.log_level = common::LogLevel::Warn;
            else if (level == "error") opts.log_level = common::LogLevel::Error;
            else if (level == "trace") opts.log_level = common::LogLevel::Trace;
            else if (level == "critical") opts.log_level = common::LogLevel::Critical;
            else if (level == "off") opts.log_level = common::LogLevel::Off;
        } else if (arg == "--target" && i + 1 < argc) {
            opts.target = argv[++i];
        }
    }
    return opts;
}

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  --log-level <level>   Set log level (trace|debug|info|warn|error|critical|off)\n"
              << "  --quiet, -q           Suppress all log output (sets log level to off)\n"
              << "  --target <name>       Select target configuration\n"
              << "  --help, -h            Show this help\n";
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    auto opts = parse_args(argc, argv);

    if (opts.help) {
        print_usage(argv[0]);
        return 0;
    }

    if (opts.quiet) {
        opts.log_level = common::LogLevel::Off;
    }

    common::Logger::instance().set_level(opts.log_level);

    // Create middleware and controller
    auto middleware = create_projection_middleware();
    AsyncOutput async_output;
    DemoController controller(std::move(middleware), async_output);
    controller.set_log_level(opts.log_level);
    controller.set_target(opts.target);

    // Start output thread
    std::thread output_thread([&async_output]() { async_output.run(); });

    // Initialize
    if (!controller.initialize()) {
        async_output.stop();
        output_thread.join();
        return 1;
    }

    // Print initial prompt
    async_output.print_prompt();

    // Main input loop
    std::string line;
    while (std::getline(std::cin, line)) {
        // Handle EOF
        if (std::cin.eof()) {
            break;
        }

        // Parse and execute command
        auto parsed = controller.parse_command(line);

        if (parsed.type == CommandType::Unknown) {
            if (!parsed.error_message.empty()) {
                async_output.print(parsed.error_message);
            }
        } else if (!parsed.is_valid()) {
            async_output.print(parsed.error_message);
        } else {
            ParsedCommand result;
            switch (parsed.type) {
                case CommandType::Help:
                    result = controller.handle_help(parsed);
                    break;
                case CommandType::Quit:
                    result = controller.handle_quit(parsed);
                    break;
                case CommandType::StartSession:
                    result = controller.handle_start_session(parsed);
                    break;
                case CommandType::DisconnectSession:
                    result = controller.handle_disconnect_session(parsed);
                    break;
                case CommandType::RenderVideo:
                    result = controller.handle_render_video(parsed);
                    break;
                case CommandType::RenderAudio:
                    result = controller.handle_render_audio(parsed);
                    break;
                case CommandType::DisconnectVideo:
                    result = controller.handle_disconnect_video(parsed);
                    break;
                case CommandType::DisconnectAudio:
                    result = controller.handle_disconnect_audio(parsed);
                    break;
                case CommandType::SendNavigation:
                    result = controller.handle_send_nav(parsed);
                    break;
                case CommandType::SendMedia:
                    result = controller.handle_send_media(parsed);
                    break;
                case CommandType::SendInput:
                    result = controller.handle_send_input(parsed);
                    break;
                case CommandType::Status:
                    result = controller.handle_status(parsed);
                    break;
                case CommandType::SetLogLevel:
                    result = controller.handle_set_log_level(parsed);
                    break;
                default:
                    result = parsed;
                    result.error_message = "Unhandled command";
                    break;
            }

            if (!result.error_message.empty()) {
                async_output.print(result.error_message);
            }
        }

        if (controller.should_quit()) {
            break;
        }

        // Prompt is printed by async_output after each command output
    }

    // Handle EOF (Ctrl+D/Ctrl+Z) - same as quit
    if (std::cin.eof() && !controller.should_quit()) {
        controller.handle_quit(ParsedCommand{});
    }

    // Shutdown
    controller.shutdown();
    async_output.stop();
    output_thread.join();

    return 0;
}