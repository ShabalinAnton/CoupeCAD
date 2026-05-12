#include "coupecad/ui/undo_stack_proxy.h"

#include "coupecad/core/commands/cabinet_commands.h"
#include "coupecad/core/project.h"
#include "coupecad/core/undo_stack.h"

#include <QCoreApplication>
#include <QSignalSpy>

#include <gtest/gtest.h>

namespace {

bool s_qapp_initialised = false;
QCoreApplication* s_qapp = nullptr;

void ensure_qapp() {
    if (s_qapp_initialised) return;
    static int    argc = 1;
    static char   arg0[] = "ui_test";
    static char*  argv[] = {arg0, nullptr};
    s_qapp = new QCoreApplication(argc, argv);
    s_qapp_initialised = true;
}

}  // namespace

using coupecad::core::Project;
using coupecad::core::SetCabinetName;
using coupecad::core::UndoStack;
using coupecad::ui::UndoStackProxy;

TEST(UndoStackProxyTest, CanUndoFalseInitially) {
    ensure_qapp();
    Project project = Project::create_empty("Initial");
    UndoStack undo{project};
    UndoStackProxy proxy{undo};
    EXPECT_FALSE(proxy.can_undo());
    EXPECT_FALSE(proxy.can_redo());
}

TEST(UndoStackProxyTest, UndoDelegatesToCoreStack) {
    ensure_qapp();
    Project project = Project::create_empty("Initial");
    UndoStack undo{project};
    UndoStackProxy proxy{undo};

    const std::string original = project.cabinet().name;
    undo.execute(std::make_unique<SetCabinetName>(project.cabinet().id, "Renamed"));
    ASSERT_EQ(project.cabinet().name, "Renamed");

    proxy.undo();
    EXPECT_EQ(project.cabinet().name, original);
}

TEST(UndoStackProxyTest, RedoDelegatesToCoreStack) {
    ensure_qapp();
    Project project = Project::create_empty("Initial");
    UndoStack undo{project};
    UndoStackProxy proxy{undo};

    const std::string original = project.cabinet().name;
    undo.execute(std::make_unique<SetCabinetName>(project.cabinet().id, "Renamed"));
    proxy.undo();
    ASSERT_EQ(project.cabinet().name, original);

    proxy.redo();
    EXPECT_EQ(project.cabinet().name, "Renamed");
}

TEST(UndoStackProxyTest, UndoEmitsChanged) {
    ensure_qapp();
    Project project = Project::create_empty("Initial");
    UndoStack undo{project};
    UndoStackProxy proxy{undo};
    undo.execute(std::make_unique<SetCabinetName>(project.cabinet().id, "Renamed"));

    QSignalSpy spy(&proxy, &UndoStackProxy::changed);
    proxy.undo();
    EXPECT_EQ(spy.count(), 1);
}
