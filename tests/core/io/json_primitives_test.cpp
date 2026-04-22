#include "coupecad/core/errors.h"
#include "coupecad/core/id.h"
#include "coupecad/core/io/json_project_serializer.h"
#include "coupecad/core/units.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using namespace coupecad::core;
using namespace coupecad::core::detail;

TEST(JsonPrimitives, MillimetersRoundTrip) {
    auto j = to_json_millimeters(Millimeters{1500});
    EXPECT_EQ(j.get<int>(), 1500);
    EXPECT_EQ(from_json_millimeters(j), Millimeters{1500});
}

TEST(JsonPrimitives, MillimetersNegative) {
    auto j = to_json_millimeters(Millimeters{-7});
    EXPECT_EQ(from_json_millimeters(j), Millimeters{-7});
}

TEST(JsonPrimitives, MillimetersRejectsNonInt) {
    nlohmann::json j = 3.14;
    EXPECT_THROW(from_json_millimeters(j), InvalidData);
}

TEST(JsonPrimitives, DimensionsRoundTrip) {
    Dimensions d{.width = Millimeters{2400},
                 .depth = Millimeters{600},
                 .height = Millimeters{2500}};
    EXPECT_EQ(from_json_dimensions(to_json_dimensions(d)), d);
}

TEST(JsonPrimitives, Vec3RoundTrip) {
    Vec3 v{Millimeters{10}, Millimeters{-20}, Millimeters{30}};
    EXPECT_EQ(from_json_vec3(to_json_vec3(v)), v);
}

TEST(JsonPrimitives, Vec3RejectsWrongShape) {
    nlohmann::json j = nlohmann::json::array({1, 2});
    EXPECT_THROW(from_json_vec3(j), InvalidData);
}

TEST(JsonPrimitives, QuatRoundTrip) {
    Quat q{0.7071, 0.0, 0.7071, 0.0};
    auto j = to_json_quat(q);
    auto back = from_json_quat(j);
    EXPECT_DOUBLE_EQ(back.w, 0.7071);
    EXPECT_DOUBLE_EQ(back.y, 0.7071);
}

TEST(JsonPrimitives, MoneyRoundTrip) {
    EXPECT_EQ(from_json_money(to_json_money(Money{.minor_units = 12345})).minor_units, 12345);
}

TEST(JsonPrimitives, RGBARoundTrip) {
    RGBA c{240, 100, 50, 255};
    EXPECT_EQ(from_json_rgba(to_json_rgba(c)), c);
}

TEST(JsonPrimitives, IdRoundTrip) {
    auto gen = make_seeded_uuid_generator(42);
    PanelId id{gen->next()};
    auto j = to_json_id(id);
    EXPECT_TRUE(j.is_string());
    EXPECT_EQ(from_json_id<PanelIdTag>(j), id);
}

TEST(JsonPrimitives, NullIdIsInvalid) {
    nlohmann::json j = nullptr;
    auto parsed = from_json_id<PanelIdTag>(j);
    EXPECT_FALSE(parsed.is_valid());
}

TEST(JsonPrimitives, MalformedIdThrows) {
    nlohmann::json j = "not-a-uuid";
    EXPECT_THROW(from_json_id<PanelIdTag>(j), InvalidData);
}
