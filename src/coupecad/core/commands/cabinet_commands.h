#pragma once

#include "coupecad/core/commands/command.h"
#include "coupecad/core/id.h"
#include "coupecad/core/units.h"

namespace coupecad::core {

// Изменить габариты шкафа. Preview-совместимая.
class SetCabinetDimensions : public PreviewableCommand {
public:
    SetCabinetDimensions(CabinetId target, Dimensions new_value);

    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    ChangeSet update(Project& project, const std::any& new_value) override;
    std::string_view label() const noexcept override { return "Set cabinet dimensions"; }
    CommandKind kind() const noexcept override { return CommandKind::CabinetDimensions; }

private:
    CabinetId target_;
    Dimensions new_value_;
    Dimensions old_value_{};
    bool applied_ = false;
};

// Изменить дефолты корпуса: материал, толщину панелей/спинки.
class SetCabinetDefaults : public Command {
public:
    SetCabinetDefaults(CabinetId target,
                       MaterialId panel_material,
                       Millimeters panel_thickness,
                       Millimeters back_thickness);

    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return "Set cabinet defaults"; }
    CommandKind kind() const noexcept override { return CommandKind::CabinetDefaults; }

private:
    CabinetId target_;
    MaterialId new_material_;
    Millimeters new_panel_thickness_;
    Millimeters new_back_thickness_;
    MaterialId old_material_{};
    Millimeters old_panel_thickness_{};
    Millimeters old_back_thickness_{};
    bool applied_ = false;
};

}  // namespace coupecad::core
