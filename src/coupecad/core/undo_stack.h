#pragma once

#include "coupecad/core/commands/change_set.h"
#include "coupecad/core/commands/command.h"
#include "coupecad/core/project.h"

#include <any>
#include <memory>
#include <string_view>
#include <vector>

namespace coupecad::core {

class MacroCommand;

class UndoStack {
public:
    explicit UndoStack(Project& project);
    ~UndoStack();

    UndoStack(const UndoStack&) = delete;
    UndoStack& operator=(const UndoStack&) = delete;

    // Discrete execute.
    void execute(std::unique_ptr<Command> cmd);

    void undo();
    void redo();
    bool can_undo() const noexcept { return !undo_.empty(); }
    bool can_redo() const noexcept { return !redo_.empty(); }
    std::size_t undo_depth() const noexcept { return undo_.size(); }
    std::size_t redo_depth() const noexcept { return redo_.size(); }

    // Labels текущих команд (для UI).
    std::vector<std::string_view> undo_labels() const;

    void clear() noexcept;

    // Макросы — тесты для них в Task 10.
    void begin_macro(std::string label);
    void end_macro();

    // Наблюдатели.
    void add_observer(IProjectObserver* obs);
    void remove_observer(IProjectObserver* obs);

    // --- Live preview (см. spec §3.3.2) ---
    class PreviewHandle {
    public:
        PreviewHandle() = default;
        bool is_active() const noexcept { return active_; }
    private:
        friend class UndoStack;
        bool active_ = false;
    };

    PreviewHandle begin_preview(std::unique_ptr<PreviewableCommand> cmd);
    void update_preview(PreviewHandle& handle, const std::any& new_value);
    void commit_preview(PreviewHandle& handle);
    void cancel_preview(PreviewHandle& handle);

    bool preview_active() const noexcept { return active_preview_ != nullptr; }

private:
    Project& project_;
    std::vector<std::unique_ptr<Command>> undo_;
    std::vector<std::unique_ptr<Command>> redo_;
    std::unique_ptr<MacroCommand> pending_macro_;
    std::unique_ptr<PreviewableCommand> active_preview_;
    std::vector<IProjectObserver*> observers_;

    void notify(const ChangeSet& cs);
};

}  // namespace coupecad::core
