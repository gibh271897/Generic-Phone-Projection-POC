/**
 * @file command_parser.hpp
 * @brief Command parser for interactive console demo application
 */

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <variant>
#include <functional>
#include <unordered_map>

namespace phone_projection {
namespace examples {

// ============================================================================
// Command Types
// ============================================================================

enum class CommandType {
    Help,
    Quit,
    StartSession,
    DisconnectSession,
    DisconnectAudio,
    DisconnectVideo,
    RenderVideo,
    RenderAudio,
    SendNavigation,
    SendMedia,
    SendInput,
    Status,
    SetLogLevel,
    Unknown
};

struct ParsedCommand {
    CommandType type = CommandType::Unknown;
    std::vector<std::string> args;
    std::string raw_input;
    std::string error_message;

    bool is_valid() const { return type != CommandType::Unknown && error_message.empty(); }
};

// ============================================================================
// Command Parser
// ============================================================================

class CommandParser {
public:
    CommandParser();

    /** Parse a command line string into a ParsedCommand */
    ParsedCommand parse(const std::string& input);

    /** Get help text for all commands */
    std::string get_help_text() const;

    /** Get short help for a specific command */
    std::string get_command_help(CommandType type) const;

    /** Check if a command requires an active session */
    static bool requires_session(CommandType type);

    /** Check if a command modifies session state */
    static bool modifies_session(CommandType type);

private:
    struct CommandInfo {
        CommandType type;
        std::string name;
        std::vector<std::string> aliases;
        std::string description;
        std::string usage;
        std::string help_detail;
        bool requires_session = false;
        bool modifies_session = false;
        std::function<ParsedCommand(const std::vector<std::string>&)> validator;
    };

    std::unordered_map<std::string, CommandInfo> commands_;
    std::vector<CommandInfo> command_list_;

    void register_commands();
    ParsedCommand make_error(CommandType type, const std::string& error, const std::string& raw) const;
    ParsedCommand make_unknown(const std::string& raw) const;
    std::vector<std::string> tokenize(const std::string& input) const;
    std::optional<CommandInfo*> find_command(const std::string& name);
};

} // namespace examples
} // namespace phone_projection