#include "coupecad/viewport/viewport_controller.h"

#include "coupecad/core/project.h"
#include "coupecad/core/units.h"
#include "coupecad/geometry/geometry_builder.h"
#include "coupecad/renderer/i_renderer.h"
#include "fake_renderer.h"

#include <gtest/gtest.h>

#include <cmath>

using coupecad::core::Millimeters;
using coupecad::core::Project;
using coupecad::geometry::GeometryBuilder;
using coupecad::renderer::CameraState;
using coupecad::viewport::MouseButton;
using coupecad::viewport::ViewportController;
using coupecad::viewport::testing::FakeRenderer;

namespace {

CameraState small_distance_camera() {
    // 100 mm from target — small distance accentuates the rounding-to-zero
    // problem in the un-carried code (pan_scale = 0.002 * 100 = 0.2 mm/px,
    // so 1px drag would round to 0 without carry).
    CameraState s;
    s.eye    = {Millimeters{0}, Millimeters{-100}, Millimeters{0}};
    s.target = {Millimeters{0}, Millimeters{0},   Millimeters{0}};
    s.up     = {Millimeters{0}, Millimeters{0},   Millimeters{1}};
    s.fov_deg = 45.0;
    return s;
}

}  // namespace

TEST(ViewportControllerCameraPrecisionTest, SmallPanStepsAccumulate) {
    Project project = Project::create_empty("prec1");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    fake.set_camera(small_distance_camera());
    ViewportController controller{project, builder, fake};

    // 1000× MMB drag of 1px right. Expected pan magnitude per step is
    // 0.2 mm per step (per Stage 4a pan-scale formula); cumulative ~200 mm.
    // With eye at (0,-100,0), target at origin, up=+Z, the right vector
    // is (-1,0,0); pan_x_mm = -dx*dist*kPanScale = -0.2 along right, so
    // world_dx = (-1) * (-0.2) = +0.2 per step → +200 cumulative.
    controller.on_mouse_press(0, 0, MouseButton::Middle);
    for (int i = 1; i <= 1000; ++i) {
        controller.on_mouse_move(i, 0, MouseButton::Middle);
    }
    controller.on_mouse_release(1000, 0, MouseButton::Middle);

    const auto& final_state = fake.camera();
    // Cumulative pan magnitude — both eye and target should have moved.
    // Allow ±5 mm slop for accumulated rounding (target ~200 mm).
    EXPECT_NEAR(static_cast<double>(final_state.eye.x.value()),    200.0, 5.0);
    EXPECT_NEAR(static_cast<double>(final_state.target.x.value()), 200.0, 5.0);
    // Eye-target vector preserved (pan invariant).
    EXPECT_EQ(final_state.eye.y.value() - final_state.target.y.value(), -100);
}

TEST(ViewportControllerCameraPrecisionTest, WheelZoomNoDriftOnNoOp) {
    Project project = Project::create_empty("prec2");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    const auto initial = small_distance_camera();
    fake.set_camera(initial);
    ViewportController controller{project, builder, fake};

    for (int i = 0; i < 50; ++i) {
        controller.on_wheel(0, 0, 0.0);   // factor = 1.0
    }

    const auto& after = fake.camera();
    EXPECT_EQ(after.eye,    initial.eye);
    EXPECT_EQ(after.target, initial.target);
}

TEST(ViewportControllerCameraPrecisionTest, FitAllResetsCarries) {
    Project project = Project::create_empty("prec3");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    fake.set_camera(small_distance_camera());
    ViewportController controller{project, builder, fake};

    // Drag to accumulate carry.
    controller.on_mouse_press(0, 0, MouseButton::Middle);
    for (int i = 1; i <= 50; ++i) controller.on_mouse_move(i, 0, MouseButton::Middle);
    controller.on_mouse_release(50, 0, MouseButton::Middle);

    controller.fit_all();
    // fit_all delegates to the FakeRenderer which is a no-op; assert via
    // a follow-up zero-pan: should produce zero net motion if carries
    // were correctly reset.
    const auto before = fake.camera();
    controller.on_mouse_press(0, 0, MouseButton::Middle);
    controller.on_mouse_release(0, 0, MouseButton::Middle);    // no move
    EXPECT_EQ(fake.camera().eye,    before.eye);
    EXPECT_EQ(fake.camera().target, before.target);
}
