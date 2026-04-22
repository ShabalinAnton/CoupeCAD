#include "coupecad/core/commands/panel_commands.h"
#include "coupecad/core/project.h"
#include "coupecad/core/undo_stack.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

namespace {
class RecordingObserver : public IProjectObserver {
public:
    std::vector<ChangeSet> changes;
    void on_changed(const Project&, const ChangeSet& cs) override {
        changes.push_back(cs);
    }
};
}  // namespace

TEST(UndoStack, ExecutePushesAndNotifies) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(7));
    UndoStack us{p};
    RecordingObserver obs;
    us.add_observer(&obs);
    us.execute(std::make_unique<AddPanel>(PanelRole::Top, NoRoleParams{}));
    EXPECT_TRUE(us.can_undo());
    EXPECT_FALSE(us.can_redo());
    ASSERT_EQ(obs.changes.size(), 1u);
    EXPECT_EQ(obs.changes[0].added_panels.size(), 1u);
}

TEST(UndoStack, UndoRedoRoundTrip) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(7));
    UndoStack us{p};
    us.execute(std::make_unique<AddPanel>(PanelRole::Top, NoRoleParams{}));
    us.execute(std::make_unique<AddPanel>(PanelRole::Bottom, NoRoleParams{}));
    EXPECT_EQ(p.cabinet().panels.size(), 2u);
    us.undo();
    EXPECT_EQ(p.cabinet().panels.size(), 1u);
    us.undo();
    EXPECT_EQ(p.cabinet().panels.size(), 0u);
    us.redo();
    EXPECT_EQ(p.cabinet().panels.size(), 1u);
    us.redo();
    EXPECT_EQ(p.cabinet().panels.size(), 2u);
}

TEST(UndoStack, NewExecuteClearsRedoStack) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(7));
    UndoStack us{p};
    us.execute(std::make_unique<AddPanel>(PanelRole::Top, NoRoleParams{}));
    us.undo();
    EXPECT_TRUE(us.can_redo());
    us.execute(std::make_unique<AddPanel>(PanelRole::Bottom, NoRoleParams{}));
    EXPECT_FALSE(us.can_redo());
}

TEST(UndoStack, UndoAtEmptyIsNoOp) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(7));
    UndoStack us{p};
    us.undo();   // не бросает
    EXPECT_FALSE(us.can_undo());
}

TEST(UndoStack, ClearResetsStacks) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(7));
    UndoStack us{p};
    us.execute(std::make_unique<AddPanel>(PanelRole::Top, NoRoleParams{}));
    us.clear();
    EXPECT_FALSE(us.can_undo());
    EXPECT_FALSE(us.can_redo());
}

TEST(UndoStack, RemoveObserver) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(7));
    UndoStack us{p};
    RecordingObserver obs;
    us.add_observer(&obs);
    us.remove_observer(&obs);
    us.execute(std::make_unique<AddPanel>(PanelRole::Top, NoRoleParams{}));
    EXPECT_TRUE(obs.changes.empty());
}
