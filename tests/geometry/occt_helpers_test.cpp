#include "coupecad/geometry/occt_helpers.h"

#include <gp_Pnt.hxx>
#include <gtest/gtest.h>

using coupecad::core::Millimeters;
using coupecad::core::Quat;
using coupecad::core::Vec3;
using coupecad::geometry::to_box_dims;
using coupecad::geometry::to_occt_point;
using coupecad::geometry::to_occt_transform;

TEST(OcctHelpersTest, ToOcctPoint_PreservesIntegerMillimetersAsDoubles) {
    const Vec3 v{Millimeters{600}, Millimeters{500}, Millimeters{2000}};
    const auto p = to_occt_point(v);
    EXPECT_DOUBLE_EQ(p.X(), 600.0);
    EXPECT_DOUBLE_EQ(p.Y(), 500.0);
    EXPECT_DOUBLE_EQ(p.Z(), 2000.0);
}

TEST(OcctHelpersTest, ToBoxDims_PreservesPositiveSizes) {
    const Vec3 size{Millimeters{800}, Millimeters{600}, Millimeters{16}};
    const auto d = to_box_dims(size);
    EXPECT_DOUBLE_EQ(d.dx, 800.0);
    EXPECT_DOUBLE_EQ(d.dy, 600.0);
    EXPECT_DOUBLE_EQ(d.dz, 16.0);
}

TEST(OcctHelpersTest, ToBoxDims_ZeroVectorYieldsZeroBox) {
    const Vec3 size{};
    const auto d = to_box_dims(size);
    EXPECT_DOUBLE_EQ(d.dx, 0.0);
    EXPECT_DOUBLE_EQ(d.dy, 0.0);
    EXPECT_DOUBLE_EQ(d.dz, 0.0);
}

TEST(OcctHelpersTest, ToOcctTransform_IdentityQuaternion_PureTranslation) {
    const Vec3 origin{Millimeters{100}, Millimeters{200}, Millimeters{300}};
    const auto trsf = to_occt_transform(origin, Quat::identity());

    gp_Pnt p(0.0, 0.0, 0.0);
    p.Transform(trsf);
    EXPECT_DOUBLE_EQ(p.X(), 100.0);
    EXPECT_DOUBLE_EQ(p.Y(), 200.0);
    EXPECT_DOUBLE_EQ(p.Z(), 300.0);
}

TEST(OcctHelpersTest, ToOcctTransform_RotationAroundZ90Deg_AppliedThenTranslated) {
    // Кватернион поворота на 90° вокруг Z: (w=cos45°, x=0, y=0, z=sin45°).
    constexpr double s = 0.70710678118654752440;  // sin(45°) = cos(45°)
    const Quat q{s, 0.0, 0.0, s};
    const Vec3 origin{Millimeters{10}, Millimeters{20}, Millimeters{0}};
    const auto trsf = to_occt_transform(origin, q);

    // Точка (1,0,0) после поворота → (0,1,0), затем + (10,20,0) = (10,21,0).
    gp_Pnt p(1.0, 0.0, 0.0);
    p.Transform(trsf);
    EXPECT_NEAR(p.X(), 10.0, 1e-9);
    EXPECT_NEAR(p.Y(), 21.0, 1e-9);
    EXPECT_NEAR(p.Z(), 0.0, 1e-9);
}
