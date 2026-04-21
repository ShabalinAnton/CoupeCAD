#include "coupecad/logging/logger.h"

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

using coupecad::logging::Logger;

class LoggerConcurrentTest : public ::testing::Test {
protected:
    void SetUp() override {
        Logger::reset_for_test();
        tmp_file_ = std::filesystem::temp_directory_path() /
                    "coupecad_logger_concurrent.log";
        std::filesystem::remove(tmp_file_);
        Logger::instance().enable_console(false);
        Logger::instance().set_log_file_path(tmp_file_);
    }
    void TearDown() override {
        Logger::reset_for_test();
        std::filesystem::remove(tmp_file_);
    }

    std::filesystem::path tmp_file_;
};

TEST_F(LoggerConcurrentTest, EightThreadsThousandLinesEach) {
    constexpr int kThreads = 8;
    constexpr int kPerThread = 1000;

    std::vector<std::thread> ts;
    for (int t = 0; t < kThreads; ++t) {
        ts.emplace_back([t]() {
            for (int i = 0; i < kPerThread; ++i) {
                Logger::instance().info("conc",
                                        "thread={} index={}",
                                        t,
                                        i);
            }
        });
    }
    for (auto& th : ts) {
        th.join();
    }

    Logger::instance().enable_file(false);
    Logger::instance().enable_file(true);

    std::ifstream in(tmp_file_);
    std::string line;
    int count = 0;
    while (std::getline(in, line)) {
        if (line.find("thread=") != std::string::npos) {
            ++count;
        }
    }
    EXPECT_EQ(count, kThreads * kPerThread);
}
