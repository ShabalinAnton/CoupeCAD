#include "coupecad/core/errors.h"
#include "coupecad/core/hardware.h"
#include "coupecad/core/io/json_project_serializer.h"
#include "coupecad/core/material.h"
#include "coupecad/core/panel.h"

#include <gtest/gtest.h>

using namespace coupecad::core;
using namespace coupecad::core::detail;

namespace {
auto gen = make_seeded_uuid_generator(777);

Material make_material() {
    return Material{
        .id = MaterialId{gen->next()},
        .name = "Test Material",
        .kind = MaterialKind::Mdf,
        .default_thickness = Millimeters{18},
        .color_hint = RGBA{200, 150, 100, 255},
        .texture_ref = std::nullopt,
        .price_per_sqm = Money{.minor_units = 120000},
    };
}
}  // namespace

TEST(JsonEntities, MaterialRoundTrip) {
    auto m = make_material();
    auto j = to_json_material(m);
    auto back = from_json_material(j);
    EXPECT_EQ(back, m);
}

TEST(JsonEntities, MaterialKindStrings) {
    EXPECT_EQ(material_kind_from_str("Mdf"), MaterialKind::Mdf);
    EXPECT_THROW(material_kind_from_str("BogusKind"), InvalidData);
}

TEST(JsonEntities, EdgeBandingRoundTrip) {
    EdgeBanding eb{.material_id = MaterialId{gen->next()},
                    .thickness = Millimeters{2}};
    EXPECT_EQ(from_json_edge_banding(to_json_edge_banding(eb)), eb);
}

TEST(JsonEntities, PanelEdgeBandingRoundTrip) {
    PanelEdgeBanding eb;
    eb.front = EdgeBanding{.material_id = MaterialId{gen->next()},
                            .thickness = Millimeters{2}};
    auto j = to_json_panel_edge_banding(eb);
    auto back = from_json_panel_edge_banding(j);
    EXPECT_EQ(back, eb);
}

TEST(JsonEntities, HardwareSpecRoundTrip) {
    HardwareSpec s{.ref = HardwareRef{"hinge.test"},
                    .kind = HardwareKind::Hinge,
                    .name = "Hinge 90",
                    .sku = std::nullopt,
                    .bbox = Vec3{Millimeters{35}, Millimeters{14}, Millimeters{60}},
                    .price_each = Money{.minor_units = 4500}};
    EXPECT_EQ(from_json_hardware_spec(to_json_hardware_spec(s)), s);
}

TEST(JsonEntities, HardwareKindStrings) {
    EXPECT_EQ(hardware_kind_from_str("Connector"), HardwareKind::Connector);
    EXPECT_THROW(hardware_kind_from_str("WtfKind"), InvalidData);
}

TEST(JsonEntities, HardwareItemRoundTrip) {
    HardwareItem h;
    h.id = HardwareItemId{gen->next()};
    h.ref = HardwareRef{"test.ref"};
    h.attachments.push_back(PanelAttachment{
        .panel_id = PanelId{gen->next()},
        .local_position = Vec3{Millimeters{5}, Millimeters{10}, Millimeters{15}},
        .orientation = Quat::identity(),
    });
    h.label = "My hinge";
    EXPECT_EQ(from_json_hardware_item(to_json_hardware_item(h)), h);
}
