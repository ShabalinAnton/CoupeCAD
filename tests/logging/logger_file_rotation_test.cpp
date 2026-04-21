#include "coupecad/logging/logger.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

using coupecad::logging::Level;
using coupecad::logging::Logger;

class LoggerFileTest : public ::testing::Test {
protected:
    void SetUp() override {
        Logger::reset_for_test();
        tmp_dir_ = std::filesystem::temp_directory_path() /
                   "coupecad_logger_file_test";
        std::filesystem::remove_all(tmp_dir_);
        std::filesystem::create_directories(tmp_dir_);
    }
    void TearDown() override {
        Logger::reset_for_test();
        std::filesystem::remove_all(tmp_dir_);
    }

    std::filesystem::path tmp_dir_;
};

TEST_F(LoggerFileTest, WritesMessageToFile) {
    auto log_file = tmp_dir_ / "out.log";
    Logger::instance().enable_console(false);
    Logger::instance().set_log_file_path(log_file);
    Logger::instance().info("test", "hello {}", 42);

    // spdlog rotating sink буферизует — заставляем сбросить через
    // выключение/включение файла (rebuild пересоздаёт logger и сбрасывает).
    Logger::instance().enable_file(false);
    Logger::instance().enable_file(true);

    ASSERT_TRUE(std::filesystem::exists(log_file));
    std::ifstream in(log_file);
    std::string contents((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    EXPECT_NE(contents.find("hello 42"), std::string::npos)
        << "File contents: " << contents;
    EXPECT_NE(contents.find("info"), std::string::npos);
    EXPECT_NE(contents.find("test"), std::string::npos);
}

TEST_F(LoggerFileTest, RespectsMinLevel) {
    auto log_file = tmp_dir_ / "out.log";
    Logger::instance().enable_console(false);
    Logger::instance().set_log_file_path(log_file);
    Logger::instance().set_min_level(Level::Warning);

    Logger::instance().info("test", "should be filtered");
    Logger::instance().warn("test", "should be present");

    Logger::instance().enable_file(false);
    Logger::instance().enable_file(true);

    std::ifstream in(log_file);
    std::string contents((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    EXPECT_EQ(contents.find("should be filtered"), std::string::npos);
    EXPECT_NE(contents.find("should be present"), std::string::npos);
}

TEST_F(LoggerFileTest, UnwritableDirectoryDoesNotCrash) {
    // Put the log file inside a "directory" that is actually a regular file
    // — create_directories will fail. Ensure Logger survives and console
    // sink still works.
    auto blocker = tmp_dir_ / "blocker";
    {
        std::ofstream f(blocker);
        f << "this is a file, not a directory\n";
    }
    auto bad_log = blocker / "out.log";   // parent is a file → mkdir fails
    Logger::instance().enable_console(false);
    // Should not throw.
    EXPECT_NO_THROW(Logger::instance().set_log_file_path(bad_log));
    // Logger should still work (without writing to file).
    EXPECT_NO_THROW(Logger::instance().info("test", "no crash"));
}
