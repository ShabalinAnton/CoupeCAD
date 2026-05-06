#include "coupecad/renderer/occt/ais_scene.h"
#include "coupecad/renderer/occt/view_driver.h"

#include "coupecad/core/cabinet.h"
#include "coupecad/core/hardware.h"
#include "coupecad/core/panel.h"
#include "coupecad/core/project.h"
#include "coupecad/geometry/geometry_builder.h"

#include <BRepPrimAPI_MakeBox.hxx>
#include <Quantity_Color.hxx>
#include <gtest/gtest.h>

using coupecad::core::Project;
using coupecad::geometry::GeometryBuilder;
using coupecad::renderer::occt::AisScene;
using coupecad::renderer::occt::ViewDriver;

namespace {

struct Fixture {
    Project   project = Project::create_empty("test");
    GeometryBuilder builder{project};
    ViewDriver      driver;
    AisScene        scene{driver, project};

    coupecad::core::PanelId add_bottom_panel() {
        auto& cabinet = project.mutable_cabinet();
        cabinet.dimensions = {coupecad::core::Millimeters{1000},
                              coupecad::core::Millimeters{500},
                              coupecad::core::Millimeters{1500}};
        coupecad::core::Panel panel;
        panel.id = project.uuid_gen().next_id<coupecad::core::PanelIdTag>();
        panel.role = coupecad::core::PanelRole::Bottom;
        cabinet.panels.emplace(panel.id, panel);
        return panel.id;
    }
};

}  // namespace

TEST(AisSceneTest, EmptyAfterConstruction) {
    Fixture f;
    EXPECT_EQ(f.scene.panel_count(), 0u);
}

TEST(AisSceneTest, AddPanelStoresAisHandle) {
    Fixture f;
    const auto pid = f.add_bottom_panel();
    f.scene.add_panel(pid, f.builder.panel_solid(pid));
    EXPECT_EQ(f.scene.panel_count(), 1u);
    EXPECT_TRUE(f.scene.has_panel(pid));
}

TEST(AisSceneTest, RemovePanelDropsHandle) {
    Fixture f;
    const auto pid = f.add_bottom_panel();
    f.scene.add_panel(pid, f.builder.panel_solid(pid));
    f.scene.remove_panel(pid);
    EXPECT_FALSE(f.scene.has_panel(pid));
    EXPECT_EQ(f.scene.panel_count(), 0u);
}

TEST(AisSceneTest, RemoveUnknownPanelIsNoOp) {
    Fixture f;
    auto missing = coupecad::core::PanelId::from_string(
        "00000000-0000-0000-0000-000000000001");
    EXPECT_NO_THROW(f.scene.remove_panel(missing));
}

TEST(AisSceneTest, ReplacePanelUpdatesHandlePointer) {
    Fixture f;
    const auto pid = f.add_bottom_panel();
    f.scene.add_panel(pid, f.builder.panel_solid(pid));
    const auto* before = f.scene.raw_ais_pointer_for_panel(pid);

    auto& cabinet = f.project.mutable_cabinet();
    cabinet.dimensions.width = coupecad::core::Millimeters{2000};
    f.builder.rebuild_all();
    f.scene.replace_panel(pid, f.builder.panel_solid(pid));

    const auto* after = f.scene.raw_ais_pointer_for_panel(pid);
    EXPECT_NE(before, after);
    EXPECT_TRUE(f.scene.has_panel(pid));
}

namespace {

coupecad::core::HardwareItemId add_minimal_hardware(Fixture& f) {
    coupecad::core::HardwareRef ref{"hw.test.box"};
    coupecad::core::HardwareSpec spec;
    spec.ref = ref;
    spec.kind = coupecad::core::HardwareKind::Other;
    spec.name = "test box";
    spec.bbox = {coupecad::core::Millimeters{30},
                 coupecad::core::Millimeters{30},
                 coupecad::core::Millimeters{30}};
    f.project.mutable_hardware_catalog().emplace(ref, spec);

    coupecad::core::HardwareItem item;
    item.id = f.project.uuid_gen().next_id<coupecad::core::HardwareItemIdTag>();
    item.ref = ref;
    coupecad::core::PanelAttachment att;
    att.panel_id = f.project.cabinet().panels.begin()->first;
    att.local_position = {coupecad::core::Millimeters{10},
                          coupecad::core::Millimeters{10},
                          coupecad::core::Millimeters{10}};
    item.attachments.push_back(att);
    f.project.mutable_cabinet().hardware.emplace(item.id, item);
    return item.id;
}

}  // namespace

TEST(AisSceneTest, AddHardwareStoresAisHandle) {
    Fixture f;
    f.add_bottom_panel();
    const auto hid = add_minimal_hardware(f);
    f.scene.add_hardware(hid, f.builder.hardware_compound(hid));
    EXPECT_TRUE(f.scene.has_hardware(hid));
    EXPECT_EQ(f.scene.hardware_count(), 1u);
}

TEST(AisSceneTest, RemoveHardwareDropsHandle) {
    Fixture f;
    f.add_bottom_panel();
    const auto hid = add_minimal_hardware(f);
    f.scene.add_hardware(hid, f.builder.hardware_compound(hid));
    f.scene.remove_hardware(hid);
    EXPECT_FALSE(f.scene.has_hardware(hid));
}

TEST(AisSceneTest, RefreshColorsForMaterialUpdatesAffectedPanelColor) {
    Fixture f;
    const auto pid = f.add_bottom_panel();
    f.scene.add_panel(pid, f.builder.panel_solid(pid));

    auto& cabinet = f.project.mutable_cabinet();
    auto& materials = f.project.mutable_materials();
    materials.at(cabinet.default_panel_material).color_hint =
        coupecad::core::RGBA{255, 0, 0, 255};

    f.scene.refresh_colors_for_material(cabinet.default_panel_material);

    EXPECT_TRUE(f.scene.has_panel(pid));
    Quantity_Color c;
    f.scene.context()->Color(
        f.scene.raw_ais_handle_for_panel(pid), c);
    EXPECT_NEAR(c.Red(),   1.0, 1e-3);
    EXPECT_NEAR(c.Green(), 0.0, 1e-3);
    EXPECT_NEAR(c.Blue(),  0.0, 1e-3);
}
