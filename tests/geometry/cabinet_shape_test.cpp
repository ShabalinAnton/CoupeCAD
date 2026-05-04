#include "coupecad/geometry/cabinet_shape.h"
#include "coupecad/geometry/hardware_shape.h"
#include "coupecad/geometry/panel_shape.h"

#include "coupecad/core/cabinet.h"
#include "coupecad/core/material.h"
#include "coupecad/core/panel.h"
#include "coupecad/core/project.h"
#include "coupecad/core/units.h"

#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>

#include <gtest/gtest.h>

#include <unordered_map>

namespace {

using namespace coupecad::core;
using coupecad::geometry::build_cabinet_compound;
using coupecad::geometry::build_panel_solid;

Project make_project_with_two_panels() {
    auto p = Project::create_empty("test", make_seeded_uuid_generator(31));
    auto& cab = p.mutable_cabinet();
    cab.dimensions = Dimensions{Millimeters{800}, Millimeters{500}, Millimeters{2000}};
    cab.default_panel_material = p.materials().begin()->first;

    Panel bottom;
    bottom.id = PanelId{p.uuid_gen().next()};
    bottom.role = PanelRole::Bottom;
    cab.panels.emplace(bottom.id, std::move(bottom));

    Panel top;
    top.id = PanelId{p.uuid_gen().next()};
    top.role = PanelRole::Top;
    cab.panels.emplace(top.id, std::move(top));
    return p;
}

std::size_t count_subshapes(const TopoDS_Compound& c, TopAbs_ShapeEnum kind) {
    std::size_t n = 0;
    for (TopExp_Explorer ex(c, kind); ex.More(); ex.Next()) ++n;
    return n;
}

}  // namespace

TEST(CabinetShapeTest, EmptyCabinet_EmptyCompound) {
    auto p = Project::create_empty("test", make_seeded_uuid_generator(1));
    auto& cab = p.mutable_cabinet();
    cab.dimensions = Dimensions{Millimeters{800}, Millimeters{500}, Millimeters{2000}};
    cab.default_panel_material = p.materials().begin()->first;

    std::unordered_map<PanelId, TopoDS_Solid> panels;
    std::unordered_map<HardwareItemId, TopoDS_Compound> hardware;
    auto get_p = [&](const PanelId& id) -> const TopoDS_Solid& { return panels.at(id); };
    auto get_h = [&](const HardwareItemId& id) -> const TopoDS_Compound& {
        return hardware.at(id);
    };

    const TopoDS_Compound result = build_cabinet_compound(p, get_p, get_h);
    EXPECT_EQ(result.ShapeType(), TopAbs_COMPOUND);
    EXPECT_EQ(count_subshapes(result, TopAbs_SOLID), 0u);
}

TEST(CabinetShapeTest, TwoPanels_TwoSolidsInCompound) {
    auto p = make_project_with_two_panels();

    std::unordered_map<PanelId, TopoDS_Solid> panels;
    for (const auto& [id, panel] : p.cabinet().panels) {
        panels.emplace(id, build_panel_solid(p.cabinet(), panel));
    }
    std::unordered_map<HardwareItemId, TopoDS_Compound> hardware;

    auto get_p = [&](const PanelId& id) -> const TopoDS_Solid& { return panels.at(id); };
    auto get_h = [&](const HardwareItemId& id) -> const TopoDS_Compound& {
        return hardware.at(id);
    };

    const TopoDS_Compound result = build_cabinet_compound(p, get_p, get_h);
    EXPECT_EQ(count_subshapes(result, TopAbs_SOLID), 2u);
}

TEST(CabinetShapeTest, BoundingBoxIsUnionOfPanelBoxes) {
    auto p = make_project_with_two_panels();

    std::unordered_map<PanelId, TopoDS_Solid> panels;
    for (const auto& [id, panel] : p.cabinet().panels) {
        panels.emplace(id, build_panel_solid(p.cabinet(), panel));
    }
    std::unordered_map<HardwareItemId, TopoDS_Compound> hardware;

    auto get_p = [&](const PanelId& id) -> const TopoDS_Solid& { return panels.at(id); };
    auto get_h = [&](const HardwareItemId& id) -> const TopoDS_Compound& {
        return hardware.at(id);
    };

    const TopoDS_Compound result = build_cabinet_compound(p, get_p, get_h);
    Bnd_Box bbox;
    BRepBndLib::Add(result, bbox);
    double xmin, ymin, zmin, xmax, ymax, zmax;
    bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);

    // Bottom: z in [0, 16]; Top: z in [1984, 2000]. Union: z in [0, 2000].
    EXPECT_NEAR(zmin, 0.0,    1e-6);
    EXPECT_NEAR(zmax, 2000.0, 1e-6);
    EXPECT_NEAR(xmin, 0.0,    1e-6);
    EXPECT_NEAR(xmax, 800.0,  1e-6);
}
