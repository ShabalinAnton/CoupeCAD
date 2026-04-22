#include "coupecad/core/commands/panel_commands.h"
#include "coupecad/core/errors.h"
#include "coupecad/core/project.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

namespace {
Project make_project() {
    return Project::create_empty("T", make_seeded_uuid_generator(2));
}
}  // namespace

TEST(AddPanel, AppliesAndRevertsRoundTrip) {
    auto p = make_project();
    AddPanel cmd{PanelRole::Shelf,
                  ShelfParams{.height_from_bottom = Millimeters{800},
                               .extent = ShelfFullWidth{}}};

    auto cs = cmd.apply(p);
    ASSERT_EQ(cs.added_panels.size(), 1u);
    auto pid = cmd.assigned_id();
    ASSERT_TRUE(pid.is_valid());
    EXPECT_EQ(p.cabinet().panels.size(), 1u);

    cmd.revert(p);
    EXPECT_EQ(p.cabinet().panels.size(), 0u);
}

TEST(AddPanel, RejectsRoleParamsMismatch) {
    EXPECT_THROW(AddPanel(PanelRole::Top, ShelfParams{}), DomainError);
}

TEST(AddPanel, RedoKeepsSameId) {
    auto p = make_project();
    AddPanel cmd{PanelRole::Top, NoRoleParams{}};
    cmd.apply(p);
    auto first_id = cmd.assigned_id();
    cmd.revert(p);
    cmd.apply(p);   // re-apply (redo)
    EXPECT_EQ(cmd.assigned_id(), first_id);
    EXPECT_EQ(p.cabinet().panels.count(first_id), 1u);
}

TEST(RemovePanel, RemovesAndRestores) {
    auto p = make_project();
    AddPanel add{PanelRole::Top, NoRoleParams{}};
    add.apply(p);
    auto pid = add.assigned_id();

    RemovePanel rm{pid};
    auto cs = rm.apply(p);
    EXPECT_EQ(cs.removed_panels.size(), 1u);
    EXPECT_EQ(p.cabinet().panels.count(pid), 0u);

    rm.revert(p);
    EXPECT_EQ(p.cabinet().panels.count(pid), 1u);
}

TEST(RemovePanel, UnknownIdThrows) {
    auto p = make_project();
    RemovePanel rm{PanelId{}};
    EXPECT_THROW(rm.apply(p), DomainError);
}

TEST(RemovePanel, BlockedByHardwareReference) {
    auto p = make_project();
    AddPanel add{PanelRole::Top, NoRoleParams{}};
    add.apply(p);
    auto pid = add.assigned_id();

    // Добавляем hardware spec + item прямым доступом (команды для них — Task 6/7).
    HardwareSpec spec{.ref = HardwareRef{"test.ref"},
                      .kind = HardwareKind::Hinge,
                      .name = "Test",
                      .bbox = Vec3{Millimeters{10}, Millimeters{10}, Millimeters{10}}};
    p.mutable_hardware_catalog()[spec.ref] = spec;
    HardwareItem hw;
    hw.id = p.uuid_gen().next_id<HardwareItemIdTag>();
    hw.ref = spec.ref;
    hw.attachments.push_back(PanelAttachment{.panel_id = pid,
                                              .local_position = Vec3{},
                                              .orientation = Quat::identity()});
    p.mutable_cabinet().hardware[hw.id] = std::move(hw);

    RemovePanel rm{pid};
    EXPECT_THROW(rm.apply(p), DomainError);
}
