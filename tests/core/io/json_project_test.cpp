#include "coupecad/core/commands/cabinet_commands.h"
#include "coupecad/core/commands/hardware_commands.h"
#include "coupecad/core/commands/hardware_spec_commands.h"
#include "coupecad/core/commands/material_commands.h"
#include "coupecad/core/commands/panel_commands.h"
#include "coupecad/core/errors.h"
#include "coupecad/core/io/json_project_serializer.h"
#include "coupecad/core/project.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

namespace {
std::string bytes_to_string(const std::vector<std::uint8_t>& b) {
    return std::string(b.begin(), b.end());
}
}  // namespace

TEST(JsonProject, EmptyProjectRoundTrip) {
    auto p = Project::create_empty("Empty", make_seeded_uuid_generator(1));
    JsonProjectSerializer s;
    auto bytes = s.serialize(p);
    auto back = s.deserialize(bytes);
    EXPECT_EQ(back.meta().name, "Empty");
    EXPECT_EQ(back.cabinet().dimensions, p.cabinet().dimensions);
    EXPECT_EQ(back.materials().size(), p.materials().size());
}

TEST(JsonProject, NonEmptyProjectRoundTrip) {
    auto p = Project::create_empty("Full", make_seeded_uuid_generator(2));

    // добавим материал, панели, hardware
    AddMaterial am{Material{.name = "Oak", .kind = MaterialKind::SolidWood,
                              .default_thickness = Millimeters{20}}};
    am.apply(p);

    AddPanel ap1{PanelRole::Top, NoRoleParams{}};
    ap1.apply(p);
    AddPanel ap2{PanelRole::Shelf,
                  ShelfParams{.height_from_bottom = Millimeters{900},
                               .extent = ShelfFullWidth{}}};
    ap2.apply(p);

    AddHardwareSpec ahs{HardwareSpec{.ref = HardwareRef{"hinge.test"},
                                      .kind = HardwareKind::Hinge,
                                      .name = "Test",
                                      .bbox = Vec3{Millimeters{35},
                                                    Millimeters{14},
                                                    Millimeters{60}}}};
    ahs.apply(p);

    AddHardware ah{HardwareRef{"hinge.test"},
                    {PanelAttachment{.panel_id = ap1.assigned_id(),
                                      .local_position = Vec3{Millimeters{10},
                                                              Millimeters{20},
                                                              Millimeters{30}},
                                      .orientation = Quat::identity()}}};
    ah.apply(p);

    JsonProjectSerializer s;
    auto bytes = s.serialize(p);
    auto back = s.deserialize(bytes);

    EXPECT_EQ(back.meta().name, "Full");
    EXPECT_EQ(back.materials().size(), p.materials().size());
    EXPECT_EQ(back.cabinet().panels.size(), p.cabinet().panels.size());
    EXPECT_EQ(back.cabinet().hardware.size(), p.cabinet().hardware.size());
    EXPECT_EQ(back.hardware_catalog().size(), p.hardware_catalog().size());

    // Повторная сериализация должна дать тот же JSON (детерминизм).
    auto bytes2 = s.serialize(back);
    EXPECT_EQ(bytes_to_string(bytes), bytes_to_string(bytes2));
}

TEST(JsonProject, MissingSchemaVersionThrows) {
    JsonProjectSerializer s;
    std::string bad = R"({"meta":{}})";
    std::vector<std::uint8_t> bytes(bad.begin(), bad.end());
    EXPECT_THROW(s.deserialize(bytes), InvalidData);
}

TEST(JsonProject, MalformedJsonThrows) {
    JsonProjectSerializer s;
    std::string bad = "{not valid json";
    std::vector<std::uint8_t> bytes(bad.begin(), bad.end());
    EXPECT_THROW(s.deserialize(bytes), InvalidData);
}

TEST(JsonProject, WrongSchemaVersionThrows) {
    JsonProjectSerializer s;
    // Искусственно подменим version в валидном проекте.
    auto p = Project::create_empty("X", make_seeded_uuid_generator(3));
    auto bytes = s.serialize(p);
    std::string text(bytes.begin(), bytes.end());
    auto pos = text.find("\"schema_version\": 1");
    ASSERT_NE(pos, std::string::npos);
    text.replace(pos, std::string("\"schema_version\": 1").size(),
                  "\"schema_version\": 99");
    std::vector<std::uint8_t> corrupted(text.begin(), text.end());
    EXPECT_THROW(s.deserialize(corrupted), UnsupportedVersion);
}
