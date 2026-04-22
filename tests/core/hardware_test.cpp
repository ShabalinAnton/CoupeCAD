#include "coupecad/core/hardware.h"
#include "coupecad/core/errors.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

namespace {
HardwareSpec make_valid_spec() {
    return HardwareSpec{
        .ref = HardwareRef{"hinge.generic.straight"},
        .kind = HardwareKind::Hinge,
        .name = "Петля прямая 90°",
        .sku = std::nullopt,
        .bbox = Vec3{Millimeters{35}, Millimeters{14}, Millimeters{60}},
        .price_each = std::nullopt,
    };
}

HardwareItem make_valid_item() {
    auto gen = make_seeded_uuid_generator(2);
    HardwareItem h;
    h.id = gen->next_id<HardwareItemIdTag>();
    h.ref = HardwareRef{"hinge.generic.straight"};
    h.attachments.push_back(PanelAttachment{
        .panel_id = gen->next_id<PanelIdTag>(),
        .local_position = Vec3{Millimeters{10}, Millimeters{100}, Millimeters{8}},
        .orientation = Quat::identity(),
    });
    return h;
}
}  // namespace

TEST(HardwareKind, NamesAreStable) {
    EXPECT_STREQ(hardware_kind_name(HardwareKind::Hinge), "Hinge");
    EXPECT_STREQ(hardware_kind_name(HardwareKind::Connector), "Connector");
}

TEST(HardwareSpec, ValidPasses) {
    EXPECT_NO_THROW(make_valid_spec().validate());
}

TEST(HardwareSpec, EmptyRefThrows) {
    auto s = make_valid_spec();
    s.ref = HardwareRef{};
    EXPECT_THROW(s.validate(), DomainError);
}

TEST(HardwareSpec, EmptyNameThrows) {
    auto s = make_valid_spec();
    s.name.clear();
    EXPECT_THROW(s.validate(), DomainError);
}

TEST(HardwareSpec, NonPositiveBboxThrows) {
    auto s = make_valid_spec();
    s.bbox.y = Millimeters{0};
    EXPECT_THROW(s.validate(), DomainError);
}

TEST(HardwareItem, ValidPasses) {
    EXPECT_NO_THROW(make_valid_item().validate());
}

TEST(HardwareItem, InvalidIdThrows) {
    auto h = make_valid_item();
    h.id = HardwareItemId{};
    EXPECT_THROW(h.validate(), DomainError);
}

TEST(HardwareItem, NoAttachmentsThrows) {
    auto h = make_valid_item();
    h.attachments.clear();
    EXPECT_THROW(h.validate(), DomainError);
}

TEST(HardwareItem, AttachmentWithoutPanelIdThrows) {
    auto h = make_valid_item();
    h.attachments.front().panel_id = PanelId{};
    EXPECT_THROW(h.validate(), DomainError);
}
