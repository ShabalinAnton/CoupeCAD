#include "coupecad/ui/undo_stack_proxy.h"

#include "coupecad/core/undo_stack.h"
#include "coupecad/logging/logger.h"

namespace coupecad::ui {

UndoStackProxy::UndoStackProxy(core::UndoStack& undo, QObject* parent)
    : QObject(parent), undo_(undo) {
    coupecad::logging::Logger::instance().info(
        "ui", "UndoStackProxy constructed");
}

bool UndoStackProxy::can_undo() const { return undo_.can_undo(); }
bool UndoStackProxy::can_redo() const { return undo_.can_redo(); }

void UndoStackProxy::undo() {
    if (!undo_.can_undo()) return;
    undo_.undo();
    emit changed();
}

void UndoStackProxy::redo() {
    if (!undo_.can_redo()) return;
    undo_.redo();
    emit changed();
}

}  // namespace coupecad::ui
