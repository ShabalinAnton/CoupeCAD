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
