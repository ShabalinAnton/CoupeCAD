#include "coupecad/core/commands/change_set.h"
#include "coupecad/core/commands/command.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

TEST(ChangeSet, DefaultIsEmpty) {
    ChangeSet cs;
    EXPECT_TRUE(cs.empty());
}

TEST(ChangeSet, AnyFieldMakesNonEmpty) {
    {
        ChangeSet cs;
        cs.added_panels.push_back(PanelId{});
        EXPECT_FALSE(cs.empty());
    }
    {
        ChangeSet cs;
        cs.cabinet_changed = true;
        EXPECT_FALSE(cs.empty());
    }
    {
        ChangeSet cs;
        cs.updated_materials.push_back(MaterialId{});
        EXPECT_FALSE(cs.empty());
    }
}

TEST(ChangeSet, MergeConcatenates) {
    ChangeSet a;
    a.added_panels.push_back(PanelId{});
    ChangeSet b;
    b.added_panels.push_back(PanelId{});
    b.removed_hardware.push_back(HardwareItemId{});
    a.merge(b);
    EXPECT_EQ(a.added_panels.size(), 2u);
    EXPECT_EQ(a.removed_hardware.size(), 1u);
}

TEST(ChangeSet, MergePropagatesCabinetChanged) {
    ChangeSet a;
    ChangeSet b;
    b.cabinet_changed = true;
    a.merge(b);
    EXPECT_TRUE(a.cabinet_changed);
}

TEST(CommandKind, KindEnumCompiles) {
    // Просто фиксируем, что enum виден и имеет ожидаемые значения.
    EXPECT_NE(static_cast<int>(CommandKind::AddPanel),
              static_cast<int>(CommandKind::RemovePanel));
    EXPECT_NE(static_cast<int>(CommandKind::AddMaterial),
              static_cast<int>(CommandKind::Macro));
}
