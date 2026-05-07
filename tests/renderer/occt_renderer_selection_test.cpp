#include "coupecad/renderer/occt/occt_renderer.h"

#include "coupecad/core/errors.h"
#include "coupecad/core/panel.h"
#include "coupecad/core/project.h"
#include "coupecad/geometry/geometry_builder.h"

#include <gtest/gtest.h>

#include <algorithm>

using coupecad::core::Millimeters;
using coupecad::core::Project;
using coupecad::geometry::GeometryBuilder;
using coupecad::renderer::EntityId;
using coupecad::renderer::occt::OcctRenderer;

namespace {

struct Fixture {
    Project project = Project::create_empty("sel");
    GeometryBuilder builder{project};
    OcctRenderer renderer{project, builder};

    Fixture() {
        auto& cab = project.mutable_cabinet();
        cab.dimensions = {Millimeters{1000}, Millimeters{500}, Millimeters{1500}};
    }

    coupecad::core::PanelId add_panel() {
        coupecad::core::Panel p;
        p.id = project.uuid_gen().next_id<coupecad::core::PanelIdTag>();
        p.role = coupecad::core::PanelRole::Bottom;
        project.mutable_cabinet().panels.emplace(p.id, p);
        coupecad::core::ChangeSet cs;
        cs.added_panels.push_back(p.id);
        renderer.sync(cs);
        return p.id;
    }
};

}  // namespace

TEST(OcctRendererSelectionTest, EmptySelectionInitially) {
    Fixture f;
    EXPECT_TRUE(f.renderer.selection().empty());
}

TEST(OcctRendererSelectionTest, SelectAddsToSelection) {
    Fixture f;
    const auto pid = f.add_panel();
    f.renderer.select(EntityId{pid});
    auto sel = f.renderer.selection();
    ASSERT_EQ(sel.size(), 1u);
    EXPECT_EQ(sel[0], EntityId{pid});
}

TEST(OcctRendererSelectionTest, DeselectRemovesFromSelection) {
    Fixture f;
    const auto pid = f.add_panel();
    f.renderer.select(EntityId{pid});
    f.renderer.deselect(EntityId{pid});
    EXPECT_TRUE(f.renderer.selection().empty());
}

TEST(OcctRendererSelectionTest, ClearSelectionEmptiesSet) {
    Fixture f;
    const auto p1 = f.add_panel();
    const auto p2 = f.add_panel();
    f.renderer.select(EntityId{p1});
    f.renderer.select(EntityId{p2});
    f.renderer.clear_selection();
    EXPECT_TRUE(f.renderer.selection().empty());
}

TEST(OcctRendererSelectionTest, SelectUnknownThrowsDomainError) {
    Fixture f;
    auto missing = coupecad::core::PanelId::from_string(
        "00000000-0000-0000-0000-000000000099");
    try {
        f.renderer.select(EntityId{missing});
        FAIL() << "expected DomainError";
    } catch (const coupecad::core::DomainError& e) {
        EXPECT_EQ(e.code(), "renderer.unknown_entity_in_selection");
    }
}

TEST(OcctRendererSelectionTest, RemovingSelectedPanelDropsItFromSelection) {
    Fixture f;
    const auto pid = f.add_panel();
    f.renderer.select(EntityId{pid});

    f.project.mutable_cabinet().panels.erase(pid);
    coupecad::core::ChangeSet rm;
    rm.removed_panels.push_back(pid);
    f.renderer.sync(rm);

    EXPECT_TRUE(f.renderer.selection().empty());
}
