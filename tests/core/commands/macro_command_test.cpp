#include "coupecad/core/commands/macro_command.h"
#include "coupecad/core/commands/panel_commands.h"
#include "coupecad/core/errors.h"
#include "coupecad/core/project.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

TEST(MacroCommand, AppliesChildrenInOrder) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(6));
    MacroCommand mc{"Add three panels"};
    mc.push(std::make_unique<AddPanel>(PanelRole::Top, NoRoleParams{}));
    mc.push(std::make_unique<AddPanel>(PanelRole::Bottom, NoRoleParams{}));
    mc.push(std::make_unique<AddPanel>(PanelRole::Back, NoRoleParams{}));
    auto cs = mc.apply(p);
    EXPECT_EQ(p.cabinet().panels.size(), 3u);
    EXPECT_EQ(cs.added_panels.size(), 3u);
}

TEST(MacroCommand, RevertsChildrenInReverseOrder) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(6));
    MacroCommand mc{"x"};
    mc.push(std::make_unique<AddPanel>(PanelRole::Top, NoRoleParams{}));
    mc.push(std::make_unique<AddPanel>(PanelRole::Bottom, NoRoleParams{}));
    mc.apply(p);
    mc.revert(p);
    EXPECT_EQ(p.cabinet().panels.size(), 0u);
}

TEST(MacroCommand, FailureInMiddleRollsBack) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(6));
    MacroCommand mc{"x"};
    mc.push(std::make_unique<AddPanel>(PanelRole::Top, NoRoleParams{}));
    // Вторая команда заведомо упадёт (RemovePanel с invalid id).
    mc.push(std::make_unique<RemovePanel>(PanelId{}));
    EXPECT_THROW(mc.apply(p), DomainError);
    EXPECT_EQ(p.cabinet().panels.size(), 0u);   // откатили первую
}

TEST(MacroCommand, PushAfterApplyThrows) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(6));
    MacroCommand mc{"x"};
    mc.push(std::make_unique<AddPanel>(PanelRole::Top, NoRoleParams{}));
    mc.apply(p);
    EXPECT_THROW(mc.push(std::make_unique<AddPanel>(PanelRole::Back, NoRoleParams{})),
                 LogicError);
}
