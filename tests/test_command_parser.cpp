/**
 * @file test_command_parser.cpp
 * @brief Unit tests for the command parser
 */

#include <gtest/gtest.h>
#include "../examples/command_parser.hpp"

using namespace phone_projection::examples;

class CommandParserTest : public ::testing::Test {
protected:
    CommandParser parser;
};

TEST_F(CommandParserTest, ParseHelp) {
    auto cmd = parser.parse("help");
    EXPECT_EQ(cmd.type, CommandType::Help);
    EXPECT_TRUE(cmd.is_valid());
    EXPECT_TRUE(cmd.error_message.empty());
}

TEST_F(CommandParserTest, ParseHelpAlias) {
    auto cmd = parser.parse("h");
    EXPECT_EQ(cmd.type, CommandType::Help);
    EXPECT_TRUE(cmd.is_valid());

    cmd = parser.parse("?");
    EXPECT_EQ(cmd.type, CommandType::Help);
    EXPECT_TRUE(cmd.is_valid());
}

TEST_F(CommandParserTest, ParseQuit) {
    auto cmd = parser.parse("quit");
    EXPECT_EQ(cmd.type, CommandType::Quit);
    EXPECT_TRUE(cmd.is_valid());

    cmd = parser.parse("exit");
    EXPECT_EQ(cmd.type, CommandType::Quit);
    EXPECT_TRUE(cmd.is_valid());

    cmd = parser.parse("q");
    EXPECT_EQ(cmd.type, CommandType::Quit);
    EXPECT_TRUE(cmd.is_valid());
}

TEST_F(CommandParserTest, ParseStartSession) {
    auto cmd = parser.parse("start session");
    EXPECT_EQ(cmd.type, CommandType::StartSession);
    EXPECT_TRUE(cmd.is_valid());

    cmd = parser.parse("start");
    EXPECT_EQ(cmd.type, CommandType::StartSession);
    EXPECT_TRUE(cmd.is_valid());

    cmd = parser.parse("connect");
    EXPECT_EQ(cmd.type, CommandType::StartSession);
    EXPECT_TRUE(cmd.is_valid());
}

TEST_F(CommandParserTest, ParseStartSessionWithArgsFails) {
    auto cmd = parser.parse("start session foo");
    EXPECT_EQ(cmd.type, CommandType::StartSession);
    EXPECT_FALSE(cmd.is_valid());
    EXPECT_FALSE(cmd.error_message.empty());
}

TEST_F(CommandParserTest, ParseDisconnectSession) {
    auto cmd = parser.parse("disconnect session");
    EXPECT_EQ(cmd.type, CommandType::DisconnectSession);
    EXPECT_TRUE(cmd.is_valid());

    cmd = parser.parse("stop");
    EXPECT_EQ(cmd.type, CommandType::DisconnectSession);
    EXPECT_TRUE(cmd.is_valid());

    cmd = parser.parse("disconnect");
    EXPECT_EQ(cmd.type, CommandType::DisconnectSession);
    EXPECT_TRUE(cmd.is_valid());
}

TEST_F(CommandParserTest, ParseRenderVideo) {
    auto cmd = parser.parse("render video");
    EXPECT_EQ(cmd.type, CommandType::RenderVideo);
    EXPECT_TRUE(cmd.is_valid());

    cmd = parser.parse("video on");
    EXPECT_EQ(cmd.type, CommandType::RenderVideo);
    EXPECT_TRUE(cmd.is_valid());

    cmd = parser.parse("video");
    EXPECT_EQ(cmd.type, CommandType::RenderVideo);
    EXPECT_TRUE(cmd.is_valid());
}

TEST_F(CommandParserTest, ParseRenderAudio) {
    auto cmd = parser.parse("render audio");
    EXPECT_EQ(cmd.type, CommandType::RenderAudio);
    EXPECT_TRUE(cmd.is_valid());

    cmd = parser.parse("audio on");
    EXPECT_EQ(cmd.type, CommandType::RenderAudio);
    EXPECT_TRUE(cmd.is_valid());

    cmd = parser.parse("audio");
    EXPECT_EQ(cmd.type, CommandType::RenderAudio);
    EXPECT_TRUE(cmd.is_valid());
}

TEST_F(CommandParserTest, ParseDisconnectVideo) {
    auto cmd = parser.parse("disconnect video");
    EXPECT_EQ(cmd.type, CommandType::DisconnectVideo);
    EXPECT_TRUE(cmd.is_valid());

    cmd = parser.parse("video off");
    EXPECT_EQ(cmd.type, CommandType::DisconnectVideo);
    EXPECT_TRUE(cmd.is_valid());

    cmd = parser.parse("novideo");
    EXPECT_EQ(cmd.type, CommandType::DisconnectVideo);
    EXPECT_TRUE(cmd.is_valid());
}

TEST_F(CommandParserTest, ParseDisconnectAudio) {
    auto cmd = parser.parse("disconnect audio");
    EXPECT_EQ(cmd.type, CommandType::DisconnectAudio);
    EXPECT_TRUE(cmd.is_valid());

    cmd = parser.parse("audio off");
    EXPECT_EQ(cmd.type, CommandType::DisconnectAudio);
    EXPECT_TRUE(cmd.is_valid());

    cmd = parser.parse("noaudio");
    EXPECT_EQ(cmd.type, CommandType::DisconnectAudio);
    EXPECT_TRUE(cmd.is_valid());
}

TEST_F(CommandParserTest, ParseSendNavigation) {
    auto cmd = parser.parse("send nav");
    EXPECT_EQ(cmd.type, CommandType::SendNavigation);
    EXPECT_TRUE(cmd.is_valid());

    cmd = parser.parse("send navigation");
    EXPECT_EQ(cmd.type, CommandType::SendNavigation);
    EXPECT_TRUE(cmd.is_valid());

    cmd = parser.parse("nav");
    EXPECT_EQ(cmd.type, CommandType::SendNavigation);
    EXPECT_TRUE(cmd.is_valid());
}

TEST_F(CommandParserTest, ParseSendMedia) {
    auto cmd = parser.parse("send media");
    EXPECT_EQ(cmd.type, CommandType::SendMedia);
    EXPECT_TRUE(cmd.is_valid());
}

TEST_F(CommandParserTest, ParseSendInput) {
    auto cmd = parser.parse("send input");
    EXPECT_EQ(cmd.type, CommandType::SendInput);
    EXPECT_TRUE(cmd.is_valid());

    cmd = parser.parse("input");
    EXPECT_EQ(cmd.type, CommandType::SendInput);
    EXPECT_TRUE(cmd.is_valid());
}

TEST_F(CommandParserTest, ParseStatus) {
    auto cmd = parser.parse("status");
    EXPECT_EQ(cmd.type, CommandType::Status);
    EXPECT_TRUE(cmd.is_valid());

    cmd = parser.parse("st");
    EXPECT_EQ(cmd.type, CommandType::Status);
    EXPECT_TRUE(cmd.is_valid());

    cmd = parser.parse("info");
    EXPECT_EQ(cmd.type, CommandType::Status);
    EXPECT_TRUE(cmd.is_valid());
}

TEST_F(CommandParserTest, ParseLogLevel) {
    auto cmd = parser.parse("log level debug");
    EXPECT_EQ(cmd.type, CommandType::SetLogLevel);
    EXPECT_TRUE(cmd.is_valid());
    EXPECT_EQ(cmd.args.size(), 1);
    EXPECT_EQ(cmd.args[0], "debug");

    cmd = parser.parse("log level info");
    EXPECT_EQ(cmd.type, CommandType::SetLogLevel);
    EXPECT_TRUE(cmd.is_valid());
    EXPECT_EQ(cmd.args[0], "info");

    cmd = parser.parse("log level warn");
    EXPECT_EQ(cmd.type, CommandType::SetLogLevel);
    EXPECT_TRUE(cmd.is_valid());
    EXPECT_EQ(cmd.args[0], "warn");

    cmd = parser.parse("log level error");
    EXPECT_EQ(cmd.type, CommandType::SetLogLevel);
    EXPECT_TRUE(cmd.is_valid());
    EXPECT_EQ(cmd.args[0], "error");
}

TEST_F(CommandParserTest, ParseLogLevelInvalid) {
    auto cmd = parser.parse("log level invalid");
    EXPECT_EQ(cmd.type, CommandType::SetLogLevel);
    EXPECT_FALSE(cmd.is_valid());
    EXPECT_FALSE(cmd.error_message.empty());

    cmd = parser.parse("log level");
    EXPECT_EQ(cmd.type, CommandType::SetLogLevel);
    EXPECT_FALSE(cmd.is_valid());
    EXPECT_FALSE(cmd.error_message.empty());

    cmd = parser.parse("log level debug info");
    EXPECT_EQ(cmd.type, CommandType::SetLogLevel);
    EXPECT_FALSE(cmd.is_valid());
    EXPECT_FALSE(cmd.error_message.empty());
}

TEST_F(CommandParserTest, CaseInsensitive) {
    auto cmd = parser.parse("HELP");
    EXPECT_EQ(cmd.type, CommandType::Help);
    EXPECT_TRUE(cmd.is_valid());

    cmd = parser.parse("Start Session");
    EXPECT_EQ(cmd.type, CommandType::StartSession);
    EXPECT_TRUE(cmd.is_valid());

    cmd = parser.parse("ReNdEr ViDeO");
    EXPECT_EQ(cmd.type, CommandType::RenderVideo);
    EXPECT_TRUE(cmd.is_valid());

    cmd = parser.parse("LoG LeVeL DeBuG");
    EXPECT_EQ(cmd.type, CommandType::SetLogLevel);
    EXPECT_TRUE(cmd.is_valid());
    EXPECT_EQ(cmd.args[0], "debug");
}

TEST_F(CommandParserTest, ExtraWhitespace) {
    auto cmd = parser.parse("  help  ");
    EXPECT_EQ(cmd.type, CommandType::Help);
    EXPECT_TRUE(cmd.is_valid());

    cmd = parser.parse("start   session");
    EXPECT_EQ(cmd.type, CommandType::StartSession);
    EXPECT_TRUE(cmd.is_valid());

    cmd = parser.parse("\trender\tvideo\t");
    EXPECT_EQ(cmd.type, CommandType::RenderVideo);
    EXPECT_TRUE(cmd.is_valid());
}

TEST_F(CommandParserTest, EmptyInput) {
    auto cmd = parser.parse("");
    EXPECT_EQ(cmd.type, CommandType::Unknown);
    EXPECT_TRUE(cmd.error_message.empty());

    cmd = parser.parse("   ");
    EXPECT_EQ(cmd.type, CommandType::Unknown);
    EXPECT_TRUE(cmd.error_message.empty());

    cmd = parser.parse("\t\n");
    EXPECT_EQ(cmd.type, CommandType::Unknown);
    EXPECT_TRUE(cmd.error_message.empty());
}

TEST_F(CommandParserTest, UnknownCommand) {
    auto cmd = parser.parse("foobar");
    EXPECT_EQ(cmd.type, CommandType::Unknown);
    EXPECT_FALSE(cmd.is_valid());
    EXPECT_FALSE(cmd.error_message.empty());
    EXPECT_NE(cmd.error_message.find("foobar"), std::string::npos);
}

TEST_F(CommandParserTest, RequiresSession) {
    EXPECT_FALSE(CommandParser::requires_session(CommandType::Help));
    EXPECT_FALSE(CommandParser::requires_session(CommandType::Quit));
    EXPECT_FALSE(CommandParser::requires_session(CommandType::StartSession));
    EXPECT_FALSE(CommandParser::requires_session(CommandType::SetLogLevel));

    EXPECT_TRUE(CommandParser::requires_session(CommandType::DisconnectSession));
    EXPECT_TRUE(CommandParser::requires_session(CommandType::RenderVideo));
    EXPECT_TRUE(CommandParser::requires_session(CommandType::RenderAudio));
    EXPECT_TRUE(CommandParser::requires_session(CommandType::DisconnectVideo));
    EXPECT_TRUE(CommandParser::requires_session(CommandType::DisconnectAudio));
    EXPECT_TRUE(CommandParser::requires_session(CommandType::SendNavigation));
    EXPECT_TRUE(CommandParser::requires_session(CommandType::SendMedia));
    EXPECT_TRUE(CommandParser::requires_session(CommandType::SendInput));
    EXPECT_TRUE(CommandParser::requires_session(CommandType::Status));
}

TEST_F(CommandParserTest, ModifiesSession) {
    EXPECT_FALSE(CommandParser::modifies_session(CommandType::Help));
    EXPECT_FALSE(CommandParser::modifies_session(CommandType::Status));
    EXPECT_FALSE(CommandParser::modifies_session(CommandType::SendNavigation));
    EXPECT_FALSE(CommandParser::modifies_session(CommandType::SendMedia));
    EXPECT_FALSE(CommandParser::modifies_session(CommandType::SendInput));
    EXPECT_FALSE(CommandParser::modifies_session(CommandType::SetLogLevel));

    EXPECT_TRUE(CommandParser::modifies_session(CommandType::Quit));
    EXPECT_TRUE(CommandParser::modifies_session(CommandType::StartSession));
    EXPECT_TRUE(CommandParser::modifies_session(CommandType::DisconnectSession));
    EXPECT_TRUE(CommandParser::modifies_session(CommandType::RenderVideo));
    EXPECT_TRUE(CommandParser::modifies_session(CommandType::RenderAudio));
    EXPECT_TRUE(CommandParser::modifies_session(CommandType::DisconnectVideo));
    EXPECT_TRUE(CommandParser::modifies_session(CommandType::DisconnectAudio));
}

TEST_F(CommandParserTest, GetHelpText) {
    std::string help = parser.get_help_text();
    EXPECT_FALSE(help.empty());
    EXPECT_NE(help.find("help"), std::string::npos);
    EXPECT_NE(help.find("start session"), std::string::npos);
    EXPECT_NE(help.find("render video"), std::string::npos);
    EXPECT_NE(help.find("render audio"), std::string::npos);
    EXPECT_NE(help.find("disconnect session"), std::string::npos);
    EXPECT_NE(help.find("send nav"), std::string::npos);
    EXPECT_NE(help.find("send media"), std::string::npos);
    EXPECT_NE(help.find("send input"), std::string::npos);
    EXPECT_NE(help.find("status"), std::string::npos);
    EXPECT_NE(help.find("log level"), std::string::npos);
    EXPECT_NE(help.find("quit"), std::string::npos);
}

TEST_F(CommandParserTest, GetCommandHelp) {
    std::string help = parser.get_command_help(CommandType::Help);
    EXPECT_FALSE(help.empty());
    EXPECT_NE(help.find("help"), std::string::npos);

    help = parser.get_command_help(CommandType::StartSession);
    EXPECT_FALSE(help.empty());
    EXPECT_NE(help.find("start session"), std::string::npos);
}