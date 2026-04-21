#include "coupecad/logging/logger.h"

#include <gtest/gtest.h>

using coupecad::logging::Level;
using coupecad::logging::Logger;

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override { Logger::reset_for_test(); }
    void TearDown() override { Logger::reset_for_test(); }
};

TEST_F(LoggerTest, DefaultMinLevelIsInfo) {
    EXPECT_EQ(Logger::instance().min_level(), Level::Info);
}

TEST_F(LoggerTest, SetAndGetMinLevel) {
    Logger::instance().set_min_level(Level::Warning);
    EXPECT_EQ(Logger::instance().min_level(), Level::Warning);
}

TEST_F(LoggerTest, OffLevelStartsDisabled_AfterSet) {
    Logger::instance().set_min_level(Level::Off);
    EXPECT_EQ(Logger::instance().min_level(), Level::Off);
}
