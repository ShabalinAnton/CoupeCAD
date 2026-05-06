#include "coupecad/renderer/occt/occt_renderer.h"

#include "coupecad/core/cabinet.h"
#include "coupecad/core/commands/change_set.h"
#include "coupecad/core/hardware.h"
#include "coupecad/core/panel.h"
#include "coupecad/core/project.h"
#include "coupecad/geometry/geometry_builder.h"

#include <Quantity_Color.hxx>
#include <gtest/gtest.h>

using coupecad::core::ChangeSet;
using coupecad::core::Project;
using coupecad::geometry::GeometryBuilder;
using coupecad::renderer::occt::OcctRenderer;

namespace {

struct Fixture {
    Project project = Project::create_empty("sync");
    GeometryBuilder builder{project};
    OcctRenderer renderer{project, builder};

    Fixture() {
        auto& cabinet = project.mutable_cabinet();
        cabinet.dimensions = {coupecad::core::Millimeters{1000},
                              coupecad::core::Millimeters{500},
                              coupecad::core::Millimeters{1500}};
    }

    coupecad::core::PanelId add_panel_to_cabinet() {
        auto& cabinet = project.mutable_cabinet();
        coupecad::core::Panel p;
        p.id = project.uuid_gen().next_id<coupecad::core::PanelIdTag>();
        p.role = coupecad::core::PanelRole::Bottom;
        cabinet.panels.emplace(p.id, p);
        return p.id;
    }
};

}  // namespace

TEST(OcctRendererSyncTest, AddedPanelEntersScene) {
    Fixture f;
    const auto pid = f.add_panel_to_cabinet();
    ChangeSet cs;
    cs.added_panels.push_back(pid);

    f.renderer.sync(cs);

    EXPECT_TRUE(f.renderer.scene().has_panel(pid));
    EXPECT_EQ(f.renderer.scene().panel_count(), 1u);
}

TEST(OcctRendererSyncTest, RemovedPanelLeavesScene) {
    Fixture f;
    const auto pid = f.add_panel_to_cabinet();
    ChangeSet add; add.added_panels.push_back(pid);
    f.renderer.sync(add);

    f.project.mutable_cabinet().panels.erase(pid);
    ChangeSet remove; remove.removed_panels.push_back(pid);
    f.renderer.sync(remove);

    EXPECT_FALSE(f.renderer.scene().has_panel(pid));
}

TEST(OcctRendererSyncTest, UpdatedPanelKeepsIdSwapsHandle) {
    Fixture f;
    const auto pid = f.add_panel_to_cabinet();
    ChangeSet add; add.added_panels.push_back(pid);
    f.renderer.sync(add);
    const auto* before = f.renderer.scene().raw_ais_pointer_for_panel(pid);

    f.project.mutable_cabinet().dimensions.width = coupecad::core::Millimeters{2000};
    f.builder.rebuild_all();
    ChangeSet update; update.updated_panels.push_back(pid);
    f.renderer.sync(update);

    const auto* after = f.renderer.scene().raw_ais_pointer_for_panel(pid);
    EXPECT_TRUE(f.renderer.scene().has_panel(pid));
    EXPECT_NE(before, after);
}

TEST(OcctRendererSyncTest, CabinetChangedReplacesAllExistingPanels) {
    Fixture f;
    const auto p1 = f.add_panel_to_cabinet();
    const auto p2 = f.add_panel_to_cabinet();
    ChangeSet add; add.added_panels = {p1, p2};
    f.renderer.sync(add);

    const auto* before1 = f.renderer.scene().raw_ais_pointer_for_panel(p1);
    const auto* before2 = f.renderer.scene().raw_ais_pointer_for_panel(p2);

    f.project.mutable_cabinet().dimensions.width = coupecad::core::Millimeters{3000};
    f.builder.rebuild_all();
    ChangeSet cab; cab.cabinet_changed = true;
    f.renderer.sync(cab);

    EXPECT_NE(before1, f.renderer.scene().raw_ais_pointer_for_panel(p1));
    EXPECT_NE(before2, f.renderer.scene().raw_ais_pointer_for_panel(p2));
}

TEST(OcctRendererSyncTest, MaterialDeltaRefreshesColors) {
    Fixture f;
    const auto pid = f.add_panel_to_cabinet();
    ChangeSet add; add.added_panels.push_back(pid);
    f.renderer.sync(add);

    auto& cabinet = f.project.mutable_cabinet();
    auto& materials = f.project.mutable_materials();
    materials.at(cabinet.default_panel_material).color_hint =
        coupecad::core::RGBA{0, 255, 0, 255};

    ChangeSet upd; upd.updated_materials.push_back(cabinet.default_panel_material);
    f.renderer.sync(upd);

    Quantity_Color c;
    f.renderer.scene().context()->Color(
        f.renderer.scene().raw_ais_handle_for_panel(pid), c);
    EXPECT_NEAR(c.Green(), 1.0, 1e-3);
}

TEST(OcctRendererSyncTest, EmptyChangeSetIsNoOp) {
    Fixture f;
    const auto pid = f.add_panel_to_cabinet();
    ChangeSet add; add.added_panels.push_back(pid);
    f.renderer.sync(add);

    const auto* before = f.renderer.scene().raw_ais_pointer_for_panel(pid);
    f.renderer.sync(ChangeSet{});
    EXPECT_EQ(before, f.renderer.scene().raw_ais_pointer_for_panel(pid));
}

TEST(OcctRendererSyncTest, RebuildAllClearsAndRepopulates) {
    Fixture f;
    const auto pid = f.add_panel_to_cabinet();
    ChangeSet add; add.added_panels.push_back(pid);
    f.renderer.sync(add);

    f.builder.rebuild_all();
    f.renderer.rebuild_all();

    EXPECT_TRUE(f.renderer.scene().has_panel(pid));
    EXPECT_EQ(f.renderer.scene().panel_count(), 1u);
}
