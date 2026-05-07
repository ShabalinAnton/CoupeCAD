#include "coupecad/viewport/viewport_controller.h"

#include "coupecad/core/cabinet.h"
#include "coupecad/core/commands/change_set.h"
#include "coupecad/core/panel.h"
#include "coupecad/core/project.h"
#include "coupecad/geometry/geometry_builder.h"
#include "fake_renderer.h"

#include <gtest/gtest.h>

using coupecad::core::ChangeSet;
using coupecad::core::Project;
using coupecad::geometry::GeometryBuilder;
using coupecad::viewport::ViewportController;
using coupecad::viewport::testing::FakeRenderer;

TEST(ViewportControllerObserverTest, OnChangedForwardsToRenderer) {
    Project project = Project::create_empty("obs1");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    ViewportController controller{project, builder, fake};

    auto pid = project.uuid_gen().next_id<coupecad::core::PanelIdTag>();
    ChangeSet cs;
    cs.added_panels.push_back(pid);

    controller.on_changed(project, cs);

    ASSERT_EQ(fake.sync_calls.size(), 1u);
    EXPECT_EQ(fake.sync_calls[0].added_panels.size(), 1u);
    EXPECT_EQ(fake.sync_calls[0].added_panels[0], pid);
    EXPECT_TRUE(controller.dirty());
}

TEST(ViewportControllerObserverTest, RebuildFromScratchCallsRebuildAndFitAll) {
    Project project = Project::create_empty("obs2");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    ViewportController controller{project, builder, fake};

    controller.mark_clean();
    controller.rebuild_from_scratch();

    EXPECT_EQ(fake.rebuild_all_count, 1);
    EXPECT_EQ(fake.fit_all_count, 1);
    EXPECT_TRUE(controller.dirty());
}
