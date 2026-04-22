#include "coupecad/core/undo_stack.h"

#include "coupecad/core/commands/macro_command.h"
#include "coupecad/core/errors.h"

#include <algorithm>

namespace coupecad::core {

UndoStack::UndoStack(Project& project) : project_(project) {}
UndoStack::~UndoStack() = default;

void UndoStack::execute(std::unique_ptr<Command> cmd) {
    if (active_preview_) {
        throw LogicError{"undo.execute_during_preview",
                         "execute() not allowed while preview is active"};
    }
    if (pending_macro_) {
        auto cs = cmd->apply(project_);
        pending_macro_->push(std::move(cmd));
        notify(cs);
        return;
    }
    auto cs = cmd->apply(project_);
    undo_.push_back(std::move(cmd));
    redo_.clear();
    notify(cs);
}

void UndoStack::undo() {
    if (undo_.empty()) return;
    auto cmd = std::move(undo_.back());
    undo_.pop_back();
    auto cs = cmd->revert(project_);
    redo_.push_back(std::move(cmd));
    notify(cs);
}

void UndoStack::redo() {
    if (redo_.empty()) return;
    auto cmd = std::move(redo_.back());
    redo_.pop_back();
    auto cs = cmd->apply(project_);
    undo_.push_back(std::move(cmd));
    notify(cs);
}

std::vector<std::string_view> UndoStack::undo_labels() const {
    std::vector<std::string_view> out;
    out.reserve(undo_.size());
    for (const auto& c : undo_) out.push_back(c->label());
    return out;
}

void UndoStack::clear() noexcept {
    undo_.clear();
    redo_.clear();
    pending_macro_.reset();
    active_preview_.reset();
}

void UndoStack::begin_macro(std::string label) {
    if (active_preview_) {
        throw LogicError{"undo.macro_during_preview",
                         "begin_macro not allowed while preview is active"};
    }
    if (pending_macro_) {
        throw LogicError{"undo.macro_already_active",
                         "begin_macro called while macro already active"};
    }
    pending_macro_ = std::make_unique<MacroCommand>(std::move(label));
}

void UndoStack::end_macro() {
    if (!pending_macro_) {
        throw LogicError{"undo.no_macro_to_end",
                         "end_macro without begin_macro"};
    }
    if (!pending_macro_->empty()) {
        undo_.push_back(std::move(pending_macro_));
        redo_.clear();
    } else {
        pending_macro_.reset();
    }
}

void UndoStack::add_observer(IProjectObserver* o) {
    observers_.push_back(o);
}

void UndoStack::remove_observer(IProjectObserver* o) {
    observers_.erase(std::remove(observers_.begin(), observers_.end(), o),
                     observers_.end());
}

void UndoStack::notify(const ChangeSet& cs) {
    for (auto* o : observers_) {
        o->on_changed(project_, cs);
    }
}

UndoStack::PreviewHandle UndoStack::begin_preview(
    std::unique_ptr<PreviewableCommand> cmd) {
    if (pending_macro_) {
        throw LogicError{"undo.preview_inside_macro",
                         "Live preview not allowed inside a macro"};
    }
    if (active_preview_) {
        throw LogicError{"undo.preview_already_active",
                         "Another preview session is already active"};
    }
    auto cs = cmd->apply(project_);
    active_preview_ = std::move(cmd);
    PreviewHandle h;
    h.active_ = true;
    notify(cs);
    return h;
}

void UndoStack::update_preview(PreviewHandle& h, const std::any& v) {
    if (!h.active_ || !active_preview_) {
        throw LogicError{"undo.preview_not_active",
                         "update_preview without active session"};
    }
    auto cs = active_preview_->update(project_, v);
    notify(cs);
}

void UndoStack::commit_preview(PreviewHandle& h) {
    if (!h.active_ || !active_preview_) {
        throw LogicError{"undo.preview_not_active",
                         "commit_preview without active session"};
    }
    // Команда уже применена в initial→current. Кладём её как Discrete-запись.
    undo_.push_back(std::move(active_preview_));
    redo_.clear();
    h.active_ = false;
}

void UndoStack::cancel_preview(PreviewHandle& h) {
    if (!h.active_ || !active_preview_) {
        throw LogicError{"undo.preview_not_active",
                         "cancel_preview without active session"};
    }
    auto cs = active_preview_->revert(project_);
    active_preview_.reset();
    h.active_ = false;
    notify(cs);
}

}  // namespace coupecad::core
