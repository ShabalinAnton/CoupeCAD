#include "coupecad/core/commands/panel_commands.h"
#include "coupecad/core/errors.h"
#include "coupecad/core/io/ccad_archive.h"
#include "coupecad/core/io/json_project_serializer.h"
#include "coupecad/core/project.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

using namespace coupecad::core;

namespace {
class FixedClock : public IClock {
public:
    std::chrono::system_clock::time_point now() const override {
        // 2026-04-22T10:00:00Z (sec since epoch — произвольное фиксированное).
        return std::chrono::system_clock::from_time_t(1782360000);
    }
};

class CcadArchiveTest : public ::testing::Test {
protected:
    void SetUp() override {
        tmp_ = std::filesystem::temp_directory_path() /
               ("coupecad_test_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()));
        std::filesystem::create_directories(tmp_);
    }
    void TearDown() override { std::filesystem::remove_all(tmp_); }
    std::filesystem::path tmp_;
};
}  // namespace

TEST_F(CcadArchiveTest, EmptyProjectRoundTrip) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(1));
    JsonProjectSerializer s;
    auto path = tmp_ / "empty.ccad";
    FixedClock clock;
    CcadArchive::save(p, path, s, clock);
    ASSERT_TRUE(std::filesystem::exists(path));

    auto back = CcadArchive::load(path, s);
    EXPECT_EQ(back.meta().name, "T");
    EXPECT_EQ(back.cabinet().dimensions, p.cabinet().dimensions);
}

TEST_F(CcadArchiveTest, NonEmptyProjectRoundTrip) {
    auto p = Project::create_empty("Loaded", make_seeded_uuid_generator(2));
    AddPanel ap{PanelRole::Top, NoRoleParams{}};
    ap.apply(p);

    JsonProjectSerializer s;
    auto path = tmp_ / "full.ccad";
    FixedClock clock;
    CcadArchive::save(p, path, s, clock);
    auto back = CcadArchive::load(path, s);
    EXPECT_EQ(back.cabinet().panels.size(), 1u);
}

TEST_F(CcadArchiveTest, MissingFileThrows) {
    JsonProjectSerializer s;
    EXPECT_THROW(CcadArchive::load(tmp_ / "no_such.ccad", s),
                 CorruptedArchive);
}

TEST_F(CcadArchiveTest, NotAZipThrows) {
    auto path = tmp_ / "bad.ccad";
    {
        std::ofstream f(path, std::ios::binary);
        f << "not a zip";
    }
    JsonProjectSerializer s;
    EXPECT_THROW(CcadArchive::load(path, s), CorruptedArchive);
}

TEST_F(CcadArchiveTest, DataFileNameFromEncoding) {
    EXPECT_EQ(CcadArchive::data_file_name("json"), "project.json");
    EXPECT_THROW(CcadArchive::data_file_name("proprietary_binary"),
                 UnsupportedEncoding);
}
