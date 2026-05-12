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
        undo.add_observer(&controller);
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

#include <QSignalSpy>

TEST(CabinetPropertiesProxyTest, SetWidthMmDispatchesCommand) {
    Fixture f;
    f.proxy.setWidthMm(1500);
    EXPECT_EQ(f.project.cabinet().dimensions.width.value(), 1500);
}

TEST(CabinetPropertiesProxyTest, SetWidthMmFiresChangedSignal) {
    Fixture f;
    QSignalSpy spy(&f.proxy, &CabinetPropertiesProxy::changed);
    f.proxy.setWidthMm(1500);
    EXPECT_GE(spy.count(), 1);
}

TEST(CabinetPropertiesProxyTest, SetWidthMmRejectsNonPositive) {
    Fixture f;
    EXPECT_NO_THROW(f.proxy.setWidthMm(0));
    EXPECT_EQ(f.project.cabinet().dimensions.width.value(), 1200);
}

TEST(CabinetPropertiesProxyTest, SetWidthMmNoOpDoesNotDispatch) {
    Fixture f;
    f.proxy.setWidthMm(1200);
    EXPECT_FALSE(f.undo.can_undo());
}

TEST(CabinetPropertiesProxyTest, UndoRestoresWidth) {
    Fixture f;
    f.proxy.setWidthMm(1500);
    f.undo.undo();
    EXPECT_EQ(f.project.cabinet().dimensions.width.value(), 1200);
    EXPECT_EQ(f.proxy.widthMm(), 1200);
}

TEST(CabinetPropertiesProxyTest, SetDepthMmDispatches) {
    Fixture f;
    f.proxy.setDepthMm(800);
    EXPECT_EQ(f.project.cabinet().dimensions.depth.value(), 800);
}

TEST(CabinetPropertiesProxyTest, SetHeightMmDispatches) {
    Fixture f;
    f.proxy.setHeightMm(2500);
    EXPECT_EQ(f.project.cabinet().dimensions.height.value(), 2500);
}

TEST(CabinetPropertiesProxyTest, SetDefaultPanelThicknessDispatches) {
    Fixture f;
    f.proxy.set_default_panel_thickness_mm(18);
    EXPECT_EQ(f.project.cabinet().default_panel_thickness.value(), 18);
}

TEST(CabinetPropertiesProxyTest, SetDefaultBackThicknessDispatches) {
    Fixture f;
    f.proxy.set_default_back_thickness_mm(6);
    EXPECT_EQ(f.project.cabinet().default_back_thickness.value(), 6);
}

TEST(CabinetPropertiesProxyTest, SetNameDispatches) {
    Fixture f;
    f.proxy.setName(QString("Renamed"));
    EXPECT_EQ(f.project.cabinet().name, std::string{"Renamed"});
}

TEST(CabinetPropertiesProxyTest, SetNameNoOpForSameValue) {
    Fixture f;
    const std::string original = f.project.cabinet().name;
    f.proxy.setName(QString::fromStdString(original));   // same as current
    EXPECT_FALSE(f.undo.can_undo());
}

TEST(CabinetPropertiesProxyTest, SetNameRejectsEmpty) {
    Fixture f;
    const std::string original = f.project.cabinet().name;
    EXPECT_NO_THROW(f.proxy.setName(QString("")));   // swallowed
    EXPECT_EQ(f.project.cabinet().name, original);
}
