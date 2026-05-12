#pragma once

#include <QObject>

namespace coupecad::core { class UndoStack; }

namespace coupecad::ui {

// Тонкая Qt-обёртка над core::UndoStack для QML-биндингов и Shortcut'ов.
// core::UndoStack остаётся Qt-free.
class UndoStackProxy : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool canUndo READ can_undo NOTIFY changed)
    Q_PROPERTY(bool canRedo READ can_redo NOTIFY changed)

public:
    explicit UndoStackProxy(core::UndoStack& undo, QObject* parent = nullptr);

    bool can_undo() const;
    bool can_redo() const;

    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

signals:
    void changed();

private:
    core::UndoStack& undo_;
};

}  // namespace coupecad::ui
