#include "coupecad/core/commands/hardware_spec_commands.h"
#include "coupecad/core/commands/hardware_commands.h"
#include "coupecad/core/commands/panel_commands.h"
#include "coupecad/core/errors.h"
#include "coupecad/core/project.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

namespace {
HardwareSpec make_spec(std::string_view ref = "test.spec") {
    return HardwareSpec{.ref = HardwareRef{std::string{ref}},
                         .kind = HardwareKind::Hinge,
                         .name = "Test",
                         .bbox = Vec3{Millimeters{10}, Millimeters{10},
                                       Millimeters{10}}};
}
}  // namespace

TEST(AddHardwareSpec, ApplyRevert) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(5));
    AddHardwareSpec cmd{make_spec()};
    cmd.apply(p);
    EXPECT_EQ(p.hardware_catalog().size(), 1u);
    cmd.revert(p);
    EXPECT_EQ(p.hardware_catalog().size(), 0u);
}

TEST(AddHardwareSpec, DuplicateRefThrows) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(5));
    AddHardwareSpec a{make_spec("dup")};
    a.apply(p);
    AddHardwareSpec b{make_spec("dup")};
    EXPECT_THROW(b.apply(p), DomainError);
}

TEST(UpdateHardwareSpec, ApplyRevert) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(5));
    AddHardwareSpec add{make_spec("u1")};
    add.apply(p);
    HardwareSpec patched = make_spec("u1");
    patched.name = "Updated name";
    UpdateHardwareSpec upd{HardwareRef{"u1"}, patched};
    upd.apply(p);
    EXPECT_EQ(p.hardware_catalog().at(HardwareRef{"u1"}).name, "Updated name");
    upd.revert(p);
    EXPECT_EQ(p.hardware_catalog().at(HardwareRef{"u1"}).name, "Test");
}

TEST(RemoveHardwareSpec, ApplyRevert) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(5));
    AddHardwareSpec add{make_spec("r1")};
    add.apply(p);
    RemoveHardwareSpec rm{HardwareRef{"r1"}};
    rm.apply(p);
    EXPECT_EQ(p.hardware_catalog().count(HardwareRef{"r1"}), 0u);
    rm.revert(p);
    EXPECT_EQ(p.hardware_catalog().count(HardwareRef{"r1"}), 1u);
}

TEST(RemoveHardwareSpec, BlockedWhenInUse) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(5));
    AddPanel add_pan{PanelRole::Top, NoRoleParams{}};
    add_pan.apply(p);
    AddHardwareSpec add_spec{make_spec("in_use")};
    add_spec.apply(p);
    AddHardware add_hw{HardwareRef{"in_use"},
                        {PanelAttachment{.panel_id = add_pan.assigned_id(),
                                          .local_position = Vec3{},
                                          .orientation = Quat::identity()}}};
    add_hw.apply(p);

    RemoveHardwareSpec rm{HardwareRef{"in_use"}};
    EXPECT_THROW(rm.apply(p), DomainError);
}
