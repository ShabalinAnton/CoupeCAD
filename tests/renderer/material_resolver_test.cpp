#include "coupecad/renderer/occt/material_resolver.h"

#include "coupecad/core/cabinet.h"
#include "coupecad/core/errors.h"
#include "coupecad/core/material.h"
#include "coupecad/core/panel.h"
#include "coupecad/core/project.h"
#include "coupecad/core/units.h"

#include <Quantity_Color.hxx>
#include <gtest/gtest.h>

using coupecad::core::Cabinet;
using coupecad::core::DomainError;
using coupecad::core::Material;
using coupecad::core::MaterialId;
using coupecad::core::MaterialIdTag;
using coupecad::core::Project;
using coupecad::core::RGBA;
using coupecad::renderer::occt::resolve_panel_color;
using coupecad::renderer::occt::resolve_hardware_color;

namespace {

Project make_project_with_default_material(RGBA default_color) {
    auto project = Project::create_empty("test");
    auto& cabinet = project.mutable_cabinet();
    project.mutable_materials().at(cabinet.default_panel_material).color_hint = default_color;
    return project;
}

}  // namespace

TEST(MaterialResolverTest, PanelUsesCabinetDefaultWhenNoOverride) {
    auto project = make_project_with_default_material(RGBA{200, 100, 50, 255});

    coupecad::core::Panel panel{};
    panel.role = coupecad::core::PanelRole::Bottom;

    const Quantity_Color color = resolve_panel_color(project, panel);
    EXPECT_NEAR(color.Red(),   200.0 / 255.0, 1e-6);
    EXPECT_NEAR(color.Green(), 100.0 / 255.0, 1e-6);
    EXPECT_NEAR(color.Blue(),   50.0 / 255.0, 1e-6);
}

TEST(MaterialResolverTest, PanelUsesOverrideWhenSet) {
    auto project = make_project_with_default_material(RGBA{0, 0, 0, 255});
    auto extra_id = project.uuid_gen().next_id<MaterialIdTag>();
    Material extra;
    extra.id = extra_id;
    extra.name = "extra";
    extra.color_hint = RGBA{10, 20, 30, 255};
    project.mutable_materials().emplace(extra_id, extra);

    coupecad::core::Panel panel{};
    panel.role = coupecad::core::PanelRole::Bottom;
    panel.material_override = extra_id;

    const Quantity_Color color = resolve_panel_color(project, panel);
    EXPECT_NEAR(color.Red(),   10.0 / 255.0, 1e-6);
    EXPECT_NEAR(color.Green(), 20.0 / 255.0, 1e-6);
    EXPECT_NEAR(color.Blue(),  30.0 / 255.0, 1e-6);
}

TEST(MaterialResolverTest, MissingMaterialThrowsDomainError) {
    auto project = make_project_with_default_material(RGBA{0, 0, 0, 255});
    coupecad::core::Panel panel{};
    panel.role = coupecad::core::PanelRole::Bottom;
    panel.material_override =
        MaterialId::from_string("ffffffff-ffff-ffff-ffff-ffffffffff00");

    try {
        (void)resolve_panel_color(project, panel);
        FAIL() << "expected DomainError";
    } catch (const DomainError& e) {
        EXPECT_EQ(e.code(), "renderer.material_not_found");
    }
}

TEST(MaterialResolverTest, HardwareUsesFixedMetallicGray) {
    const Quantity_Color color = resolve_hardware_color();
    EXPECT_NEAR(color.Red(),   0.55, 1e-6);
    EXPECT_NEAR(color.Green(), 0.57, 1e-6);
    EXPECT_NEAR(color.Blue(),  0.60, 1e-6);
}
