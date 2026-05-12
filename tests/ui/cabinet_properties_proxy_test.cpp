#include "coupecad/ui/cabinet_properties_proxy.h"

#include "coupecad/core/project.h"
#include "coupecad/core/undo_stack.h"
#include "coupecad/core/units.h"
#include "coupecad/geometry/geometry_builder.h"
#include "coupecad/viewport/viewport_controller.h"
#include "fake_renderer.h"

#include <QCoreApplication>

#include <gtest/gtest.h>

namespace {

bool s_qapp_initialised = false;

void ensure_qapp() {
    if (s_qapp_initialised) return;
    static int    argc = 1;
    static char   arg0[] = "cabinet_proxy_test";
    static char*  argv[] = {arg0, nullptr};
    new QCoreApplication(argc, argv);
    s_qapp_initialised = true;
}

}  // namespace

using coupecad::core::Millimeters;
using coupecad::core::Project;
using coupecad::core::UndoStack;
using coupecad::geometry::GeometryBuilder;
using coupecad::ui::CabinetPropertiesProxy;
using coupecad::viewport::ViewportController;
using coupecad::viewport::testing::FakeRenderer;

namespace {

struct Fixture {
    Project project = Project::create_empty("Initial");
    UndoStack undo{project};
    GeometryBuilder builder{project};
    FakeRenderer fake;
    ViewportController controller{project, builder, fake};
    CabinetPropertiesProxy proxy{project, undo, controller};

    Fixture() {
        ensure_qapp();
        auto& cab = project.mutable_cabinet();
        cab.dimensions = {Millimeters{1200}, Millimeters{600}, Millimeters{2000}};
        cab.default_panel_thickness = Millimeters{16};
        cab.default_back_thickness  = Millimeters{4};
    }
};

}  // namespace

TEST(CabinetPropertiesProxyTest, NameReflectsModel) {
    Fixture f;
    EXPECT_EQ(f.proxy.name(),
              QString::fromStdString(f.project.cabinet().name));
}

TEST(CabinetPropertiesProxyTest, WidthMmReflectsModel) {
    Fixture f;
    EXPECT_EQ(f.proxy.widthMm(), 1200);
    EXPECT_EQ(f.proxy.depthMm(), 600);
    EXPECT_EQ(f.proxy.heightMm(), 2000);
}

TEST(CabinetPropertiesProxyTest, DefaultThicknessesReflectModel) {
    Fixture f;
    EXPECT_EQ(f.proxy.default_panel_thickness_mm(), 16);
    EXPECT_EQ(f.proxy.default_back_thickness_mm(), 4);
}
