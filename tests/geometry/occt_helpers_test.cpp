#include "coupecad/geometry/occt_helpers.h"

#include <gtest/gtest.h>

using coupecad::core::Millimeters;
using coupecad::core::Quat;
using coupecad::core::Vec3;
using coupecad::geometry::to_occt_point;

TEST(OcctHelpersTest, ToOcctPoint_PreservesIntegerMillimetersAsDoubles) {
    const Vec3 v{Millimeters{600}, Millimeters{500}, Millimeters{2000}};
    const auto p = to_occt_point(v);
    EXPECT_DOUBLE_EQ(p.X(), 600.0);
    EXPECT_DOUBLE_EQ(p.Y(), 500.0);
    EXPECT_DOUBLE_EQ(p.Z(), 2000.0);
}
