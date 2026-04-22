#include "coupecad/core/commands/macro_command.h"

#include "coupecad/core/errors.h"
#include "coupecad/core/project.h"

namespace coupecad::core {

MacroCommand::MacroCommand(std::string label) : label_(std::move(label)) {}

void MacroCommand::push(std::unique_ptr<Command> cmd) {
    if (applied_) {
        throw LogicError{"macro.push_after_apply",
                         "Cannot push into an already-applied MacroCommand"};
    }
    children_.push_back(std::move(cmd));
}

ChangeSet MacroCommand::apply(Project& project) {
    ChangeSet acc;
    std::size_t done = 0;
    try {
        for (; done < children_.size(); ++done) {
            acc.merge(children_[done]->apply(project));
        }
    } catch (...) {
        // Откатить уже применённые под-команды в обратном порядке.
        while (done > 0) {
            --done;
            try { children_[done]->revert(project); } catch (...) {}
        }
        throw;
    }
    applied_ = true;
    return acc;
}

ChangeSet MacroCommand::revert(Project& project) {
    ChangeSet acc;
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        acc.merge((*it)->revert(project));
    }
    return acc;
}

}  // namespace coupecad::core
