#pragma once

#include "coupecad/core/commands/command.h"
#include "coupecad/core/hardware.h"
#include "coupecad/core/id.h"

#include <optional>

namespace coupecad::core {

class AddHardwareSpec : public Command {
public:
    explicit AddHardwareSpec(HardwareSpec spec);
    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return "Add hardware spec"; }
    CommandKind kind() const noexcept override { return CommandKind::AddHardwareSpec; }
private:
    HardwareSpec spec_;
    bool applied_ = false;
};

class UpdateHardwareSpec : public Command {
public:
    UpdateHardwareSpec(HardwareRef target, HardwareSpec new_value);
    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return "Update hardware spec"; }
    CommandKind kind() const noexcept override { return CommandKind::UpdateHardwareSpec; }
private:
    HardwareRef target_;
    HardwareSpec new_value_;
    HardwareSpec old_value_;
    bool applied_ = false;
};

class RemoveHardwareSpec : public Command {
public:
    explicit RemoveHardwareSpec(HardwareRef target);
    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return "Remove hardware spec"; }
    CommandKind kind() const noexcept override { return CommandKind::RemoveHardwareSpec; }
private:
    HardwareRef target_;
    std::optional<HardwareSpec> snapshot_;
};

}  // namespace coupecad::core
