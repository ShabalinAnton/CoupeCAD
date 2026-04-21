#include "coupecad/logging/logger.h"

#include <gtest/gtest.h>

#include <filesystem>

using coupecad::logging::Logger;

class LoggerSinksTest : public ::testing::Test {
protected:
    void SetUp() override { Logger::reset_for_test(); }
    void TearDown() override { Logger::reset_for_test(); }
};

TEST_F(LoggerSinksTest, ConsoleAndFileEnabledByDefault) {
    EXPECT_TRUE(Logger::instance().console_enabled());
    EXPECT_TRUE(Logger::instance().file_enabled());
}

TEST_F(LoggerSinksTest, DisableConsole) {
    Logger::instance().enable_console(false);
    EXPECT_FALSE(Logger::instance().console_enabled());
    EXPECT_TRUE(Logger::instance().file_enabled());
}

TEST_F(LoggerSinksTest, DisableFile) {
    Logger::instance().enable_file(false);
    EXPECT_TRUE(Logger::instance().console_enabled());
    EXPECT_FALSE(Logger::instance().file_enabled());
}

TEST_F(LoggerSinksTest, SetCustomLogFilePath) {
    auto custom = std::filesystem::temp_directory_path() / "coupecad-test.log";
    Logger::instance().set_log_file_path(custom);
    EXPECT_EQ(Logger::instance().log_file_path(), custom);
}
