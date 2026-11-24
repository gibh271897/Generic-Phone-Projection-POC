/**
 * @file command_parser.cpp
 * @brief Command parser implementation for interactive console demo application
 */

#include "command_parser.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace phone_projection {
namespace examples {

namespace {

std::string to_lower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

} // anonymous namespace

CommandParser::CommandParser() {
    register_commands();
}

std::vector<std::string> CommandParser::tokenize(const std::string& input) const {
    std::vector<std::string> tokens;
    std::istringstream iss(trim(input));
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

std::optional<CommandParser::CommandInfo*> CommandParser::find_command(const std::string& name) {
    auto it = commands_.find(to_lower(name));
    if (it != commands_.end()) {
        return &it->second;
    }
    return std::nullopt;
}

ParsedCommand CommandParser::make_error(CommandType type, const std::string& error, const std::string& raw) const {
    ParsedCommand cmd;
    cmd.type = type;
    cmd.raw_input = raw;
    cmd.error_message = error;
    return cmd;
}

ParsedCommand CommandParser::make_unknown(const std::string& raw) const {
    ParsedCommand cmd;
    cmd.type = CommandType::Unknown;
    cmd.raw_input = raw;
    cmd.error_message = "Unknown command '" + raw + "'. Type 'help'.";
    return cmd;
}

void CommandParser::register_commands() {
    auto add = [this](CommandType type, const std::string& name, const std::vector<std::string>& aliases,
                      const std::string& desc, const std::string& usage, const std::string& detail,
                      bool req_session, bool mod_session,
                      std::function<ParsedCommand(const std::vector<std::string>&)> validator) {
        CommandInfo info;
        info.type = type;
        info.name = name;
        info.aliases = aliases;
        info.description = desc;
        info.usage = usage;
        info.help_detail = detail;
        info.requires_session = req_session;
        info.modifies_session = mod_session;
        info.validator = std::move(validator);

        std::string key = to_lower(name);
        commands_[key] = info;
        for (const auto& alias : aliases) {
            commands_[to_lower(alias)] = info;
        }
        command_list_.push_back(info);
    };

    add(CommandType::Help, "help", {"h", "?"},
        "Show available commands",
        "help",
        "Display all available commands with descriptions.",
        false, false,
        [](const std::vector<std::string>& args) -> ParsedCommand {
            ParsedCommand cmd;
            cmd.type = CommandType::Help;
            cmd.args = args;
            return cmd;
        });

    add(CommandType::Quit, "quit", {"exit", "q"},
        "Quit the application (disconnects session if active)",
        "quit",
        "If a session is active, it will be disconnected cleanly before exiting.",
        false, true,
        [](const std::vector<std::string>& args) -> ParsedCommand {
            ParsedCommand cmd;
            cmd.type = CommandType::Quit;
            cmd.args = args;
            return cmd;
        });

    add(CommandType::StartSession, "start session", {"start", "connect"},
        "Create and connect a projection session",
        "start session",
        "Creates a new session and connects it. Prints session ID and state transitions.",
        false, true,
        [](const std::vector<std::string>& args) -> ParsedCommand {
            ParsedCommand cmd;
            cmd.type = CommandType::StartSession;
            cmd.args = args;
            if (!args.empty()) {
                return ParsedCommand{CommandType::StartSession, args, "", "Usage: start session (no arguments)"};
            }
            return cmd;
        });

    add(CommandType::DisconnectSession, "disconnect session", {"stop", "disconnect"},
        "Disconnect the active session",
        "disconnect session",
        "Stops the session (Stopping -> Disconnected). Releases audio/video if still held.",
        true, true,
        [](const std::vector<std::string>& args) -> ParsedCommand {
            ParsedCommand cmd;
            cmd.type = CommandType::DisconnectSession;
            cmd.args = args;
            if (!args.empty()) {
                return ParsedCommand{CommandType::DisconnectSession, args, "", "Usage: disconnect session (no arguments)"};
            }
            return cmd;
        });

    add(CommandType::RenderVideo, "render video", {"video on", "video"},
        "Request video surface and render video",
        "render video",
        "Requests video surface resource for the active session.",
        true, true,
        [](const std::vector<std::string>& args) -> ParsedCommand {
            ParsedCommand cmd;
            cmd.type = CommandType::RenderVideo;
            cmd.args = args;
            if (!args.empty()) {
                return ParsedCommand{CommandType::RenderVideo, args, "", "Usage: render video (no arguments)"};
            }
            return cmd;
        });

    add(CommandType::RenderAudio, "render audio", {"audio on", "audio"},
        "Request audio focus and render audio",
        "render audio",
        "Requests audio focus resource for the active session.",
        true, true,
        [](const std::vector<std::string>& args) -> ParsedCommand {
            ParsedCommand cmd;
            cmd.type = CommandType::RenderAudio;
            cmd.args = args;
            if (!args.empty()) {
                return ParsedCommand{CommandType::RenderAudio, args, "", "Usage: render audio (no arguments)"};
            }
            return cmd;
        });

    add(CommandType::DisconnectVideo, "disconnect video", {"video off", "novideo"},
        "Release video surface",
        "disconnect video",
        "Releases the video surface resource if currently held.",
        true, true,
        [](const std::vector<std::string>& args) -> ParsedCommand {
            ParsedCommand cmd;
            cmd.type = CommandType::DisconnectVideo;
            cmd.args = args;
            if (!args.empty()) {
                return ParsedCommand{CommandType::DisconnectVideo, args, "", "Usage: disconnect video (no arguments)"};
            }
            return cmd;
        });

    add(CommandType::DisconnectAudio, "disconnect audio", {"audio off", "noaudio"},
        "Release audio focus",
        "disconnect audio",
        "Releases the audio focus resource if currently held.",
        true, true,
        [](const std::vector<std::string>& args) -> ParsedCommand {
            ParsedCommand cmd;
            cmd.type = CommandType::DisconnectAudio;
            cmd.args = args;
            if (!args.empty()) {
                return ParsedCommand{CommandType::DisconnectAudio, args, "", "Usage: disconnect audio (no arguments)"};
            }
            return cmd;
        });

    add(CommandType::SendNavigation, "send nav", {"send navigation", "nav"},
        "Send simulated navigation event",
        "send nav",
        "Injects a simulated navigation template render event.",
        true, false,
        [](const std::vector<std::string>& args) -> ParsedCommand {
            ParsedCommand cmd;
            cmd.type = CommandType::SendNavigation;
            cmd.args = args;
            if (!args.empty()) {
                return ParsedCommand{CommandType::SendNavigation, args, "", "Usage: send nav (no arguments)"};
            }
            return cmd;
        });

    add(CommandType::SendMedia, "send media", {},
        "Send simulated media event",
        "send media",
        "Injects a simulated media template render event.",
        true, false,
        [](const std::vector<std::string>& args) -> ParsedCommand {
            ParsedCommand cmd;
            cmd.type = CommandType::SendMedia;
            cmd.args = args;
            if (!args.empty()) {
                return ParsedCommand{CommandType::SendMedia, args, "", "Usage: send media (no arguments)"};
            }
            return cmd;
        });

    add(CommandType::SendInput, "send input", {"input"},
        "Send simulated input event (touch/rotary/key/voice)",
        "send input",
        "Injects a simulated input event sequence (touch, rotary, voice, key).",
        true, false,
        [](const std::vector<std::string>& args) -> ParsedCommand {
            ParsedCommand cmd;
            cmd.type = CommandType::SendInput;
            cmd.args = args;
            if (!args.empty()) {
                return ParsedCommand{CommandType::SendInput, args, "", "Usage: send input (no arguments)"};
            }
            return cmd;
        });

    add(CommandType::Status, "status", {"st", "info"},
        "Show current session status",
        "status",
        "Prints session state, session ID, audio/video held status, and target configuration.",
        true, false,
        [](const std::vector<std::string>& args) -> ParsedCommand {
            ParsedCommand cmd;
            cmd.type = CommandType::Status;
            cmd.args = args;
            if (!args.empty()) {
                return ParsedCommand{CommandType::Status, args, "", "Usage: status (no arguments)"};
            }
            return cmd;
        });

    add(CommandType::SetLogLevel, "log level", {"loglevel"},
        "Set log level (debug|info|warn|error)",
        "log level <level>",
        "Changes the logging verbosity. Levels: debug, info, warn, error.",
        false, false,
        [this](const std::vector<std::string>& args) -> ParsedCommand {
            ParsedCommand cmd;
            cmd.type = CommandType::SetLogLevel;
            if (args.size() != 1) {
                return ParsedCommand{CommandType::SetLogLevel, args, "", "Usage: log level <debug|info|warn|error>"};
            }
            std::string level = to_lower(args[0]);
            if (level != "debug" && level != "info" && level != "warn" && level != "error") {
                return ParsedCommand{CommandType::SetLogLevel, args, "", "Invalid log level. Use: debug, info, warn, error"};
            }
            cmd.args = {level};  // Store lowercase version
            return cmd;
        });
}

ParsedCommand CommandParser::parse(const std::string& input) {
    std::string trimmed = trim(input);
    if (trimmed.empty()) {
        ParsedCommand cmd;
        cmd.type = CommandType::Unknown;
        cmd.raw_input = input;
        cmd.error_message = "";
        return cmd;
    }

    auto tokens = tokenize(trimmed);
    if (tokens.empty()) {
        return make_unknown(trimmed);
    }

    std::string cmd_name = to_lower(tokens[0]);
    std::vector<std::string> args(tokens.begin() + 1, tokens.end());

    // Handle multi-word commands
    if (tokens.size() >= 2) {
        std::string two_word = to_lower(tokens[0] + " " + tokens[1]);
        if (auto info = find_command(two_word)) {
            std::vector<std::string> remaining(tokens.begin() + 2, tokens.end());
            if ((*info)->validator) {
                return (*info)->validator(remaining);
            }
            ParsedCommand cmd;
            cmd.type = (*info)->type;
            cmd.args = remaining;
            cmd.raw_input = input;
            return cmd;
        }
    }

    if (auto info = find_command(cmd_name)) {
        if ((*info)->validator) {
            return (*info)->validator(args);
        }
        ParsedCommand cmd;
        cmd.type = (*info)->type;
        cmd.args = args;
        cmd.raw_input = input;
        return cmd;
    }

    return make_unknown(tokens[0]);
}

std::string CommandParser::get_help_text() const {
    std::ostringstream oss;
    oss << "Available commands:\n";
    for (const auto& info : command_list_) {
        oss << "  " << info.name;
        if (!info.aliases.empty()) {
            oss << " (" << info.aliases[0];
            for (size_t i = 1; i < info.aliases.size(); ++i) {
                oss << ", " << info.aliases[i];
            }
            oss << ")";
        }
        oss << "\n    " << info.description << "\n";
    }
    return oss.str();
}

std::string CommandParser::get_command_help(CommandType type) const {
    for (const auto& info : command_list_) {
        if (info.type == type) {
            std::ostringstream oss;
            oss << info.name << " - " << info.description << "\n";
            oss << "Usage: " << info.usage << "\n";
            oss << info.help_detail;
            return oss.str();
        }
    }
    return "No help available for this command.";
}

// Static lookup tables for requires_session and modifies_session
namespace {
    struct SessionInfo {
        CommandType type;
        bool requires_session;
        bool modifies_session;
    };

    const SessionInfo session_info_table[] = {
        {CommandType::Help, false, false},
        {CommandType::Quit, false, true},
        {CommandType::StartSession, false, true},
        {CommandType::DisconnectSession, true, true},
        {CommandType::RenderVideo, true, true},
        {CommandType::RenderAudio, true, true},
        {CommandType::DisconnectVideo, true, true},
        {CommandType::DisconnectAudio, true, true},
        {CommandType::SendNavigation, true, false},
        {CommandType::SendMedia, true, false},
        {CommandType::SendInput, true, false},
        {CommandType::Status, true, false},
        {CommandType::SetLogLevel, false, false},
        {CommandType::Unknown, false, false},
    };
}

bool CommandParser::requires_session(CommandType type) {
    for (const auto& info : session_info_table) {
        if (info.type == type) return info.requires_session;
    }
    return false;
}

bool CommandParser::modifies_session(CommandType type) {
    for (const auto& info : session_info_table) {
        if (info.type == type) return info.modifies_session;
    }
    return false;
}

} // namespace examples
} // namespace phone_projection