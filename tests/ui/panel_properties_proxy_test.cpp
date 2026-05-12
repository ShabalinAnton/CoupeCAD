#include "coupecad/ui/panel_properties_proxy.h"

#include "coupecad/core/cabinet.h"
#include "coupecad/core/panel.h"
#include "coupecad/core/project.h"
#include "coupecad/core/undo_stack.h"
#include "coupecad/core/units.h"
#include "coupecad/geometry/geometry_builder.h"
#include "coupecad/renderer/entity_id.h"
#include "coupecad/viewport/viewport_controller.h"
#include "fake_renderer.h"

#include <QCoreApplication>

#include <gtest/gtest.h>

namespace {

bool s_qapp_initialised = false;

void ensure_qapp() {
    if (s_qapp_initialised) return;
    static int    argc = 1;
    static char   arg0[] = "panel_proxy_test";
    static char*  argv[] = {arg0, nullptr};
    new QCoreApplication(argc, argv);
    s_qapp_initialised = true;
}

}  // namespace

using coupecad::core::Millimeters;
using coupecad::core::Panel;
using coupecad::core::PanelId;
using coupecad::core::PanelIdTag;
using coupecad::core::PanelRole;
using coupecad::core::Project;
using coupecad::core::UndoStack;
using coupecad::geometry::GeometryBuilder;
using coupecad::renderer::EntityId;
using coupecad::ui::PanelPropertiesProxy;
using coupecad::viewport::ViewportController;
using coupecad::viewport::testing::FakeRenderer;

namespace {

struct Fixture {
    Project project = Project::create_empty("Initial");
    UndoStack undo{project};
    GeometryBuilder builder{project};
    FakeRenderer fake;
    ViewportController controller{project, builder, fake};
    PanelPropertiesProxy proxy{project, undo, controller};

    Fixture() {
        ensure_qapp();
        undo.add_observer(&controller);
        auto& cab = project.mutable_cabinet();
        cab.dimensions = {Millimeters{1200}, Millimeters{600}, Millimeters{2000}};
        cab.default_panel_thickness = Millimeters{16};
    }

    PanelId add_bottom_panel() {
        Panel p;
        p.id = project.uuid_gen().next_id<PanelIdTag>();
        p.role = PanelRole::Bottom;
        project.mutable_cabinet().panels.emplace(p.id, p);
        return p.id;
    }
};

}  // namespace

TEST(PanelPropertiesProxyTest, HasPanelFalseWhenNothingSelected) {
    Fixture f;
    EXPECT_FALSE(f.proxy.has_panel());
}

TEST(PanelPropertiesProxyTest, HasPanelTrueAfterSelectOne) {
    Fixture f;
    const auto pid = f.add_bottom_panel();
    f.fake.select(EntityId{pid});
    EXPECT_TRUE(f.proxy.has_panel());
    EXPECT_EQ(f.proxy.panel_id_string(), QString::fromStdString(pid.to_string()));
}

TEST(PanelPropertiesProxyTest, HasPanelFalseWhenTwoSelected) {
    Fixture f;
    const auto p1 = f.add_bottom_panel();
    const auto p2 = f.add_bottom_panel();
    f.fake.select(EntityId{p1});
    f.fake.select(EntityId{p2});
    EXPECT_FALSE(f.proxy.has_panel());
}

TEST(PanelPropertiesProxyTest, RoleReadbackMatchesPanelRole) {
    Fixture f;
    const auto pid = f.add_bottom_panel();
    f.fake.select(EntityId{pid});
    EXPECT_EQ(f.proxy.role(), QString("Bottom"));
}

TEST(PanelPropertiesProxyTest, LabelReadbackEmptyByDefault) {
    Fixture f;
    const auto pid = f.add_bottom_panel();
    f.fake.select(EntityId{pid});
    EXPECT_EQ(f.proxy.label(), QString{});
}

TEST(PanelPropertiesProxyTest, ThicknessOverrideReadbackUsesDefaultWhenAbsent) {
    Fixture f;
    const auto pid = f.add_bottom_panel();
    f.fake.select(EntityId{pid});
    EXPECT_FALSE(f.proxy.has_thickness_override());
    EXPECT_EQ(f.proxy.thickness_override_mm(), 16);
}

TEST(PanelPropertiesProxyTest, ThicknessOverrideReadbackUsesOverrideWhenSet) {
    Fixture f;
    const auto pid = f.add_bottom_panel();
    f.project.mutable_cabinet().panels.at(pid).thickness_override = Millimeters{18};
    f.fake.select(EntityId{pid});
    EXPECT_TRUE(f.proxy.has_thickness_override());
    EXPECT_EQ(f.proxy.thickness_override_mm(), 18);
}

TEST(PanelPropertiesProxyTest, MaterialOverrideEmptyByDefault) {
    Fixture f;
    const auto pid = f.add_bottom_panel();
    f.fake.select(EntityId{pid});
    EXPECT_FALSE(f.proxy.has_material_override());
    EXPECT_EQ(f.proxy.material_override_uuid(), QString{});
}
