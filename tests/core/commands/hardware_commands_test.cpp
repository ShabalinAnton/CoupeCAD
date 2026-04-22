#include "coupecad/core/commands/hardware_commands.h"
#include "coupecad/core/commands/panel_commands.h"
#include "coupecad/core/errors.h"
#include "coupecad/core/project.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

namespace {
struct Fixture {
    Project p = Project::create_empty("T", make_seeded_uuid_generator(4));
    PanelId panel_id;
    HardwareRef ref{"hinge.test"};

    Fixture() {
        AddPanel add{PanelRole::Top, NoRoleParams{}};
        add.apply(p);
        panel_id = add.assigned_id();
        HardwareSpec spec{.ref = ref, .kind = HardwareKind::Hinge,
                          .name = "Test hinge",
                          .bbox = Vec3{Millimeters{10}, Millimeters{10},
                                       Millimeters{10}}};
        p.mutable_hardware_catalog()[ref] = spec;
    }

    PanelAttachment att() const {
        return PanelAttachment{.panel_id = panel_id,
                                .local_position = Vec3{},
                                .orientation = Quat::identity()};
    }
};
}  // namespace

TEST(AddHardware, ApplyRevert) {
    Fixture f;
    AddHardware cmd{f.ref, {f.att()}};
    cmd.apply(f.p);
    EXPECT_EQ(f.p.cabinet().hardware.size(), 1u);
    cmd.revert(f.p);
    EXPECT_EQ(f.p.cabinet().hardware.size(), 0u);
}

TEST(AddHardware, UnknownRefThrows) {
    Fixture f;
    AddHardware cmd{HardwareRef{"unknown"}, {f.att()}};
    EXPECT_THROW(cmd.apply(f.p), DomainError);
}

TEST(AddHardware, NoAttachmentsThrows) {
    Fixture f;
    AddHardware cmd{f.ref, {}};
    EXPECT_THROW(cmd.apply(f.p), DomainError);
}

TEST(AddHardware, UnknownPanelThrows) {
    Fixture f;
    PanelAttachment bad{.panel_id = f.p.uuid_gen().next_id<PanelIdTag>(),
                         .local_position = Vec3{},
                         .orientation = Quat::identity()};
    AddHardware cmd{f.ref, {bad}};
    EXPECT_THROW(cmd.apply(f.p), DomainError);
}

TEST(RemoveHardware, ApplyRevert) {
    Fixture f;
    AddHardware add{f.ref, {f.att()}};
    add.apply(f.p);
    auto hid = add.assigned_id();
    RemoveHardware rm{hid};
    rm.apply(f.p);
    EXPECT_EQ(f.p.cabinet().hardware.size(), 0u);
    rm.revert(f.p);
    EXPECT_EQ(f.p.cabinet().hardware.size(), 1u);
}

TEST(UpdateHardwareAttachments, ApplyRevert) {
    Fixture f;
    AddHardware add{f.ref, {f.att()}};
    add.apply(f.p);
    auto hid = add.assigned_id();
    auto doubled = f.att();
    doubled.local_position.x = Millimeters{50};
    UpdateHardwareAttachments upd{hid, {f.att(), doubled}};
    upd.apply(f.p);
    EXPECT_EQ(f.p.cabinet().hardware.at(hid).attachments.size(), 2u);
    upd.revert(f.p);
    EXPECT_EQ(f.p.cabinet().hardware.at(hid).attachments.size(), 1u);
}
