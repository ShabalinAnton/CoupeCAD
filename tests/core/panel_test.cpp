#include "coupecad/core/panel.h"
#include "coupecad/core/errors.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

namespace {
PanelId next_panel_id(UuidGenerator& g) { return g.next_id<PanelIdTag>(); }
}  // namespace

TEST(PanelRole, NameForEveryRole) {
    EXPECT_STREQ(panel_role_name(PanelRole::Top), "Top");
    EXPECT_STREQ(panel_role_name(PanelRole::Shelf), "Shelf");
    EXPECT_STREQ(panel_role_name(PanelRole::Custom), "Custom");
}

TEST(IsRoleParamsValidFor, MatchedPairs) {
    EXPECT_TRUE(is_role_params_valid_for(PanelRole::Top, NoRoleParams{}));
    EXPECT_TRUE(is_role_params_valid_for(PanelRole::Shelf, ShelfParams{}));
    EXPECT_TRUE(is_role_params_valid_for(PanelRole::DividerVertical,
                                         DividerVerticalParams{}));
    EXPECT_TRUE(is_role_params_valid_for(PanelRole::Facade, FacadeParams{}));
    EXPECT_TRUE(is_role_params_valid_for(PanelRole::Custom, CustomParams{}));
}

TEST(IsRoleParamsValidFor, MismatchedPairs) {
    EXPECT_FALSE(is_role_params_valid_for(PanelRole::Top, ShelfParams{}));
    EXPECT_FALSE(is_role_params_valid_for(PanelRole::Shelf, NoRoleParams{}));
    EXPECT_FALSE(is_role_params_valid_for(PanelRole::Facade, ShelfParams{}));
}

TEST(Panel, ValidShelfPasses) {
    auto gen = make_seeded_uuid_generator(3);
    Panel p;
    p.id = next_panel_id(*gen);
    p.role = PanelRole::Shelf;
    p.role_params = ShelfParams{
        .height_from_bottom = Millimeters{800},
        .extent = ShelfFullWidth{},
    };
    EXPECT_NO_THROW(p.validate());
}

TEST(Panel, InvalidIdThrows) {
    Panel p;
    p.role = PanelRole::Top;
    p.role_params = NoRoleParams{};
    EXPECT_THROW(p.validate(), DomainError);
}

TEST(Panel, RoleParamsMismatchThrows) {
    auto gen = make_seeded_uuid_generator(3);
    Panel p;
    p.id = next_panel_id(*gen);
    p.role = PanelRole::Shelf;
    p.role_params = NoRoleParams{};   // wrong variant
    EXPECT_THROW(p.validate(), DomainError);
}

TEST(Panel, NonPositiveThicknessOverrideThrows) {
    auto gen = make_seeded_uuid_generator(3);
    Panel p;
    p.id = next_panel_id(*gen);
    p.role = PanelRole::Top;
    p.role_params = NoRoleParams{};
    p.thickness_override = Millimeters{0};
    EXPECT_THROW(p.validate(), DomainError);
}

TEST(Panel, EdgeBandingWithInvalidMaterialThrows) {
    auto gen = make_seeded_uuid_generator(3);
    Panel p;
    p.id = next_panel_id(*gen);
    p.role = PanelRole::Top;
    p.role_params = NoRoleParams{};
    p.edge_banding.front = EdgeBanding{
        .material_id = MaterialId{},   // invalid
        .thickness = Millimeters{2},
    };
    EXPECT_THROW(p.validate(), DomainError);
}

TEST(Panel, EdgeBandingWithZeroThicknessThrows) {
    auto gen = make_seeded_uuid_generator(3);
    Panel p;
    p.id = next_panel_id(*gen);
    p.role = PanelRole::Top;
    p.role_params = NoRoleParams{};
    p.edge_banding.left = EdgeBanding{
        .material_id = MaterialId{gen->next()},
        .thickness = Millimeters{0},
    };
    EXPECT_THROW(p.validate(), DomainError);
}

TEST(Panel, EqualityIsValueBased) {
    auto gen = make_seeded_uuid_generator(3);
    Panel a;
    a.id = next_panel_id(*gen);
    a.role = PanelRole::Top;
    Panel b = a;
    EXPECT_EQ(a, b);
    b.label = "Different";
    EXPECT_NE(a, b);
}
