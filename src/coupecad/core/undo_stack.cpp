#include "coupecad/core/undo_stack.h"

#include "coupecad/core/commands/macro_command.h"
#include "coupecad/core/errors.h"

#include <algorithm>

namespace coupecad::core {

UndoStack::UndoStack(Project& project) : project_(project) {}
UndoStack::~UndoStack() = default;

void UndoStack::execute(std::unique_ptr<Command> cmd) {
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
}

void UndoStack::begin_macro(std::string label) {
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

}  // namespace coupecad::core
