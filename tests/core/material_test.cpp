#include "coupecad/core/material.h"
#include "coupecad/core/errors.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

namespace {
Material make_valid() {
    auto gen = make_seeded_uuid_generator(1);
    Material m;
    m.id = gen->next_id<MaterialIdTag>();
    m.name = "ЛДСП 16мм";
    m.kind = MaterialKind::ChipboardLaminated;
    m.default_thickness = Millimeters{16};
    m.color_hint = RGBA{240, 240, 240, 255};
    return m;
}
}  // namespace

TEST(Material, ValidPasses) {
    EXPECT_NO_THROW(make_valid().validate());
}

TEST(Material, InvalidIdThrows) {
    Material m = make_valid();
    m.id = MaterialId{};  // default = nil
    EXPECT_THROW(m.validate(), DomainError);
}

TEST(Material, EmptyNameThrows) {
    Material m = make_valid();
    m.name.clear();
    EXPECT_THROW(m.validate(), DomainError);
}

TEST(Material, ZeroThicknessThrows) {
    Material m = make_valid();
    m.default_thickness = Millimeters{0};
    EXPECT_THROW(m.validate(), DomainError);
}

TEST(Material, NegativeThicknessThrows) {
    Material m = make_valid();
    m.default_thickness = Millimeters{-5};
    EXPECT_THROW(m.validate(), DomainError);
}

TEST(Material, EqualityIsValueBased) {
    Material a = make_valid();
    Material b = a;
    EXPECT_EQ(a, b);
    b.name = "Different";
    EXPECT_NE(a, b);
}

TEST(MaterialKind, NamesAreStable) {
    EXPECT_STREQ(material_kind_name(MaterialKind::ChipboardLaminated),
                 "ChipboardLaminated");
    EXPECT_STREQ(material_kind_name(MaterialKind::Mdf), "Mdf");
    EXPECT_STREQ(material_kind_name(MaterialKind::Glass), "Glass");
}
