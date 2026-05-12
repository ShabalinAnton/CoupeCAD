#include "coupecad/viewport/viewport_controller.h"

#include "coupecad/core/project.h"
#include "coupecad/geometry/geometry_builder.h"
#include "fake_renderer.h"

#include <gtest/gtest.h>

using coupecad::core::Project;
using coupecad::geometry::GeometryBuilder;
using coupecad::viewport::ViewportController;
using coupecad::viewport::testing::FakeRenderer;

TEST(ViewportControllerTest, DirtyFlagInitiallyTrue) {
    Project project = Project::create_empty("vc1");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    ViewportController controller{project, builder, fake};

    EXPECT_TRUE(controller.dirty());
}

TEST(ViewportControllerTest, MarkCleanClearsDirty) {
    Project project = Project::create_empty("vc2");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    ViewportController controller{project, builder, fake};

    controller.mark_clean();
    EXPECT_FALSE(controller.dirty());
}

TEST(ViewportControllerTest, SelectionEmptyOnNewController) {
    Project project = Project::create_empty("vc3");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    ViewportController controller{project, builder, fake};

    EXPECT_TRUE(controller.selection_empty());
    EXPECT_EQ(controller.selection_count(), 0u);
}

TEST(ViewportControllerTest, SetViewportSizeForwardsToRenderer) {
    Project project = Project::create_empty("vc4");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    ViewportController controller{project, builder, fake};

    controller.set_viewport_size(1920, 1080);
    EXPECT_EQ(fake.last_set_viewport.width, 1920);
    EXPECT_EQ(fake.last_set_viewport.height, 1080);
    EXPECT_TRUE(controller.dirty());
}

TEST(ViewportControllerTest, MousePressDoesNotCallRendererBeforeRelease) {
    Project project = Project::create_empty("vc5");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    ViewportController controller{project, builder, fake};

    controller.on_mouse_press(100, 100, coupecad::viewport::MouseButton::Right);
    EXPECT_TRUE(fake.set_camera_calls.empty());
    EXPECT_EQ(fake.clear_selection_count, 0);
}

TEST(ViewportControllerTest, MouseMoveWithMismatchedButtonIsNoOp) {
    Project project = Project::create_empty("vc6");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    ViewportController controller{project, builder, fake};

    // No press first; spurious move should be ignored.
    controller.on_mouse_move(120, 100, coupecad::viewport::MouseButton::Left);
    EXPECT_TRUE(fake.set_camera_calls.empty());
}

TEST(ViewportControllerTest, MouseReleaseClearsDragState) {
    Project project = Project::create_empty("vc7");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    ViewportController controller{project, builder, fake};

    controller.on_mouse_press(100, 100, coupecad::viewport::MouseButton::Left);
    controller.on_mouse_release(100, 100, coupecad::viewport::MouseButton::Left);
    // After release, a fresh move with same button should not orbit
    // (drag is no longer active).
    fake.set_camera_calls.clear();
    controller.on_mouse_move(150, 100, coupecad::viewport::MouseButton::Left);
    EXPECT_TRUE(fake.set_camera_calls.empty());
}
