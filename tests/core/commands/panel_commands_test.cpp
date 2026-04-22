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

TEST(UpdatePanelRoleParams, ApplyRevert) {
    auto p = make_project();
    AddPanel add{PanelRole::Shelf,
                  ShelfParams{.height_from_bottom = Millimeters{800},
                               .extent = ShelfFullWidth{}}};
    add.apply(p);
    auto pid = add.assigned_id();

    UpdatePanelRoleParams upd{pid,
                               ShelfParams{.height_from_bottom = Millimeters{1000},
                                            .extent = ShelfFullWidth{}}};
    upd.apply(p);
    auto& sp = std::get<ShelfParams>(p.cabinet().panels.at(pid).role_params);
    EXPECT_EQ(sp.height_from_bottom, Millimeters{1000});

    upd.revert(p);
    auto& sp2 = std::get<ShelfParams>(p.cabinet().panels.at(pid).role_params);
    EXPECT_EQ(sp2.height_from_bottom, Millimeters{800});
}

TEST(UpdatePanelRoleParams, WrongVariantThrows) {
    auto p = make_project();
    AddPanel add{PanelRole::Top, NoRoleParams{}};
    add.apply(p);
    UpdatePanelRoleParams upd{add.assigned_id(), ShelfParams{}};
    EXPECT_THROW(upd.apply(p), DomainError);
}

TEST(UpdatePanelRoleParams, PreviewUpdateLive) {
    auto p = make_project();
    AddPanel add{PanelRole::Shelf,
                  ShelfParams{.height_from_bottom = Millimeters{800},
                               .extent = ShelfFullWidth{}}};
    add.apply(p);
    auto pid = add.assigned_id();
    UpdatePanelRoleParams upd{pid,
                               ShelfParams{.height_from_bottom = Millimeters{900},
                                            .extent = ShelfFullWidth{}}};
    upd.apply(p);
    upd.update(p, std::any{RoleParams{ShelfParams{.height_from_bottom = Millimeters{950},
                                                   .extent = ShelfFullWidth{}}}});
    auto& sp = std::get<ShelfParams>(p.cabinet().panels.at(pid).role_params);
    EXPECT_EQ(sp.height_from_bottom, Millimeters{950});
    upd.revert(p);
    auto& sp2 = std::get<ShelfParams>(p.cabinet().panels.at(pid).role_params);
    EXPECT_EQ(sp2.height_from_bottom, Millimeters{800});
}

TEST(SetPanelMaterial, ApplyRevert) {
    auto p = make_project();
    AddPanel add{PanelRole::Top, NoRoleParams{}};
    add.apply(p);
    auto pid = add.assigned_id();
    auto mat_id = p.uuid_gen().next_id<MaterialIdTag>();
    p.mutable_materials()[mat_id] = Material{.id = mat_id, .name = "Test",
                                              .kind = MaterialKind::Mdf,
                                              .default_thickness = Millimeters{18}};
    SetPanelMaterial cmd{pid, mat_id};
    cmd.apply(p);
    EXPECT_EQ(p.cabinet().panels.at(pid).material_override, mat_id);
    cmd.revert(p);
    EXPECT_FALSE(p.cabinet().panels.at(pid).material_override.has_value());
}

TEST(SetPanelMaterial, UnknownMaterialThrows) {
    auto p = make_project();
    AddPanel add{PanelRole::Top, NoRoleParams{}};
    add.apply(p);
    SetPanelMaterial cmd{add.assigned_id(),
                          p.uuid_gen().next_id<MaterialIdTag>()};
    EXPECT_THROW(cmd.apply(p), DomainError);
}

TEST(SetPanelThickness, ApplyRevert) {
    auto p = make_project();
    AddPanel add{PanelRole::Top, NoRoleParams{}};
    add.apply(p);
    SetPanelThickness cmd{add.assigned_id(), Millimeters{22}};
    cmd.apply(p);
    EXPECT_EQ(*p.cabinet().panels.at(add.assigned_id()).thickness_override,
              Millimeters{22});
    cmd.revert(p);
    EXPECT_FALSE(p.cabinet().panels.at(add.assigned_id())
                     .thickness_override.has_value());
}

TEST(SetPanelThickness, RejectsNonPositive) {
    EXPECT_THROW(SetPanelThickness(PanelId{}, Millimeters{0}), DomainError);
}

TEST(SetPanelEdgeBanding, ApplyRevert) {
    auto p = make_project();
    AddPanel add{PanelRole::Top, NoRoleParams{}};
    add.apply(p);
    auto pid = add.assigned_id();
    auto mat_id = p.cabinet().default_panel_material;
    SetPanelEdgeBanding cmd{pid, PanelSide::Front,
                              EdgeBanding{.material_id = mat_id,
                                           .thickness = Millimeters{2}}};
    cmd.apply(p);
    ASSERT_TRUE(p.cabinet().panels.at(pid).edge_banding.front.has_value());
    EXPECT_EQ(p.cabinet().panels.at(pid).edge_banding.front->thickness,
              Millimeters{2});
    cmd.revert(p);
    EXPECT_FALSE(p.cabinet().panels.at(pid).edge_banding.front.has_value());
}

TEST(SetPanelLabel, ApplyRevert) {
    auto p = make_project();
    AddPanel add{PanelRole::Top, NoRoleParams{}};
    add.apply(p);
    auto pid = add.assigned_id();
    SetPanelLabel cmd{pid, std::string{"Top panel"}};
    cmd.apply(p);
    EXPECT_EQ(*p.cabinet().panels.at(pid).label, "Top panel");
    cmd.revert(p);
    EXPECT_FALSE(p.cabinet().panels.at(pid).label.has_value());
}

TEST(SetPanelGrain, ApplyRevert) {
    auto p = make_project();
    AddPanel add{PanelRole::Top, NoRoleParams{}};
    add.apply(p);
    auto pid = add.assigned_id();
    SetPanelGrain cmd{pid, GrainDirection::Horizontal};
    cmd.apply(p);
    EXPECT_EQ(p.cabinet().panels.at(pid).grain_direction,
              GrainDirection::Horizontal);
    cmd.revert(p);
    EXPECT_EQ(p.cabinet().panels.at(pid).grain_direction, GrainDirection::None);
}
