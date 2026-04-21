#include "coupecad/core/units.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

TEST(Millimeters, DefaultIsZero) {
    EXPECT_EQ(Millimeters{}.value(), 0);
}

TEST(Millimeters, ConstructFromInt) {
    EXPECT_EQ(Millimeters{1500}.value(), 1500);
}

TEST(Millimeters, Comparison) {
    EXPECT_TRUE(Millimeters{100} < Millimeters{200});
    EXPECT_TRUE(Millimeters{200} > Millimeters{100});
    EXPECT_TRUE(Millimeters{100} == Millimeters{100});
}

TEST(Millimeters, ArithmeticAddSub) {
    EXPECT_EQ((Millimeters{100} + Millimeters{50}).value(), 150);
    EXPECT_EQ((Millimeters{100} - Millimeters{30}).value(), 70);
    auto m = Millimeters{100};
    m += Millimeters{5};
    EXPECT_EQ(m.value(), 105);
    m -= Millimeters{10};
    EXPECT_EQ(m.value(), 95);
}

TEST(Millimeters, MultiplyByScalar) {
    EXPECT_EQ((Millimeters{16} * 3).value(), 48);
    EXPECT_EQ((3 * Millimeters{16}).value(), 48);
}

TEST(Millimeters, Negation) {
    EXPECT_EQ((-Millimeters{50}).value(), -50);
}

TEST(Millimeters, UDLLiteral) {
    using namespace coupecad::core;
    EXPECT_EQ((1500_mm).value(), 1500);
}

TEST(Dimensions, EqualityValueSemantics) {
    Dimensions a{.width = Millimeters{2400}, .depth = Millimeters{600},
                 .height = Millimeters{2400}};
    Dimensions b = a;
    EXPECT_EQ(a, b);
    b.width = Millimeters{2500};
    EXPECT_NE(a, b);
}

TEST(Vec3, Equality) {
    Vec3 a{Millimeters{1}, Millimeters{2}, Millimeters{3}};
    Vec3 b{Millimeters{1}, Millimeters{2}, Millimeters{3}};
    EXPECT_EQ(a, b);
}

TEST(Quat, IdentityIsWaxis) {
    auto q = Quat::identity();
    EXPECT_DOUBLE_EQ(q.w, 1.0);
    EXPECT_DOUBLE_EQ(q.x, 0.0);
    EXPECT_DOUBLE_EQ(q.y, 0.0);
    EXPECT_DOUBLE_EQ(q.z, 0.0);
}

TEST(Money, MinorUnits) {
    Money m{.minor_units = 12345};
    EXPECT_EQ(m.minor_units, 12345);
}

TEST(RGBA, DefaultIsBlackOpaque) {
    RGBA c;
    EXPECT_EQ(c.r, 0);
    EXPECT_EQ(c.g, 0);
    EXPECT_EQ(c.b, 0);
    EXPECT_EQ(c.a, 255);
}
