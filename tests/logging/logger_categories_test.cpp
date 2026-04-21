#include "coupecad/logging/logger.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

using coupecad::logging::Level;
using coupecad::logging::Logger;

class LoggerCategoriesTest : public ::testing::Test {
protected:
    void SetUp() override {
        Logger::reset_for_test();
        tmp_file_ = std::filesystem::temp_directory_path() /
                    "coupecad_logger_categories.log";
        std::filesystem::remove(tmp_file_);
        Logger::instance().enable_console(false);
        Logger::instance().set_log_file_path(tmp_file_);
    }
    void TearDown() override {
        Logger::reset_for_test();
        std::filesystem::remove(tmp_file_);
    }

    std::string read_log() {
        Logger::instance().enable_file(false);
        Logger::instance().enable_file(true);
        std::ifstream in(tmp_file_);
        return {std::istreambuf_iterator<char>(in),
                std::istreambuf_iterator<char>()};
    }

    std::filesystem::path tmp_file_;
};

TEST_F(LoggerCategoriesTest, CategoryLevelOverridesGlobal_LessVerbose) {
    Logger::instance().set_min_level(Level::Trace);
    Logger::instance().set_category_level("noisy", Level::Error);

    Logger::instance().info("regular", "regular-info");
    Logger::instance().info("noisy",   "noisy-info-should-be-suppressed");
    Logger::instance().error("noisy",  "noisy-error-should-pass");

    auto contents = read_log();
    EXPECT_NE(contents.find("regular-info"), std::string::npos);
    EXPECT_EQ(contents.find("noisy-info-should-be-suppressed"), std::string::npos);
    EXPECT_NE(contents.find("noisy-error-should-pass"), std::string::npos);
}

TEST_F(LoggerCategoriesTest, CategoryLevelOverridesGlobal_MoreVerbose) {
    Logger::instance().set_min_level(Level::Warning);
    Logger::instance().set_category_level("loud", Level::Debug);

    Logger::instance().debug("loud",    "loud-debug-should-pass");
    Logger::instance().debug("regular", "regular-debug-should-be-suppressed");

    auto contents = read_log();
    EXPECT_NE(contents.find("loud-debug-should-pass"), std::string::npos);
    EXPECT_EQ(contents.find("regular-debug-should-be-suppressed"), std::string::npos);
}

TEST_F(LoggerCategoriesTest, ClearCategoryLevelRestoresGlobal) {
    Logger::instance().set_min_level(Level::Warning);
    Logger::instance().set_category_level("temp", Level::Debug);
    Logger::instance().clear_category_level("temp");

    Logger::instance().debug("temp", "should-be-suppressed-after-clear");
    auto contents = read_log();
    EXPECT_EQ(contents.find("should-be-suppressed-after-clear"), std::string::npos);
}
