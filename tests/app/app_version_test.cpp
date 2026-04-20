#include <gtest/gtest.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

#ifdef COUPECAD_BINARY_PATH
constexpr const char* kCoupeCadBinary = COUPECAD_BINARY_PATH;
#else
constexpr const char* kCoupeCadBinary = "coupecad";
#endif

struct ProcessResult {
    int exit_code = -1;
    std::string stdout_output;
};

ProcessResult run(const std::string& command) {
    ProcessResult result;
    std::array<char, 256> buffer{};
#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if (pipe == nullptr) {
        return result;
    }
    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result.stdout_output.append(buffer.data());
    }
#ifdef _WIN32
    result.exit_code = _pclose(pipe);
#else
    result.exit_code = pclose(pipe);
#endif
    return result;
}

}  // namespace

TEST(AppVersion, PrintsVersionAndExitsZero) {
    const std::string command = std::string("\"") + kCoupeCadBinary + "\" --version";
    const auto result = run(command);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_NE(result.stdout_output.find("CoupeCAD 0.1.0"), std::string::npos)
        << "Got: '" << result.stdout_output << "'";
}
