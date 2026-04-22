#include "coupecad/core/commands/panel_commands.h"
#include "coupecad/core/errors.h"
#include "coupecad/core/project.h"
#include "coupecad/core/undo_stack.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

TEST(UndoStackMacro, GroupsExecutesIntoOneEntry) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(8));
    UndoStack us{p};
    us.begin_macro("Add pair");
    us.execute(std::make_unique<AddPanel>(PanelRole::Top, NoRoleParams{}));
    us.execute(std::make_unique<AddPanel>(PanelRole::Bottom, NoRoleParams{}));
    us.end_macro();
    EXPECT_EQ(us.undo_depth(), 1u);
    EXPECT_EQ(p.cabinet().panels.size(), 2u);
    us.undo();
    EXPECT_EQ(p.cabinet().panels.size(), 0u);
    us.redo();
    EXPECT_EQ(p.cabinet().panels.size(), 2u);
}

TEST(UndoStackMacro, EmptyMacroAddsNothing) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(8));
    UndoStack us{p};
    us.begin_macro("Nop");
    us.end_macro();
    EXPECT_EQ(us.undo_depth(), 0u);
}

TEST(UndoStackMacro, NestedBeginThrows) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(8));
    UndoStack us{p};
    us.begin_macro("A");
    EXPECT_THROW(us.begin_macro("B"), LogicError);
    us.end_macro();
}

TEST(UndoStackMacro, EndWithoutBeginThrows) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(8));
    UndoStack us{p};
    EXPECT_THROW(us.end_macro(), LogicError);
}
