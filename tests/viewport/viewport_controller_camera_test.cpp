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

CameraState make_initial_camera() {
    CameraState s;
    s.eye    = {Millimeters{0},    Millimeters{-1000}, Millimeters{0}};
    s.target = {Millimeters{0},    Millimeters{0},      Millimeters{0}};
    s.up     = {Millimeters{0},    Millimeters{0},      Millimeters{1}};
    s.fov_deg = 45.0;
    return s;
}

double distance_xy(const coupecad::core::Vec3& a,
                   const coupecad::core::Vec3& b) {
    const double dx = static_cast<double>(a.x.value() - b.x.value());
    const double dy = static_cast<double>(a.y.value() - b.y.value());
    return std::sqrt(dx * dx + dy * dy);
}

}  // namespace

TEST(ViewportControllerCameraTest, LmbDragOrbitsAroundTarget) {
    Project project = Project::create_empty("cam1");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    fake.set_camera(make_initial_camera());
    ViewportController controller{project, builder, fake};

    controller.on_mouse_press(400, 300, MouseButton::Left);
    controller.on_mouse_move(450, 300, MouseButton::Left);  // +50px in X

    ASSERT_FALSE(fake.set_camera_calls.empty());
    const auto& after = fake.set_camera_calls.back();
    // Distance to target preserved (within int-mm rounding).
    const double dist = distance_xy(after.eye, after.target);
    EXPECT_NEAR(dist, 1000.0, 5.0);
    // Eye actually moved (azimuth changed).
    EXPECT_NE(after.eye, make_initial_camera().eye);
}

TEST(ViewportControllerCameraTest, MmbDragPansBothEyeAndTarget) {
    Project project = Project::create_empty("cam2");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    fake.set_camera(make_initial_camera());
    ViewportController controller{project, builder, fake};

    const auto initial = fake.camera();
    controller.on_mouse_press(400, 300, MouseButton::Middle);
    controller.on_mouse_move(420, 300, MouseButton::Middle);

    ASSERT_FALSE(fake.set_camera_calls.empty());
    const auto& after = fake.set_camera_calls.back();
    EXPECT_NE(after.eye,    initial.eye);
    EXPECT_NE(after.target, initial.target);
    // Eye - target vector is preserved on pan.
    const auto eye_target_initial = coupecad::core::Vec3{
        Millimeters{initial.eye.x.value() - initial.target.x.value()},
        Millimeters{initial.eye.y.value() - initial.target.y.value()},
        Millimeters{initial.eye.z.value() - initial.target.z.value()}};
    const auto eye_target_after = coupecad::core::Vec3{
        Millimeters{after.eye.x.value() - after.target.x.value()},
        Millimeters{after.eye.y.value() - after.target.y.value()},
        Millimeters{after.eye.z.value() - after.target.z.value()}};
    EXPECT_EQ(eye_target_initial, eye_target_after);
}

TEST(ViewportControllerCameraTest, WheelUpZoomsIn) {
    Project project = Project::create_empty("cam3");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    fake.set_camera(make_initial_camera());
    ViewportController controller{project, builder, fake};

    controller.on_wheel(0, 0, +1.0);  // wheel up

    ASSERT_FALSE(fake.set_camera_calls.empty());
    const auto& after = fake.set_camera_calls.back();
    const double dist = distance_xy(after.eye, after.target);
    // 1000 / 1.1 ≈ 909
    EXPECT_NEAR(dist, 909.0, 3.0);
}

TEST(ViewportControllerCameraTest, FitAllDelegatesToRenderer) {
    Project project = Project::create_empty("cam4");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    ViewportController controller{project, builder, fake};

    controller.fit_all();
    EXPECT_EQ(fake.fit_all_count, 1);
    EXPECT_TRUE(controller.dirty());
}
