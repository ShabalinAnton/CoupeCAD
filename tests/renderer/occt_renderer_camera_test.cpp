#include "coupecad/renderer/occt/occt_renderer.h"

#include "coupecad/core/panel.h"
#include "coupecad/core/project.h"
#include "coupecad/geometry/geometry_builder.h"

#include <gtest/gtest.h>

using coupecad::core::Millimeters;
using coupecad::core::Project;
using coupecad::geometry::GeometryBuilder;
using coupecad::renderer::CameraState;
using coupecad::renderer::occt::OcctRenderer;

TEST(OcctRendererCameraTest, SetThenGetReturnsApprox) {
    Project project = Project::create_empty("cam");
    GeometryBuilder builder{project};
    OcctRenderer renderer{project, builder};

    CameraState in;
    in.eye    = {Millimeters{1000}, Millimeters{-1000}, Millimeters{500}};
    in.target = {Millimeters{0},    Millimeters{0},     Millimeters{0}};
    in.up     = {Millimeters{0},    Millimeters{0},     Millimeters{1}};
    in.fov_deg = 60.0;

    renderer.set_camera(in);
    const CameraState out = renderer.camera();

    EXPECT_EQ(out.eye, in.eye);
    EXPECT_EQ(out.target, in.target);
    EXPECT_NEAR(out.fov_deg, in.fov_deg, 1e-3);
}

TEST(OcctRendererCameraTest, OrthographicWhenFovZero) {
    Project project = Project::create_empty("cam2");
    GeometryBuilder builder{project};
    OcctRenderer renderer{project, builder};

    CameraState in;
    in.eye    = {Millimeters{0}, Millimeters{-1000}, Millimeters{0}};
    in.target = {Millimeters{0}, Millimeters{0},      Millimeters{0}};
    in.fov_deg = 0.0;

    renderer.set_camera(in);
    EXPECT_NEAR(renderer.camera().fov_deg, 0.0, 1e-9);
}

TEST(OcctRendererCameraTest, FitAllDoesNotThrowOnEmptyScene) {
    Project project = Project::create_empty("cam3");
    GeometryBuilder builder{project};
    OcctRenderer renderer{project, builder};
    EXPECT_NO_THROW(renderer.fit_all());
}
