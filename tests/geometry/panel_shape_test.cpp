#include "coupecad/geometry/panel_shape.h"

#include "coupecad/core/cabinet.h"
#include "coupecad/core/id.h"
#include "coupecad/core/material.h"
#include "coupecad/core/panel.h"
#include "coupecad/core/units.h"

#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <TopAbs_ShapeEnum.hxx>

#include <gtest/gtest.h>

namespace {

using coupecad::core::Cabinet;
using coupecad::core::CabinetId;
using coupecad::core::CustomParams;
using coupecad::core::Dimensions;
using coupecad::core::DividerHorizontalParams;
using coupecad::core::DividerVerticalParams;
using coupecad::core::DrawerBackParams;
using coupecad::core::DrawerBottomParams;
using coupecad::core::DrawerFrontParams;
using coupecad::core::DrawerSideParams;
using coupecad::core::FacadeParams;
using coupecad::core::make_seeded_uuid_generator;
using coupecad::core::MaterialId;
using coupecad::core::Millimeters;
using coupecad::core::Panel;
using coupecad::core::PanelId;
using coupecad::core::PanelRole;
using coupecad::core::PlinthParams;
using coupecad::core::Quat;
using coupecad::core::ShelfFullWidth;
using coupecad::core::ShelfParams;
using coupecad::core::Vec3;
using coupecad::geometry::build_panel_solid;

// Минимальная фабрика стандартного шкафа 800×500×2000, толщина панелей 16.
Cabinet make_cabinet() {
    auto gen = make_seeded_uuid_generator(42);
    Cabinet c;
    c.id = CabinetId{gen->next()};
    c.name = "Test cabinet";
    c.dimensions = Dimensions{Millimeters{800}, Millimeters{500}, Millimeters{2000}};
    c.default_panel_material = MaterialId{gen->next()};
    c.default_panel_thickness = Millimeters{16};
    c.default_back_thickness = Millimeters{4};
    return c;
}

Panel make_role_panel(PanelRole role) {
    auto gen = make_seeded_uuid_generator(7);
    Panel p;
    p.id = PanelId{gen->next()};
    p.role = role;
    return p;
}

}  // namespace

TEST(PanelShapeTest, Bottom_BBoxAndVolume) {
    const Cabinet c = make_cabinet();
    const Panel p = make_role_panel(PanelRole::Bottom);
    const TopoDS_Solid solid = build_panel_solid(c, p);

    EXPECT_EQ(solid.ShapeType(), TopAbs_SOLID);

    Bnd_Box bbox;
    BRepBndLib::Add(solid, bbox);
    double xmin, ymin, zmin, xmax, ymax, zmax;
    bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);

    // Bottom: origin (0,0,0), size = (W, D - t_back, t) = (800, 496, 16).
    EXPECT_NEAR(xmin, 0.0,   1e-6);
    EXPECT_NEAR(ymin, 0.0,   1e-6);
    EXPECT_NEAR(zmin, 0.0,   1e-6);
    EXPECT_NEAR(xmax, 800.0, 1e-6);
    EXPECT_NEAR(ymax, 496.0, 1e-6);
    EXPECT_NEAR(zmax, 16.0,  1e-6);

    GProp_GProps vp;
    BRepGProp::VolumeProperties(solid, vp);
    EXPECT_NEAR(vp.Mass(), 800.0 * 496.0 * 16.0, 1e-3);
}

TEST(PanelShapeTest, Top_OriginIsAtCabinetTop) {
    const Cabinet c = make_cabinet();
    const Panel p = make_role_panel(PanelRole::Top);
    const TopoDS_Solid solid = build_panel_solid(c, p);

    Bnd_Box bbox;
    BRepBndLib::Add(solid, bbox);
    double xmin, ymin, zmin, xmax, ymax, zmax;
    bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);

    // Top: origin = (0, 0, H - t) = (0, 0, 1984), size = (800, 496, 16).
    EXPECT_NEAR(zmin, 1984.0, 1e-6);
    EXPECT_NEAR(zmax, 2000.0, 1e-6);
}

TEST(PanelShapeTest, SideLeft_OccupiesFullHeight) {
    const Cabinet c = make_cabinet();
    const Panel p = make_role_panel(PanelRole::SideLeft);
    const TopoDS_Solid solid = build_panel_solid(c, p);

    Bnd_Box bbox;
    BRepBndLib::Add(solid, bbox);
    double xmin, ymin, zmin, xmax, ymax, zmax;
    bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);

    // SideLeft: origin (0,0,0), size = (t, D - t_back, H) = (16, 496, 2000).
    EXPECT_NEAR(xmax - xmin, 16.0,   1e-6);
    EXPECT_NEAR(ymax - ymin, 496.0,  1e-6);
    EXPECT_NEAR(zmax - zmin, 2000.0, 1e-6);
}

// Параметризованный smoke-тест: для каждой роли без обязательных RoleParams
// проверяем, что Stage 2-обёртка возвращает валидный непустой solid.
class PanelShapeAllRolesTest : public ::testing::TestWithParam<PanelRole> {};

TEST_P(PanelShapeAllRolesTest, ProducesValidNonEmptySolid) {
    const Cabinet c = make_cabinet();
    const Panel p = make_role_panel(GetParam());

    const TopoDS_Solid solid = build_panel_solid(c, p);

    EXPECT_EQ(solid.ShapeType(), TopAbs_SOLID);

    GProp_GProps vp;
    BRepGProp::VolumeProperties(solid, vp);
    EXPECT_GT(vp.Mass(), 0.0);
}

INSTANTIATE_TEST_SUITE_P(
    NoParamsRoles, PanelShapeAllRolesTest,
    ::testing::Values(
        PanelRole::Top,
        PanelRole::Bottom,
        PanelRole::SideLeft,
        PanelRole::SideRight,
        PanelRole::Back
    ));

TEST(PanelShapeTest, Shelf_AtMiddleHeight_HasCorrectZ) {
    Cabinet c = make_cabinet();
    Panel p = make_role_panel(PanelRole::Shelf);
    p.role_params = ShelfParams{Millimeters{1000}, ShelfFullWidth{}};

    const TopoDS_Solid solid = build_panel_solid(c, p);

    Bnd_Box bbox;
    BRepBndLib::Add(solid, bbox);
    double xmin, ymin, zmin, xmax, ymax, zmax;
    bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);

    // Shelf: z от height_from_bottom до height_from_bottom + default_panel_thickness.
    EXPECT_NEAR(zmin, 1000.0, 1e-6);
    EXPECT_NEAR(zmax, 1016.0, 1e-6);
}

// --- Smoke tests для остальных ролей с обязательными RoleParams ---
// Spec §11 требует покрытия всех 15 ролей. Top/Bottom/SideLeft/SideRight/Back
// покрыты PanelShapeAllRolesTest, Shelf — отдельным тестом выше. Ниже — оставшиеся 9.

TEST(PanelShapeTest, DividerVertical_ProducesValidSolid) {
    Cabinet c = make_cabinet();
    Panel p = make_role_panel(PanelRole::DividerVertical);
    p.role_params = DividerVerticalParams{Millimeters{400}};

    const TopoDS_Solid solid = build_panel_solid(c, p);

    EXPECT_EQ(solid.ShapeType(), TopAbs_SOLID);
    GProp_GProps vp;
    BRepGProp::VolumeProperties(solid, vp);
    EXPECT_GT(vp.Mass(), 0.0);
}

TEST(PanelShapeTest, DividerHorizontal_ProducesValidSolid) {
    Cabinet c = make_cabinet();
    Panel p = make_role_panel(PanelRole::DividerHorizontal);
    p.role_params = DividerHorizontalParams{Millimeters{500}};

    const TopoDS_Solid solid = build_panel_solid(c, p);

    EXPECT_EQ(solid.ShapeType(), TopAbs_SOLID);
    GProp_GProps vp;
    BRepGProp::VolumeProperties(solid, vp);
    EXPECT_GT(vp.Mass(), 0.0);
}

TEST(PanelShapeTest, Facade_ProducesValidSolid) {
    Cabinet c = make_cabinet();
    Panel p = make_role_panel(PanelRole::Facade);
    p.role_params = FacadeParams{};

    const TopoDS_Solid solid = build_panel_solid(c, p);

    EXPECT_EQ(solid.ShapeType(), TopAbs_SOLID);
    GProp_GProps vp;
    BRepGProp::VolumeProperties(solid, vp);
    EXPECT_GT(vp.Mass(), 0.0);
}

TEST(PanelShapeTest, DrawerBottom_ProducesValidSolid) {
    Cabinet c = make_cabinet();
    Panel p = make_role_panel(PanelRole::DrawerBottom);
    p.role_params = DrawerBottomParams{Millimeters{200}, Millimeters{450}};

    const TopoDS_Solid solid = build_panel_solid(c, p);

    EXPECT_EQ(solid.ShapeType(), TopAbs_SOLID);
    GProp_GProps vp;
    BRepGProp::VolumeProperties(solid, vp);
    EXPECT_GT(vp.Mass(), 0.0);
}

TEST(PanelShapeTest, DrawerFront_ProducesValidSolid) {
    Cabinet c = make_cabinet();
    Panel p = make_role_panel(PanelRole::DrawerFront);
    p.role_params = DrawerFrontParams{Millimeters{200}, Millimeters{180}};

    const TopoDS_Solid solid = build_panel_solid(c, p);

    EXPECT_EQ(solid.ShapeType(), TopAbs_SOLID);
    GProp_GProps vp;
    BRepGProp::VolumeProperties(solid, vp);
    EXPECT_GT(vp.Mass(), 0.0);
}

TEST(PanelShapeTest, DrawerSide_ProducesValidSolid) {
    Cabinet c = make_cabinet();
    Panel p = make_role_panel(PanelRole::DrawerSide);
    p.role_params = DrawerSideParams{Millimeters{200}, Millimeters{180},
                                     Millimeters{450},
                                     DrawerSideParams::Side::Left};

    const TopoDS_Solid solid = build_panel_solid(c, p);

    EXPECT_EQ(solid.ShapeType(), TopAbs_SOLID);
    GProp_GProps vp;
    BRepGProp::VolumeProperties(solid, vp);
    EXPECT_GT(vp.Mass(), 0.0);
}

TEST(PanelShapeTest, DrawerBack_ProducesValidSolid) {
    Cabinet c = make_cabinet();
    Panel p = make_role_panel(PanelRole::DrawerBack);
    p.role_params = DrawerBackParams{Millimeters{200}, Millimeters{180}};

    const TopoDS_Solid solid = build_panel_solid(c, p);

    EXPECT_EQ(solid.ShapeType(), TopAbs_SOLID);
    GProp_GProps vp;
    BRepGProp::VolumeProperties(solid, vp);
    EXPECT_GT(vp.Mass(), 0.0);
}

TEST(PanelShapeTest, Plinth_ProducesValidSolid) {
    Cabinet c = make_cabinet();
    Panel p = make_role_panel(PanelRole::Plinth);
    p.role_params = PlinthParams{Millimeters{100}, Millimeters{50}};

    const TopoDS_Solid solid = build_panel_solid(c, p);

    EXPECT_EQ(solid.ShapeType(), TopAbs_SOLID);
    GProp_GProps vp;
    BRepGProp::VolumeProperties(solid, vp);
    EXPECT_GT(vp.Mass(), 0.0);
}

TEST(PanelShapeTest, Custom_AxisAligned_BBoxMatchesParams) {
    Cabinet c = make_cabinet();
    Panel p = make_role_panel(PanelRole::Custom);
    p.role_params = CustomParams{
        Vec3{Millimeters{100}, Millimeters{50}, Millimeters{200}},
        Vec3{Millimeters{300}, Millimeters{40}, Millimeters{600}},
        Quat::identity()};

    const TopoDS_Solid solid = build_panel_solid(c, p);

    Bnd_Box bbox;
    BRepBndLib::Add(solid, bbox);
    double xmin, ymin, zmin, xmax, ymax, zmax;
    bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);
    EXPECT_NEAR(xmin, 100.0, 1e-6);
    EXPECT_NEAR(ymin, 50.0,  1e-6);
    EXPECT_NEAR(zmin, 200.0, 1e-6);
    EXPECT_NEAR(xmax, 400.0, 1e-6);
    EXPECT_NEAR(ymax, 90.0,  1e-6);
    EXPECT_NEAR(zmax, 800.0, 1e-6);
}

// Spec §10 risk #4: тест с произвольной ориентацией Custom-панели.
// Поворот 90° вокруг Z вокруг origin: размер 100×40×200, position (200,100,0).
// После поворота 90° по Z box (100×40×200) занимает диапазон x:[-40,0], y:[0,100],
// затем сдвиг на (200,100,0) даёт bbox x:[160,200], y:[100,200], z:[0,200].
TEST(PanelShapeTest, Custom_RotatedAroundZ_BBoxIsRotatedThenTranslated) {
    Cabinet c = make_cabinet();
    Panel p = make_role_panel(PanelRole::Custom);
    // Quat (w,x,y,z) для поворота 90° вокруг Z = (cos45°, 0, 0, sin45°).
    constexpr double s = 0.70710678118654752440;
    p.role_params = CustomParams{
        Vec3{Millimeters{200}, Millimeters{100}, Millimeters{0}},
        Vec3{Millimeters{100}, Millimeters{40}, Millimeters{200}},
        Quat{s, 0.0, 0.0, s}};

    const TopoDS_Solid solid = build_panel_solid(c, p);

    EXPECT_EQ(solid.ShapeType(), TopAbs_SOLID);
    Bnd_Box bbox;
    BRepBndLib::Add(solid, bbox);
    double xmin, ymin, zmin, xmax, ymax, zmax;
    bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);
    EXPECT_NEAR(xmin, 160.0, 1e-6);
    EXPECT_NEAR(xmax, 200.0, 1e-6);
    EXPECT_NEAR(ymin, 100.0, 1e-6);
    EXPECT_NEAR(ymax, 200.0, 1e-6);
    EXPECT_NEAR(zmin, 0.0,   1e-6);
    EXPECT_NEAR(zmax, 200.0, 1e-6);

    GProp_GProps vp;
    BRepGProp::VolumeProperties(solid, vp);
    EXPECT_NEAR(vp.Mass(), 100.0 * 40.0 * 200.0, 1e-3);
}
