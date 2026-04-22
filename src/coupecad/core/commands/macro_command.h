#pragma once

#include "coupecad/core/commands/command.h"

#include <memory>
#include <string>
#include <vector>

namespace coupecad::core {

// Composite-команда: список других команд, применяется и откатывается
// атомарно. Label задаётся при конструировании.
class MacroCommand : public Command {
public:
    explicit MacroCommand(std::string label);

    // Добавить под-команду. Выполняется только через apply() макро-команды.
    void push(std::unique_ptr<Command> cmd);

    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return label_; }
    CommandKind kind() const noexcept override { return CommandKind::Macro; }

    bool empty() const noexcept { return children_.empty(); }
    std::size_t size() const noexcept { return children_.size(); }

private:
    std::string label_;
    std::vector<std::unique_ptr<Command>> children_;
    bool applied_ = false;
};

}  // namespace coupecad::core
