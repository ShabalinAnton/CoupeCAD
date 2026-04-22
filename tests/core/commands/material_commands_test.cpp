#include "coupecad/core/commands/material_commands.h"
#include "coupecad/core/commands/panel_commands.h"
#include "coupecad/core/errors.h"
#include "coupecad/core/project.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

namespace {
Project make_project() {
    return Project::create_empty("T", make_seeded_uuid_generator(3));
}

Material make_mat(std::string name = "NewMaterial") {
    return Material{.name = std::move(name),
                     .kind = MaterialKind::Mdf,
                     .default_thickness = Millimeters{18}};
}
}  // namespace

TEST(AddMaterial, ApplyRevert) {
    auto p = make_project();
    auto size_before = p.materials().size();
    AddMaterial cmd{make_mat()};
    cmd.apply(p);
    EXPECT_TRUE(cmd.assigned_id().is_valid());
    EXPECT_EQ(p.materials().size(), size_before + 1);
    cmd.revert(p);
    EXPECT_EQ(p.materials().size(), size_before);
}

TEST(AddMaterial, InvalidMaterialThrows) {
    auto p = make_project();
    Material bad = make_mat();
    bad.default_thickness = Millimeters{0};
    AddMaterial cmd{bad};
    EXPECT_THROW(cmd.apply(p), DomainError);
}

TEST(UpdateMaterial, ApplyRevert) {
    auto p = make_project();
    AddMaterial add{make_mat("Orig")};
    add.apply(p);
    auto mid = add.assigned_id();
    Material patched = make_mat("Updated");
    UpdateMaterial upd{mid, patched};
    upd.apply(p);
    EXPECT_EQ(p.materials().at(mid).name, "Updated");
    upd.revert(p);
    EXPECT_EQ(p.materials().at(mid).name, "Orig");
}

TEST(RemoveMaterial, ApplyRevert) {
    auto p = make_project();
    AddMaterial add{make_mat()};
    add.apply(p);
    auto mid = add.assigned_id();
    RemoveMaterial rm{mid};
    rm.apply(p);
    EXPECT_EQ(p.materials().count(mid), 0u);
    rm.revert(p);
    EXPECT_EQ(p.materials().count(mid), 1u);
}

TEST(RemoveMaterial, CabinetDefaultBlocksRemoval) {
    auto p = make_project();
    RemoveMaterial rm{p.cabinet().default_panel_material};
    EXPECT_THROW(rm.apply(p), DomainError);
}

TEST(RemoveMaterial, PanelOverrideBlocksRemoval) {
    auto p = make_project();
    AddMaterial add_mat{make_mat()};
    add_mat.apply(p);
    auto mid = add_mat.assigned_id();

    AddPanel add_pan{PanelRole::Top, NoRoleParams{}};
    add_pan.apply(p);
    SetPanelMaterial set_mat{add_pan.assigned_id(), mid};
    set_mat.apply(p);

    RemoveMaterial rm{mid};
    EXPECT_THROW(rm.apply(p), DomainError);
}
