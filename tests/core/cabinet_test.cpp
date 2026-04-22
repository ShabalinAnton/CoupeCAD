#include "coupecad/core/cabinet.h"
#include "coupecad/core/errors.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

namespace {
Cabinet make_valid_cabinet(UuidGenerator& gen) {
    Cabinet c;
    c.id = gen.next_id<CabinetIdTag>();
    c.dimensions = Dimensions{
        .width = Millimeters{2400},
        .depth = Millimeters{600},
        .height = Millimeters{2400},
    };
    c.default_panel_material = gen.next_id<MaterialIdTag>();
    return c;
}

PanelId add_top_panel(Cabinet& c, UuidGenerator& gen) {
    Panel p;
    p.id = gen.next_id<PanelIdTag>();
    p.role = PanelRole::Top;
    p.role_params = NoRoleParams{};
    auto id = p.id;
    c.panels[id] = std::move(p);
    return id;
}
}  // namespace

TEST(Cabinet, ValidEmptyPasses) {
    auto gen = make_seeded_uuid_generator(4);
    EXPECT_NO_THROW(make_valid_cabinet(*gen).validate());
}

TEST(Cabinet, InvalidIdThrows) {
    auto gen = make_seeded_uuid_generator(4);
    Cabinet c = make_valid_cabinet(*gen);
    c.id = CabinetId{};
    EXPECT_THROW(c.validate(), DomainError);
}

TEST(Cabinet, ZeroDimensionThrows) {
    auto gen = make_seeded_uuid_generator(4);
    Cabinet c = make_valid_cabinet(*gen);
    c.dimensions.depth = Millimeters{0};
    EXPECT_THROW(c.validate(), DomainError);
}

TEST(Cabinet, InvalidDefaultMaterialThrows) {
    auto gen = make_seeded_uuid_generator(4);
    Cabinet c = make_valid_cabinet(*gen);
    c.default_panel_material = MaterialId{};
    EXPECT_THROW(c.validate(), DomainError);
}

TEST(Cabinet, NonPositivePanelThicknessThrows) {
    auto gen = make_seeded_uuid_generator(4);
    Cabinet c = make_valid_cabinet(*gen);
    c.default_panel_thickness = Millimeters{0};
    EXPECT_THROW(c.validate(), DomainError);
}

TEST(Cabinet, PanelMapKeyMismatchThrows) {
    auto gen = make_seeded_uuid_generator(4);
    Cabinet c = make_valid_cabinet(*gen);
    Panel p;
    p.id = gen->next_id<PanelIdTag>();
    p.role = PanelRole::Top;
    p.role_params = NoRoleParams{};
    PanelId wrong_key = gen->next_id<PanelIdTag>();
    c.panels[wrong_key] = std::move(p);
    EXPECT_THROW(c.validate(), DomainError);
}

TEST(Cabinet, ValidPanelsPass) {
    auto gen = make_seeded_uuid_generator(4);
    Cabinet c = make_valid_cabinet(*gen);
    add_top_panel(c, *gen);
    EXPECT_NO_THROW(c.validate());
}

TEST(Cabinet, HardwareReferencingMissingPanelThrows) {
    auto gen = make_seeded_uuid_generator(4);
    Cabinet c = make_valid_cabinet(*gen);
    HardwareItem h;
    h.id = gen->next_id<HardwareItemIdTag>();
    h.ref = HardwareRef{"hinge.generic.straight"};
    h.attachments.push_back(PanelAttachment{
        .panel_id = gen->next_id<PanelIdTag>(),  // не в этом cabinet
        .local_position = Vec3{},
        .orientation = Quat::identity(),
    });
    c.hardware[h.id] = std::move(h);
    EXPECT_THROW(c.validate(), DomainError);
}

TEST(Cabinet, HardwareReferencingExistingPanelPasses) {
    auto gen = make_seeded_uuid_generator(4);
    Cabinet c = make_valid_cabinet(*gen);
    auto pid = add_top_panel(c, *gen);
    HardwareItem h;
    h.id = gen->next_id<HardwareItemIdTag>();
    h.ref = HardwareRef{"hinge.generic.straight"};
    h.attachments.push_back(PanelAttachment{
        .panel_id = pid,
        .local_position = Vec3{},
        .orientation = Quat::identity(),
    });
    c.hardware[h.id] = std::move(h);
    EXPECT_NO_THROW(c.validate());
}
