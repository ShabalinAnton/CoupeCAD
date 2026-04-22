#include "coupecad/core/id.h"
#include "coupecad/core/errors.h"

#include <gtest/gtest.h>

#include <set>
#include <unordered_set>

using namespace coupecad::core;

TEST(IdTypes, AreDistinct) {
    // PanelId и CabinetId — разные типы, не присваиваются друг другу.
    static_assert(!std::is_assignable_v<PanelId&, CabinetId>);
    static_assert(!std::is_assignable_v<CabinetId&, MaterialId>);
}

TEST(Id, DefaultConstructedIsInvalid) {
    PanelId p;
    EXPECT_FALSE(p.is_valid());
}

TEST(Id, RoundTripStringFormat) {
    auto gen = make_seeded_uuid_generator(42);
    PanelId id{gen->next()};
    auto str = id.to_string();
    EXPECT_EQ(str.size(), 36);  // canonical UUID length
    auto parsed = PanelId::from_string(str);
    EXPECT_EQ(id, parsed);
}

TEST(Id, FromInvalidString) {
    auto bad = PanelId::from_string("not-a-uuid");
    EXPECT_FALSE(bad.is_valid());
}

TEST(Id, UseInUnorderedMap) {
    std::unordered_set<PanelId> seen;
    auto gen = make_seeded_uuid_generator(7);
    for (int i = 0; i < 10; ++i) {
        seen.insert(PanelId{gen->next()});
    }
    EXPECT_EQ(seen.size(), 10u);
}

TEST(SeededUuidGenerator, DeterministicSequenceForSameSeed) {
    auto a = make_seeded_uuid_generator(123);
    auto b = make_seeded_uuid_generator(123);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(a->next(), b->next());
    }
}

TEST(SeededUuidGenerator, DifferentSeedsProduceDifferentSequences) {
    auto a = make_seeded_uuid_generator(1);
    auto b = make_seeded_uuid_generator(2);
    EXPECT_NE(a->next(), b->next());
}

TEST(RandomUuidGenerator, ProducesDifferentValues) {
    auto gen = make_random_uuid_generator();
    auto x = gen->next();
    auto y = gen->next();
    EXPECT_NE(x, y);
}

TEST(HardwareRef, ConstructionAndEquality) {
    HardwareRef a{"hinge.generic.straight"};
    HardwareRef b{"hinge.generic.straight"};
    EXPECT_EQ(a, b);
    HardwareRef c{"slide.generic"};
    EXPECT_NE(a, c);
}

TEST(CoupecadException, CarriesCodeAndMessage) {
    DomainError e{"panel.invalid_role", "role/role_params mismatch"};
    EXPECT_STREQ(e.what(), "role/role_params mismatch");
    EXPECT_EQ(e.code(), "panel.invalid_role");
}
