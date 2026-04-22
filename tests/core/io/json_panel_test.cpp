#include "coupecad/core/errors.h"
#include "coupecad/core/io/json_project_serializer.h"
#include "coupecad/core/panel.h"

#include <gtest/gtest.h>

using namespace coupecad::core;
using namespace coupecad::core::detail;

namespace {
Panel make_panel(PanelRole role, RoleParams rp) {
    auto gen = make_seeded_uuid_generator(111);
    Panel p;
    p.id = PanelId{gen->next()};
    p.role = role;
    p.role_params = std::move(rp);
    return p;
}

void round_trip(const Panel& p) {
    auto j = to_json_panel(p);
    auto back = from_json_panel(j);
    EXPECT_EQ(back, p);
}
}  // namespace

TEST(JsonPanel, TopBottomBackSides) {
    round_trip(make_panel(PanelRole::Top, NoRoleParams{}));
    round_trip(make_panel(PanelRole::Bottom, NoRoleParams{}));
    round_trip(make_panel(PanelRole::Back, NoRoleParams{}));
    round_trip(make_panel(PanelRole::SideLeft, NoRoleParams{}));
    round_trip(make_panel(PanelRole::SideRight, NoRoleParams{}));
}

TEST(JsonPanel, ShelfFullWidth) {
    round_trip(make_panel(PanelRole::Shelf,
                ShelfParams{.height_from_bottom = Millimeters{800},
                            .extent = ShelfFullWidth{}}));
}

TEST(JsonPanel, ShelfBetweenDividers) {
    auto gen = make_seeded_uuid_generator(1);
    round_trip(make_panel(PanelRole::Shelf,
                ShelfParams{.height_from_bottom = Millimeters{1000},
                            .extent = ShelfBetweenDividers{
                                .from = PanelId{gen->next()},
                                .to = PanelId{gen->next()}}}));
}

TEST(JsonPanel, DividerVerticalFull) {
    round_trip(make_panel(PanelRole::DividerVertical,
                DividerVerticalParams{.offset_from_left = Millimeters{600},
                                       .height_extent = VerticalExtentFull{}}));
}

TEST(JsonPanel, DividerVerticalRange) {
    round_trip(make_panel(PanelRole::DividerVertical,
                DividerVerticalParams{.offset_from_left = Millimeters{700},
                                       .height_extent = VerticalExtentRange{
                                           .from_z = Millimeters{200},
                                           .to_z = Millimeters{1800}}}));
}

TEST(JsonPanel, DividerHorizontalRange) {
    round_trip(make_panel(PanelRole::DividerHorizontal,
                DividerHorizontalParams{.offset_from_bottom = Millimeters{1200},
                                         .depth_extent = DepthExtentRange{
                                             .from_y = Millimeters{0},
                                             .to_y = Millimeters{500}}}));
}

TEST(JsonPanel, FacadeFullFront) {
    round_trip(make_panel(PanelRole::Facade,
                FacadeParams{.extent = FacadeFullFront{},
                              .hinge_side = HingeSide::Left}));
}

TEST(JsonPanel, FacadeRect) {
    round_trip(make_panel(PanelRole::Facade,
                FacadeParams{.extent = FacadeRect{
                                 .from_x = Millimeters{100},
                                 .to_x = Millimeters{700},
                                 .from_z = Millimeters{200},
                                 .to_z = Millimeters{1200}},
                              .hinge_side = HingeSide::Right}));
}

TEST(JsonPanel, DrawerBottomFrontSideBack) {
    round_trip(make_panel(PanelRole::DrawerBottom,
                DrawerBottomParams{.height_from_bottom = Millimeters{200},
                                    .depth = Millimeters{500}}));
    round_trip(make_panel(PanelRole::DrawerFront,
                DrawerFrontParams{.height_from_bottom = Millimeters{200},
                                   .height = Millimeters{180}}));
    round_trip(make_panel(PanelRole::DrawerSide,
                DrawerSideParams{.height_from_bottom = Millimeters{200},
                                  .height = Millimeters{180},
                                  .depth = Millimeters{500},
                                  .side = DrawerSideParams::Side::Right}));
    round_trip(make_panel(PanelRole::DrawerBack,
                DrawerBackParams{.height_from_bottom = Millimeters{200},
                                  .height = Millimeters{180}}));
}

TEST(JsonPanel, Plinth) {
    round_trip(make_panel(PanelRole::Plinth,
                PlinthParams{.height = Millimeters{100},
                              .setback = Millimeters{50}}));
}

TEST(JsonPanel, Custom) {
    round_trip(make_panel(PanelRole::Custom,
                CustomParams{.position = Vec3{Millimeters{10}, Millimeters{20}, Millimeters{30}},
                              .size = Vec3{Millimeters{100}, Millimeters{16}, Millimeters{200}},
                              .orientation = Quat{0.7071, 0.0, 0.7071, 0.0}}));
}

TEST(JsonPanel, UnknownRoleThrows) {
    EXPECT_THROW(panel_role_from_str("Nonsense"), InvalidData);
}

TEST(JsonPanel, UnknownRoleParamsKindThrows) {
    nlohmann::json j{{"kind", "BogusShape"}};
    EXPECT_THROW(from_json_role_params(j), InvalidData);
}
