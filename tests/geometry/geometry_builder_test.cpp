#include "coupecad/geometry/geometry_builder.h"

#include "coupecad/core/cabinet.h"
#include "coupecad/core/commands/change_set.h"
#include "coupecad/core/hardware.h"
#include "coupecad/core/material.h"
#include "coupecad/core/panel.h"
#include "coupecad/core/project.h"
#include "coupecad/core/units.h"

#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>

#include <gtest/gtest.h>

namespace {

using namespace coupecad::core;
using coupecad::geometry::GeometryBuilder;

Project make_project_with_one_panel() {
    auto p = Project::create_empty("test", make_seeded_uuid_generator(101));
    auto& cab = p.mutable_cabinet();
    cab.dimensions = Dimensions{Millimeters{800}, Millimeters{500}, Millimeters{2000}};
    cab.default_panel_material = p.materials().begin()->first;

    Panel bottom;
    bottom.id = PanelId{p.uuid_gen().next()};
    bottom.role = PanelRole::Bottom;
    cab.panels.emplace(bottom.id, std::move(bottom));
    return p;
}

// Создаёт проект со SideLeft панелью + hardware-spec + одним HardwareItem,
// привязанным к этой панели. Возвращает пару (проект, item_id).
struct ProjectWithHardware {
    Project project;
    PanelId side_id;
    HardwareItemId item_id;
};

ProjectWithHardware make_project_with_hardware(std::uint64_t seed) {
    auto p = Project::create_empty("test", make_seeded_uuid_generator(seed));
    auto& cab = p.mutable_cabinet();
    cab.dimensions = Dimensions{Millimeters{800}, Millimeters{500}, Millimeters{2000}};
    cab.default_panel_material = p.materials().begin()->first;

    Panel side;
    side.id = PanelId{p.uuid_gen().next()};
    side.role = PanelRole::SideLeft;
    const auto side_id = side.id;
    cab.panels.emplace(side.id, std::move(side));

    HardwareRef ref{"hinge.test"};
    HardwareSpec spec;
    spec.ref = ref;
    spec.kind = HardwareKind::Hinge;
    spec.name = "Test";
    spec.bbox = Vec3{Millimeters{50}, Millimeters{30}, Millimeters{20}};
    p.mutable_hardware_catalog().emplace(ref, std::move(spec));

    HardwareItem item;
    item.id = HardwareItemId{p.uuid_gen().next()};
    item.ref = ref;
    item.attachments.push_back(
        PanelAttachment{side_id, Vec3{}, Quat::identity()});
    const auto item_id = item.id;
    cab.hardware.emplace(item.id, std::move(item));

    return ProjectWithHardware{std::move(p), side_id, item_id};
}

std::size_t count_solids(const TopoDS_Shape& s) {
    std::size_t n = 0;
    for (TopExp_Explorer ex(s, TopAbs_SOLID); ex.More(); ex.Next()) ++n;
    return n;
}

}  // namespace

TEST(GeometryBuilderTest, FreshBuilder_HasEmptyCaches) {
    auto p = make_project_with_one_panel();
    GeometryBuilder b(p);
    EXPECT_EQ(b.panel_cache_size(), 0u);
    EXPECT_EQ(b.hardware_cache_size(), 0u);
}

TEST(GeometryBuilderTest, PanelSolid_LazyBuildAndCache) {
    auto p = make_project_with_one_panel();
    GeometryBuilder b(p);

    const auto panel_id = p.cabinet().panels.begin()->first;
    EXPECT_FALSE(b.has_cached_panel(panel_id));

    const TopoDS_Solid& s1 = b.panel_solid(panel_id);
    EXPECT_TRUE(b.has_cached_panel(panel_id));
    EXPECT_EQ(b.panel_cache_size(), 1u);

    const TopoDS_Solid& s2 = b.panel_solid(panel_id);
    EXPECT_TRUE(s1.IsSame(s2));
}

TEST(GeometryBuilderTest, HardwareCompound_LazyBuildAndCache) {
    auto ph = make_project_with_hardware(303);
    GeometryBuilder b(ph.project);
    EXPECT_FALSE(b.has_cached_hardware(ph.item_id));

    const TopoDS_Compound& c1 = b.hardware_compound(ph.item_id);
    EXPECT_TRUE(b.has_cached_hardware(ph.item_id));
    EXPECT_EQ(b.hardware_cache_size(), 1u);

    const TopoDS_Compound& c2 = b.hardware_compound(ph.item_id);
    EXPECT_TRUE(c1.IsSame(c2));
}

TEST(GeometryBuilderTest, CabinetCompound_ContainsAllPanels) {
    auto p = make_project_with_one_panel();
    GeometryBuilder b(p);

    const TopoDS_Compound& c = b.cabinet_compound();
    EXPECT_EQ(c.ShapeType(), TopAbs_COMPOUND);
    EXPECT_EQ(count_solids(c), 1u);
}

TEST(GeometryBuilderTest, CabinetCompound_RepeatCallReturnsSameHandleIfNotDirty) {
    auto p = make_project_with_one_panel();
    GeometryBuilder b(p);

    const TopoDS_Compound& c1 = b.cabinet_compound();
    const TopoDS_Compound& c2 = b.cabinet_compound();
    EXPECT_TRUE(c1.IsSame(c2));
}

TEST(GeometryBuilderTest, ApplyChanges_UpdatedPanelInvalidatesOnlyThatPanel) {
    auto p = make_project_with_one_panel();
    GeometryBuilder b(p);

    const auto panel_id = p.cabinet().panels.begin()->first;
    b.panel_solid(panel_id);
    EXPECT_EQ(b.panel_cache_size(), 1u);

    ChangeSet cs;
    cs.updated_panels.push_back(panel_id);
    b.apply_changes(cs);

    EXPECT_FALSE(b.has_cached_panel(panel_id));
    EXPECT_EQ(b.panel_cache_size(), 0u);
}

TEST(GeometryBuilderTest, ApplyChanges_RemovedPanelEvictsFromCache) {
    auto p = make_project_with_one_panel();
    GeometryBuilder b(p);

    const auto panel_id = p.cabinet().panels.begin()->first;
    b.panel_solid(panel_id);

    ChangeSet cs;
    cs.removed_panels.push_back(panel_id);
    b.apply_changes(cs);

    EXPECT_FALSE(b.has_cached_panel(panel_id));
}

TEST(GeometryBuilderTest, ApplyChanges_CabinetChangedClearsAllPanelCache) {
    auto p = make_project_with_one_panel();
    GeometryBuilder b(p);

    const auto panel_id = p.cabinet().panels.begin()->first;
    b.panel_solid(panel_id);
    EXPECT_EQ(b.panel_cache_size(), 1u);

    ChangeSet cs;
    cs.cabinet_changed = true;
    b.apply_changes(cs);

    EXPECT_EQ(b.panel_cache_size(), 0u);
}

TEST(GeometryBuilderTest, ApplyChanges_UpdatedPanelAlsoClearsHardwareCache) {
    auto ph = make_project_with_hardware(404);
    GeometryBuilder b(ph.project);
    b.hardware_compound(ph.item_id);
    EXPECT_EQ(b.hardware_cache_size(), 1u);

    ChangeSet cs;
    cs.updated_panels.push_back(ph.side_id);
    b.apply_changes(cs);

    EXPECT_EQ(b.hardware_cache_size(), 0u)
        << "Updating a panel must invalidate hardware attached to it";
}

TEST(GeometryBuilderTest, ApplyChanges_EmptyChangeSetIsNoop) {
    auto p = make_project_with_one_panel();
    GeometryBuilder b(p);

    const auto panel_id = p.cabinet().panels.begin()->first;
    b.panel_solid(panel_id);
    b.cabinet_compound();
    EXPECT_EQ(b.panel_cache_size(), 1u);

    ChangeSet cs;
    b.apply_changes(cs);

    EXPECT_EQ(b.panel_cache_size(), 1u);
    const TopoDS_Compound& c1 = b.cabinet_compound();
    const TopoDS_Compound& c2 = b.cabinet_compound();
    EXPECT_TRUE(c1.IsSame(c2));
}

TEST(GeometryBuilderTest, ApplyChanges_NonEmptyDeltaMarksCompoundDirty) {
    auto p = make_project_with_one_panel();
    GeometryBuilder b(p);

    const auto panel_id = p.cabinet().panels.begin()->first;
    const TopoDS_Compound before = b.cabinet_compound();

    ChangeSet cs;
    cs.updated_panels.push_back(panel_id);
    b.apply_changes(cs);

    const TopoDS_Compound& after = b.cabinet_compound();
    EXPECT_FALSE(before.IsSame(after));
}

TEST(GeometryBuilderTest, RebuildAll_ClearsBothCachesAndDirtiesCompound) {
    auto p = make_project_with_one_panel();
    GeometryBuilder b(p);

    const auto panel_id = p.cabinet().panels.begin()->first;
    b.panel_solid(panel_id);
    b.cabinet_compound();
    EXPECT_EQ(b.panel_cache_size(), 1u);

    b.rebuild_all();

    EXPECT_EQ(b.panel_cache_size(), 0u);
    EXPECT_EQ(b.hardware_cache_size(), 0u);
    const TopoDS_Compound& after = b.cabinet_compound();
    EXPECT_EQ(after.ShapeType(), TopAbs_COMPOUND);
    EXPECT_EQ(b.panel_cache_size(), 1u) << "cabinet_compound прогревает кеш";
}

// Интеграционный тест: шкаф из 4 role-based панелей + 1 HardwareItem.
// Проверяем compound целиком через GeometryBuilder.
TEST(GeometryBuilderTest, FullCabinet_CompoundContainsAllShapes) {
    auto p = Project::create_empty("test", make_seeded_uuid_generator(505));
    auto& cab = p.mutable_cabinet();
    cab.dimensions = Dimensions{Millimeters{800}, Millimeters{500}, Millimeters{2000}};
    cab.default_panel_material = p.materials().begin()->first;

    PanelId bottom_id;
    for (auto role : {PanelRole::Bottom, PanelRole::Top,
                      PanelRole::SideLeft, PanelRole::SideRight}) {
        Panel pn;
        pn.id = PanelId{p.uuid_gen().next()};
        pn.role = role;
        if (role == PanelRole::Bottom) bottom_id = pn.id;
        cab.panels.emplace(pn.id, std::move(pn));
    }

    HardwareRef ref{"hinge.test"};
    HardwareSpec spec;
    spec.ref = ref;
    spec.kind = HardwareKind::Hinge;
    spec.name = "Test";
    spec.bbox = Vec3{Millimeters{50}, Millimeters{30}, Millimeters{20}};
    p.mutable_hardware_catalog().emplace(ref, std::move(spec));

    // Привязываем hardware к Bottom-панели детерминированно (unordered_map
    // iteration order непредсказуем, hardware на Top-панели выходил бы
    // за bbox шкафа по Z).
    HardwareItem item;
    item.id = HardwareItemId{p.uuid_gen().next()};
    item.ref = ref;
    item.attachments.push_back(
        PanelAttachment{bottom_id, Vec3{}, Quat::identity()});
    cab.hardware.emplace(item.id, std::move(item));

    GeometryBuilder b(p);
    const TopoDS_Compound& result = b.cabinet_compound();

    EXPECT_EQ(result.ShapeType(), TopAbs_COMPOUND);
    EXPECT_EQ(count_solids(result), 5u) << "4 panels + 1 hardware solid";

    Bnd_Box bbox;
    BRepBndLib::Add(result, bbox);
    double xmin, ymin, zmin, xmax, ymax, zmax;
    bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);
    EXPECT_NEAR(xmin, 0.0,    1e-6);
    EXPECT_NEAR(xmax, 800.0,  1e-6);
    EXPECT_NEAR(zmin, 0.0,    1e-6);
    EXPECT_NEAR(zmax, 2000.0, 1e-6);
}
