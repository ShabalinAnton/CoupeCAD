#include "coupecad/core/commands/cabinet_commands.h"
#include "coupecad/core/errors.h"
#include "coupecad/core/project.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

namespace {
Project make_project() {
    return Project::create_empty("T", make_seeded_uuid_generator(1));
}
}  // namespace

TEST(SetCabinetDimensions, ApplyAndRevert) {
    auto p = make_project();
    Dimensions original = p.cabinet().dimensions;

    SetCabinetDimensions cmd{p.cabinet().id,
                              Dimensions{Millimeters{3000}, Millimeters{700},
                                         Millimeters{2500}}};
    auto cs = cmd.apply(p);
    EXPECT_TRUE(cs.cabinet_changed);
    EXPECT_EQ(p.cabinet().dimensions.width, Millimeters{3000});

    cmd.revert(p);
    EXPECT_EQ(p.cabinet().dimensions, original);
}

TEST(SetCabinetDimensions, RejectsNonPositive) {
    EXPECT_THROW(SetCabinetDimensions(CabinetId{},
                                       Dimensions{Millimeters{0},
                                                  Millimeters{600},
                                                  Millimeters{2400}}),
                 DomainError);
}

TEST(SetCabinetDimensions, WrongTargetThrowsOnApply) {
    auto p = make_project();
    SetCabinetDimensions cmd{CabinetId{p.uuid_gen().next()},
                              Dimensions{Millimeters{1000}, Millimeters{500},
                                         Millimeters{1000}}};
    EXPECT_THROW(cmd.apply(p), DomainError);
}

TEST(SetCabinetDimensions, PreviewUpdateChangesLiveProject) {
    auto p = make_project();
    SetCabinetDimensions cmd{p.cabinet().id,
                              Dimensions{Millimeters{2500}, Millimeters{650},
                                         Millimeters{2400}}};
    cmd.apply(p);   // устанавливает initial снимок
    cmd.update(p,
               std::any{Dimensions{Millimeters{2800}, Millimeters{700},
                                    Millimeters{2500}}});
    EXPECT_EQ(p.cabinet().dimensions.width, Millimeters{2800});
    cmd.revert(p);
    EXPECT_EQ(p.cabinet().dimensions.width, Millimeters{2400});   // исходные 2400
}

TEST(SetCabinetDimensions, PreviewUpdateRejectsWrongType) {
    auto p = make_project();
    SetCabinetDimensions cmd{p.cabinet().id,
                              Dimensions{Millimeters{2500}, Millimeters{650},
                                         Millimeters{2400}}};
    cmd.apply(p);
    EXPECT_THROW(cmd.update(p, std::any{42}), LogicError);
}

TEST(SetCabinetDefaults, ApplyAndRevert) {
    auto p = make_project();
    auto old_material = p.cabinet().default_panel_material;
    auto new_mat_id = p.uuid_gen().next_id<MaterialIdTag>();

    // Добавим материал вручную (Stage 1a-style), затем через команду переключим
    // default.
    Material mat{.id = new_mat_id,
                  .name = "MDF 18mm",
                  .kind = MaterialKind::Mdf,
                  .default_thickness = Millimeters{18}};
    p.mutable_materials()[new_mat_id] = mat;

    SetCabinetDefaults cmd{p.cabinet().id, new_mat_id, Millimeters{18},
                            Millimeters{6}};
    cmd.apply(p);
    EXPECT_EQ(p.cabinet().default_panel_material, new_mat_id);
    EXPECT_EQ(p.cabinet().default_panel_thickness, Millimeters{18});
    EXPECT_EQ(p.cabinet().default_back_thickness, Millimeters{6});

    cmd.revert(p);
    EXPECT_EQ(p.cabinet().default_panel_material, old_material);
    EXPECT_EQ(p.cabinet().default_panel_thickness, Millimeters{16});
    EXPECT_EQ(p.cabinet().default_back_thickness, Millimeters{4});
}

TEST(SetCabinetDefaults, UnknownMaterialThrowsOnApply) {
    auto p = make_project();
    auto unknown = p.uuid_gen().next_id<MaterialIdTag>();
    SetCabinetDefaults cmd{p.cabinet().id, unknown, Millimeters{16},
                            Millimeters{4}};
    EXPECT_THROW(cmd.apply(p), DomainError);
}

TEST(SetCabinetNameTest, AppliesAndStoresPreviousName) {
    Project project = Project::create_empty("Initial");
    SetCabinetName cmd{project.cabinet().id, "Renamed"};
    auto cs = cmd.apply(project);
    EXPECT_EQ(project.cabinet().name, "Renamed");
    EXPECT_TRUE(cs.cabinet_changed);
}

TEST(SetCabinetNameTest, RevertRestoresName) {
    Project project = Project::create_empty("Initial");
    const std::string original_name = project.cabinet().name;
    SetCabinetName cmd{project.cabinet().id, "Renamed"};
    cmd.apply(project);
    cmd.revert(project);
    EXPECT_EQ(project.cabinet().name, original_name);
}

TEST(SetCabinetNameTest, RejectsEmptyName) {
    try {
        SetCabinetName cmd{CabinetId{}, ""};
        FAIL() << "expected DomainError";
    } catch (const DomainError& e) {
        EXPECT_EQ(e.code(), "cabinet.empty_name");
    }
}

TEST(SetCabinetNameTest, WrongIdThrows) {
    Project project = Project::create_empty("Initial");
    SetCabinetName cmd{CabinetId::from_string("00000000-0000-0000-0000-000000000099"),
                       "Renamed"};
    try {
        cmd.apply(project);
        FAIL() << "expected DomainError";
    } catch (const DomainError& e) {
        EXPECT_EQ(e.code(), "cabinet.wrong_id");
    }
}
