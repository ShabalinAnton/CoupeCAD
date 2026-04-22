#include "coupecad/core/project.h"
#include "coupecad/core/errors.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

TEST(Project, EmptyProjectValidates) {
    auto p = Project::create_empty("My wardrobe",
                                    make_seeded_uuid_generator(10));
    EXPECT_EQ(p.meta().name, "My wardrobe");
    EXPECT_EQ(p.materials().size(), 1u);
    EXPECT_TRUE(p.cabinet().id.is_valid());
    EXPECT_NO_THROW(p.validate());
}

TEST(Project, ChangeSetEmptyByDefault) {
    ChangeSet cs;
    EXPECT_TRUE(cs.empty());
}

TEST(Project, ChangeSetNotEmptyAfterAnyField) {
    ChangeSet cs;
    cs.added_panels.push_back(PanelId{});
    EXPECT_FALSE(cs.empty());
}

TEST(Project, AddingPanelDirectlyValidates) {
    auto p = Project::create_empty("X", make_seeded_uuid_generator(11));
    Panel pan;
    pan.id = p.uuid_gen().next_id<PanelIdTag>();
    pan.role = PanelRole::Top;
    pan.role_params = NoRoleParams{};
    p.mutable_cabinet().panels[pan.id] = pan;
    EXPECT_NO_THROW(p.validate());
}

TEST(Project, EdgeBandingReferencingUnknownMaterialThrows) {
    auto p = Project::create_empty("X", make_seeded_uuid_generator(12));
    Panel pan;
    pan.id = p.uuid_gen().next_id<PanelIdTag>();
    pan.role = PanelRole::Top;
    pan.role_params = NoRoleParams{};
    pan.edge_banding.front = EdgeBanding{
        .material_id = p.uuid_gen().next_id<MaterialIdTag>(),  // не в materials
        .thickness = Millimeters{2},
    };
    p.mutable_cabinet().panels[pan.id] = pan;
    EXPECT_THROW(p.validate(), DomainError);
}

TEST(Project, HardwareItemReferencingUnknownRefThrows) {
    auto p = Project::create_empty("X", make_seeded_uuid_generator(13));
    Panel pan;
    pan.id = p.uuid_gen().next_id<PanelIdTag>();
    pan.role = PanelRole::Top;
    pan.role_params = NoRoleParams{};
    auto pid = pan.id;
    p.mutable_cabinet().panels[pid] = pan;

    HardwareItem h;
    h.id = p.uuid_gen().next_id<HardwareItemIdTag>();
    h.ref = HardwareRef{"unknown.ref"};
    h.attachments.push_back(PanelAttachment{
        .panel_id = pid,
        .local_position = Vec3{},
        .orientation = Quat::identity(),
    });
    p.mutable_cabinet().hardware[h.id] = std::move(h);
    EXPECT_THROW(p.validate(), DomainError);
}

TEST(Project, AddingHardwareSpecAndItemThatReferenceItValidates) {
    auto p = Project::create_empty("X", make_seeded_uuid_generator(14));
    Panel pan;
    pan.id = p.uuid_gen().next_id<PanelIdTag>();
    pan.role = PanelRole::Top;
    pan.role_params = NoRoleParams{};
    auto pid = pan.id;
    p.mutable_cabinet().panels[pid] = pan;

    HardwareSpec spec{
        .ref = HardwareRef{"hinge.generic.straight"},
        .kind = HardwareKind::Hinge,
        .name = "Петля",
        .sku = std::nullopt,
        .bbox = Vec3{Millimeters{35}, Millimeters{14}, Millimeters{60}},
        .price_each = std::nullopt,
    };
    p.mutable_hardware_catalog()[spec.ref] = spec;

    HardwareItem h;
    h.id = p.uuid_gen().next_id<HardwareItemIdTag>();
    h.ref = spec.ref;
    h.attachments.push_back(PanelAttachment{
        .panel_id = pid,
        .local_position = Vec3{},
        .orientation = Quat::identity(),
    });
    p.mutable_cabinet().hardware[h.id] = std::move(h);

    EXPECT_NO_THROW(p.validate());
}

TEST(Project, DeterministicIdsForSeededGenerator) {
    auto a = Project::create_empty("X", make_seeded_uuid_generator(99));
    auto b = Project::create_empty("X", make_seeded_uuid_generator(99));
    EXPECT_EQ(a.cabinet().id, b.cabinet().id);
    EXPECT_EQ(a.cabinet().default_panel_material,
              b.cabinet().default_panel_material);
}
