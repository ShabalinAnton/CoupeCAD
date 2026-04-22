#include "coupecad/core/geometry.h"
#include "coupecad/core/project.h"
#include "coupecad/core/errors.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

namespace {

// Тестовый шкаф 2400×600×2400 с толщиной корпуса 16 и спинки 4.
struct TestProject {
    Project project = Project::create_empty(
        "T", make_seeded_uuid_generator(100));
    UuidGenerator& gen() { return project.uuid_gen(); }
    Cabinet& cab() { return project.mutable_cabinet(); }
};

PanelId add_panel(Cabinet& c, UuidGenerator& gen,
                  PanelRole role, RoleParams params) {
    Panel p;
    p.id = gen.next_id<PanelIdTag>();
    p.role = role;
    p.role_params = std::move(params);
    auto id = p.id;
    c.panels[id] = std::move(p);
    return id;
}

}  // namespace

TEST(Geometry, TopPanel) {
    TestProject t;
    auto id = add_panel(t.cab(), t.gen(), PanelRole::Top, NoRoleParams{});
    auto g = compute_panel_geometry(t.cab(), t.cab().panels.at(id));
    EXPECT_EQ(g.origin.z, Millimeters{2384});       // 2400 - 16
    EXPECT_EQ(g.size.x, Millimeters{2400});
    EXPECT_EQ(g.size.y, Millimeters{596});          // 600 - 4 (back)
    EXPECT_EQ(g.size.z, Millimeters{16});
}

TEST(Geometry, BottomPanel) {
    TestProject t;
    auto id = add_panel(t.cab(), t.gen(), PanelRole::Bottom, NoRoleParams{});
    auto g = compute_panel_geometry(t.cab(), t.cab().panels.at(id));
    EXPECT_EQ(g.origin.z, Millimeters{0});
    EXPECT_EQ(g.size.z, Millimeters{16});
}

TEST(Geometry, SideLeftPanel) {
    TestProject t;
    auto id = add_panel(t.cab(), t.gen(), PanelRole::SideLeft, NoRoleParams{});
    auto g = compute_panel_geometry(t.cab(), t.cab().panels.at(id));
    EXPECT_EQ(g.origin.x, Millimeters{0});
    EXPECT_EQ(g.size.x, Millimeters{16});
    EXPECT_EQ(g.size.z, Millimeters{2400});
}

TEST(Geometry, SideRightPanel) {
    TestProject t;
    auto id = add_panel(t.cab(), t.gen(), PanelRole::SideRight, NoRoleParams{});
    auto g = compute_panel_geometry(t.cab(), t.cab().panels.at(id));
    EXPECT_EQ(g.origin.x, Millimeters{2384});       // 2400 - 16
    EXPECT_EQ(g.size.x, Millimeters{16});
}

TEST(Geometry, BackPanel) {
    TestProject t;
    auto id = add_panel(t.cab(), t.gen(), PanelRole::Back, NoRoleParams{});
    auto g = compute_panel_geometry(t.cab(), t.cab().panels.at(id));
    EXPECT_EQ(g.origin.y, Millimeters{596});        // 600 - 4
    EXPECT_EQ(g.size.y, Millimeters{4});
}

TEST(Geometry, ShelfFullWidth) {
    TestProject t;
    auto id = add_panel(t.cab(), t.gen(), PanelRole::Shelf,
                        ShelfParams{
                            .height_from_bottom = Millimeters{800},
                            .extent = ShelfFullWidth{},
                        });
    auto g = compute_panel_geometry(t.cab(), t.cab().panels.at(id));
    EXPECT_EQ(g.origin.x, Millimeters{16});         // вычисление от side
    EXPECT_EQ(g.size.x, Millimeters{2368});         // 2400 - 2*16
    EXPECT_EQ(g.origin.z, Millimeters{800});
    EXPECT_EQ(g.size.z, Millimeters{16});
}

TEST(Geometry, ShelfBetweenDividers) {
    TestProject t;
    auto d1 = add_panel(t.cab(), t.gen(), PanelRole::DividerVertical,
                        DividerVerticalParams{
                            .offset_from_left = Millimeters{600},
                        });
    auto d2 = add_panel(t.cab(), t.gen(), PanelRole::DividerVertical,
                        DividerVerticalParams{
                            .offset_from_left = Millimeters{1800},
                        });
    auto sid = add_panel(t.cab(), t.gen(), PanelRole::Shelf,
                         ShelfParams{
                             .height_from_bottom = Millimeters{1000},
                             .extent = ShelfBetweenDividers{.from = d1,
                                                            .to = d2},
                         });
    auto g = compute_panel_geometry(t.cab(), t.cab().panels.at(sid));
    EXPECT_EQ(g.origin.x, Millimeters{616});        // 600 + 16 (правый край левого divider)
    EXPECT_EQ(g.size.x, Millimeters{1184});         // 1800 - 616
}

TEST(Geometry, ShelfOverflowThrows) {
    TestProject t;
    auto id = add_panel(t.cab(), t.gen(), PanelRole::Shelf,
                        ShelfParams{
                            .height_from_bottom = Millimeters{2400},
                            .extent = ShelfFullWidth{},
                        });
    EXPECT_THROW(compute_panel_geometry(t.cab(), t.cab().panels.at(id)),
                 DomainError);
}

TEST(Geometry, DividerVerticalFullHeight) {
    TestProject t;
    auto id = add_panel(t.cab(), t.gen(), PanelRole::DividerVertical,
                        DividerVerticalParams{
                            .offset_from_left = Millimeters{500},
                        });
    auto g = compute_panel_geometry(t.cab(), t.cab().panels.at(id));
    EXPECT_EQ(g.origin.x, Millimeters{500});
    EXPECT_EQ(g.size.z, Millimeters{2400});
}

TEST(Geometry, DividerHorizontal) {
    TestProject t;
    auto id = add_panel(t.cab(), t.gen(), PanelRole::DividerHorizontal,
                        DividerHorizontalParams{
                            .offset_from_bottom = Millimeters{1200},
                        });
    auto g = compute_panel_geometry(t.cab(), t.cab().panels.at(id));
    EXPECT_EQ(g.origin.x, Millimeters{16});
    EXPECT_EQ(g.size.x, Millimeters{2368});
    EXPECT_EQ(g.origin.z, Millimeters{1200});
}

TEST(Geometry, FacadeFullFront) {
    TestProject t;
    auto id = add_panel(t.cab(), t.gen(), PanelRole::Facade,
                        FacadeParams{
                            .extent = FacadeFullFront{},
                            .hinge_side = HingeSide::Left,
                        });
    auto g = compute_panel_geometry(t.cab(), t.cab().panels.at(id));
    EXPECT_EQ(g.origin.x, Millimeters{0});
    EXPECT_EQ(g.size.x, Millimeters{2400});
    EXPECT_EQ(g.size.z, Millimeters{2400});
    EXPECT_EQ(g.origin.y, Millimeters{-16});        // фасад перед корпусом
}

TEST(Geometry, FacadeRect) {
    TestProject t;
    auto id = add_panel(t.cab(), t.gen(), PanelRole::Facade,
                        FacadeParams{
                            .extent = FacadeRect{
                                .from_x = Millimeters{100},
                                .to_x = Millimeters{700},
                                .from_z = Millimeters{200},
                                .to_z = Millimeters{1200},
                            },
                            .hinge_side = HingeSide::Right,
                        });
    auto g = compute_panel_geometry(t.cab(), t.cab().panels.at(id));
    EXPECT_EQ(g.size.x, Millimeters{600});
    EXPECT_EQ(g.size.z, Millimeters{1000});
}

TEST(Geometry, Plinth) {
    TestProject t;
    auto id = add_panel(t.cab(), t.gen(), PanelRole::Plinth,
                        PlinthParams{
                            .height = Millimeters{100},
                            .setback = Millimeters{50},
                        });
    auto g = compute_panel_geometry(t.cab(), t.cab().panels.at(id));
    EXPECT_EQ(g.origin.y, Millimeters{50});
    EXPECT_EQ(g.size.x, Millimeters{2400});
    EXPECT_EQ(g.size.z, Millimeters{100});
}

TEST(Geometry, CustomPanelKeepsOrientation) {
    TestProject t;
    Quat q{0.7071, 0.0, 0.7071, 0.0};
    auto id = add_panel(t.cab(), t.gen(), PanelRole::Custom,
                        CustomParams{
                            .position = Vec3{Millimeters{100},
                                             Millimeters{200},
                                             Millimeters{300}},
                            .size = Vec3{Millimeters{400},
                                         Millimeters{16},
                                         Millimeters{500}},
                            .orientation = q,
                        });
    auto g = compute_panel_geometry(t.cab(), t.cab().panels.at(id));
    EXPECT_EQ(g.origin, (Vec3{Millimeters{100}, Millimeters{200}, Millimeters{300}}));
    EXPECT_EQ(g.size, (Vec3{Millimeters{400}, Millimeters{16}, Millimeters{500}}));
    EXPECT_DOUBLE_EQ(g.orientation.w, 0.7071);
    EXPECT_DOUBLE_EQ(g.orientation.y, 0.7071);
}

TEST(Geometry, DrawerBottomFront) {
    TestProject t;
    auto db = add_panel(t.cab(), t.gen(), PanelRole::DrawerBottom,
                        DrawerBottomParams{
                            .height_from_bottom = Millimeters{200},
                            .depth = Millimeters{500},
                        });
    auto df = add_panel(t.cab(), t.gen(), PanelRole::DrawerFront,
                        DrawerFrontParams{
                            .height_from_bottom = Millimeters{200},
                            .height = Millimeters{180},
                        });
    auto gdb = compute_panel_geometry(t.cab(), t.cab().panels.at(db));
    EXPECT_EQ(gdb.size.x, Millimeters{2368});
    EXPECT_EQ(gdb.size.y, Millimeters{500});
    auto gdf = compute_panel_geometry(t.cab(), t.cab().panels.at(df));
    EXPECT_EQ(gdf.origin.y, Millimeters{-16});
    EXPECT_EQ(gdf.size.z, Millimeters{180});
}

TEST(Geometry, ThicknessOverrideUsed) {
    TestProject t;
    Panel p;
    p.id = t.gen().next_id<PanelIdTag>();
    p.role = PanelRole::Top;
    p.role_params = NoRoleParams{};
    p.thickness_override = Millimeters{18};
    auto id = p.id;
    t.cab().panels[id] = std::move(p);
    auto g = compute_panel_geometry(t.cab(), t.cab().panels.at(id));
    EXPECT_EQ(g.size.z, Millimeters{18});
    EXPECT_EQ(g.origin.z, Millimeters{2382});       // 2400 - 18
}
