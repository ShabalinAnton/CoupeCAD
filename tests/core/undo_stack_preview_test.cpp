#include "coupecad/core/commands/cabinet_commands.h"
#include "coupecad/core/commands/panel_commands.h"
#include "coupecad/core/errors.h"
#include "coupecad/core/project.h"
#include "coupecad/core/undo_stack.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

TEST(UndoStackPreview, CommitLandsAsSingleEntry) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(9));
    UndoStack us{p};
    auto cmd = std::make_unique<SetCabinetDimensions>(
        p.cabinet().id,
        Dimensions{Millimeters{2500}, Millimeters{700}, Millimeters{2500}});
    auto h = us.begin_preview(std::move(cmd));
    us.update_preview(h, std::any{Dimensions{Millimeters{2600},
                                               Millimeters{700},
                                               Millimeters{2500}}});
    us.update_preview(h, std::any{Dimensions{Millimeters{2700},
                                               Millimeters{700},
                                               Millimeters{2500}}});
    us.commit_preview(h);
    EXPECT_EQ(us.undo_depth(), 1u);
    EXPECT_EQ(p.cabinet().dimensions.width, Millimeters{2700});
    us.undo();
    EXPECT_EQ(p.cabinet().dimensions.width, Millimeters{2400});   // исходные
}

TEST(UndoStackPreview, CancelLeavesStackEmpty) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(9));
    UndoStack us{p};
    auto cmd = std::make_unique<SetCabinetDimensions>(
        p.cabinet().id,
        Dimensions{Millimeters{3000}, Millimeters{600}, Millimeters{2400}});
    auto h = us.begin_preview(std::move(cmd));
    us.update_preview(h, std::any{Dimensions{Millimeters{3500},
                                               Millimeters{600},
                                               Millimeters{2400}}});
    us.cancel_preview(h);
    EXPECT_EQ(us.undo_depth(), 0u);
    EXPECT_EQ(p.cabinet().dimensions.width, Millimeters{2400});
    EXPECT_FALSE(h.is_active());
}

TEST(UndoStackPreview, NestedBeginThrows) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(9));
    UndoStack us{p};
    auto h = us.begin_preview(std::make_unique<SetCabinetDimensions>(
        p.cabinet().id, p.cabinet().dimensions));
    EXPECT_THROW(us.begin_preview(std::make_unique<SetCabinetDimensions>(
                      p.cabinet().id, p.cabinet().dimensions)),
                 LogicError);
    us.cancel_preview(h);
}

TEST(UndoStackPreview, ExecuteBlockedDuringPreview) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(9));
    UndoStack us{p};
    auto h = us.begin_preview(std::make_unique<SetCabinetDimensions>(
        p.cabinet().id, p.cabinet().dimensions));
    EXPECT_THROW(us.execute(std::make_unique<AddPanel>(PanelRole::Top,
                                                         NoRoleParams{})),
                 LogicError);
    us.cancel_preview(h);
}

TEST(UndoStackPreview, MacroBlockedDuringPreview) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(9));
    UndoStack us{p};
    auto h = us.begin_preview(std::make_unique<SetCabinetDimensions>(
        p.cabinet().id, p.cabinet().dimensions));
    EXPECT_THROW(us.begin_macro("x"), LogicError);
    us.cancel_preview(h);
}

TEST(UndoStackPreview, PreviewInsideMacroThrows) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(9));
    UndoStack us{p};
    us.begin_macro("m");
    EXPECT_THROW(us.begin_preview(std::make_unique<SetCabinetDimensions>(
                      p.cabinet().id, p.cabinet().dimensions)),
                 LogicError);
    us.end_macro();
}
