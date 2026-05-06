#include "coupecad/renderer/entity_id.h"

#include "coupecad/core/id.h"

#include <gtest/gtest.h>

#include <unordered_map>
#include <unordered_set>

using coupecad::core::HardwareItemId;
using coupecad::core::PanelId;
using coupecad::core::Id;
using coupecad::renderer::EntityId;

namespace {

PanelId make_panel_id(std::string_view s) {
    return PanelId::from_string(s);
}

HardwareItemId make_hw_id(std::string_view s) {
    return HardwareItemId::from_string(s);
}

}  // namespace

TEST(EntityIdTest, EqualityWithinSameAlternative) {
    const auto p1 = make_panel_id("11111111-1111-1111-1111-111111111111");
    const auto p2 = make_panel_id("11111111-1111-1111-1111-111111111111");
    EXPECT_EQ(EntityId{p1}, EntityId{p2});
}

TEST(EntityIdTest, InequalityAcrossAlternatives) {
    const auto p = make_panel_id("11111111-1111-1111-1111-111111111111");
    const auto h = make_hw_id("11111111-1111-1111-1111-111111111111");
    EXPECT_NE(EntityId{p}, EntityId{h});
}

TEST(EntityIdTest, UsableAsUnorderedKey) {
    std::unordered_set<EntityId> set;
    set.insert(EntityId{make_panel_id("11111111-1111-1111-1111-111111111111")});
    set.insert(EntityId{make_hw_id("22222222-2222-2222-2222-222222222222")});
    EXPECT_EQ(set.size(), 2u);
    EXPECT_TRUE(set.contains(
        EntityId{make_panel_id("11111111-1111-1111-1111-111111111111")}));
}
