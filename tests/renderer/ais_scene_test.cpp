#include "coupecad/renderer/occt/ais_scene.h"
#include "coupecad/renderer/occt/view_driver.h"

#include "coupecad/core/cabinet.h"
#include "coupecad/core/panel.h"
#include "coupecad/core/project.h"
#include "coupecad/geometry/geometry_builder.h"

#include <BRepPrimAPI_MakeBox.hxx>
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
