#include "coupecad/core/io/json_project_serializer.h"
#include "coupecad/core/project.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace coupecad::core;

namespace {
std::filesystem::path golden_path() {
    // Путь относительный к source директории.
    return std::filesystem::path{COUPECAD_TESTS_DIR} / "core" / "io" / "golden" /
           "v1_reference.json";
}

std::string read_file(const std::filesystem::path& p) {
    std::ifstream f(p);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
}  // namespace

TEST(Golden, V1ReferenceMatchesEmptyProject) {
    auto p = Project::create_empty("Golden", make_seeded_uuid_generator(42));
    JsonProjectSerializer s;
    auto bytes = s.serialize(p);
    std::string actual(bytes.begin(), bytes.end());

    if (std::getenv("GOLDEN_UPDATE")) {
        std::filesystem::create_directories(golden_path().parent_path());
        std::ofstream out(golden_path());
        out << actual;
        SUCCEED() << "Updated golden at " << golden_path();
        return;
    }

    ASSERT_TRUE(std::filesystem::exists(golden_path()))
        << "Golden not found; run once with GOLDEN_UPDATE=1 to create: "
        << golden_path();
    std::string expected = read_file(golden_path());
    EXPECT_EQ(actual, expected)
        << "Serialized project doesn't match golden.\n"
        << "If the change is intentional, run this test with GOLDEN_UPDATE=1\n"
        << "and commit the updated " << golden_path();
}
