#include "coupecad/viewport/viewport_controller.h"

#include "coupecad/core/id.h"
#include "coupecad/core/project.h"
#include "coupecad/geometry/geometry_builder.h"
#include "coupecad/renderer/entity_id.h"
#include "fake_renderer.h"

#include <gtest/gtest.h>

using coupecad::core::PanelId;
using coupecad::core::PanelIdTag;
using coupecad::core::Project;
using coupecad::geometry::GeometryBuilder;
using coupecad::renderer::EntityId;
using coupecad::viewport::MouseButton;
using coupecad::viewport::ViewportController;
using coupecad::viewport::testing::FakeRenderer;

TEST(ViewportControllerPickTest, RmbClickWithHitSelectsId) {
    Project project = Project::create_empty("pick1");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    auto pid = project.uuid_gen().next_id<PanelIdTag>();
    fake.next_pick_result = EntityId{pid};

    ViewportController controller{project, builder, fake};
    controller.on_mouse_press(100, 100, MouseButton::Right);
    controller.on_mouse_release(100, 100, MouseButton::Right);

    EXPECT_EQ(fake.clear_selection_count, 1);
    ASSERT_EQ(fake.select_calls.size(), 1u);
    EXPECT_EQ(fake.select_calls[0], EntityId{pid});
}

TEST(ViewportControllerPickTest, RmbClickWithMissOnlyClears) {
    Project project = Project::create_empty("pick2");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    fake.next_pick_result = std::nullopt;

    ViewportController controller{project, builder, fake};
    controller.on_mouse_press(100, 100, MouseButton::Right);
    controller.on_mouse_release(100, 100, MouseButton::Right);

    EXPECT_EQ(fake.clear_selection_count, 1);
    EXPECT_TRUE(fake.select_calls.empty());
}

TEST(ViewportControllerPickTest, RmbDragDoesNotPick) {
    Project project = Project::create_empty("pick3");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    auto pid = project.uuid_gen().next_id<PanelIdTag>();
    fake.next_pick_result = EntityId{pid};

    ViewportController controller{project, builder, fake};
    controller.on_mouse_press(100, 100, MouseButton::Right);
    controller.on_mouse_move(110, 100, MouseButton::Right);   // +10px > threshold
    controller.on_mouse_release(110, 100, MouseButton::Right);

    EXPECT_EQ(fake.clear_selection_count, 0);
    EXPECT_TRUE(fake.select_calls.empty());
}
