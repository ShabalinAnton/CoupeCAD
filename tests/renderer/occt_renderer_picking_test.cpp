#include "coupecad/renderer/occt/occt_renderer.h"

#include "coupecad/core/panel.h"
#include "coupecad/core/project.h"
#include "coupecad/geometry/geometry_builder.h"

#include <gtest/gtest.h>

using coupecad::core::Millimeters;
using coupecad::core::Project;
using coupecad::geometry::GeometryBuilder;
using coupecad::renderer::CameraState;
using coupecad::renderer::EntityId;
using coupecad::renderer::ViewportSize;
using coupecad::renderer::occt::OcctRenderer;

namespace {

coupecad::core::PanelId set_up_project_and_panel(Project& project) {
    auto& cab = project.mutable_cabinet();
    cab.dimensions = {Millimeters{1000}, Millimeters{500}, Millimeters{1500}};
    coupecad::core::Panel p;
    p.id = project.uuid_gen().next_id<coupecad::core::PanelIdTag>();
    p.role = coupecad::core::PanelRole::Bottom;
    cab.panels.emplace(p.id, p);
    return p.id;
}

}  // namespace

TEST(OcctRendererPickingTest, PickOutsideViewportReturnsNullopt) {
    Project project = Project::create_empty("pick");
    const auto pid = set_up_project_and_panel(project);
    GeometryBuilder builder{project};
    OcctRenderer r{project, builder};
    coupecad::core::ChangeSet cs; cs.added_panels.push_back(pid);
    r.sync(cs);
    r.set_viewport_size(ViewportSize{800, 600});

    EXPECT_FALSE(r.pick(-1, 0).has_value());
    EXPECT_FALSE(r.pick(10000, 10000).has_value());
}

TEST(OcctRendererPickingTest, PickOnPanelReturnsItsId) {
    Project project = Project::create_empty("pick2");
    const auto pid = set_up_project_and_panel(project);
    GeometryBuilder builder{project};
    OcctRenderer r{project, builder};
    coupecad::core::ChangeSet cs; cs.added_panels.push_back(pid);
    r.sync(cs);
    r.set_viewport_size(ViewportSize{800, 600});

    CameraState cam;
    cam.eye    = {Millimeters{500}, Millimeters{250}, Millimeters{5000}};
    cam.target = {Millimeters{500}, Millimeters{250}, Millimeters{0}};
    cam.up     = {Millimeters{0},   Millimeters{1},   Millimeters{0}};
    cam.fov_deg = 30.0;
    r.set_camera(cam);

    auto hit = r.pick(400, 300);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(*hit, EntityId{pid});
}
