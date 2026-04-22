#pragma once

#include "coupecad/core/commands/command.h"
#include "coupecad/core/hardware.h"
#include "coupecad/core/id.h"

#include <optional>
#include <vector>

namespace coupecad::core {

class AddHardware : public Command {
public:
    AddHardware(HardwareRef ref, std::vector<PanelAttachment> attachments);
    HardwareItemId assigned_id() const noexcept { return assigned_id_; }
    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return "Add hardware"; }
    CommandKind kind() const noexcept override { return CommandKind::AddHardware; }
private:
    HardwareRef ref_;
    std::vector<PanelAttachment> attachments_;
    HardwareItemId assigned_id_{};
    bool applied_ = false;
};

class RemoveHardware : public Command {
public:
    explicit RemoveHardware(HardwareItemId target);
    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return "Remove hardware"; }
    CommandKind kind() const noexcept override { return CommandKind::RemoveHardware; }
private:
    HardwareItemId target_;
    std::optional<HardwareItem> snapshot_;
};

class UpdateHardwareAttachments : public Command {
public:
    UpdateHardwareAttachments(HardwareItemId target,
                               std::vector<PanelAttachment> new_attachments);
    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return "Update hardware attachments"; }
    CommandKind kind() const noexcept override { return CommandKind::UpdateHardwareAttachments; }
private:
    HardwareItemId target_;
    std::vector<PanelAttachment> new_value_;
    std::vector<PanelAttachment> old_value_;
    bool applied_ = false;
};

}  // namespace coupecad::core
