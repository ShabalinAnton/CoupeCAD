#pragma once

#include "coupecad/core/commands/command.h"
#include "coupecad/core/id.h"
#include "coupecad/core/panel.h"

#include <optional>
#include <string>

namespace coupecad::core {

// Добавить новую панель. Конструктор НЕ назначает id — вместо этого
// id выдаётся в apply() (или пере-используется из предыдущего apply
// при redo после undo, чтобы внешние ссылки не ломались).
class AddPanel : public Command {
public:
    AddPanel(PanelRole role, RoleParams role_params);

    // Доступно только после первого apply().
    PanelId assigned_id() const noexcept { return assigned_id_; }

    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return "Add panel"; }
    CommandKind kind() const noexcept override { return CommandKind::AddPanel; }

private:
    PanelRole role_;
    RoleParams role_params_;
    PanelId assigned_id_{};   // назначается в apply
    bool applied_ = false;
};

class RemovePanel : public Command {
public:
    explicit RemovePanel(PanelId target);

    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return "Remove panel"; }
    CommandKind kind() const noexcept override { return CommandKind::RemovePanel; }

private:
    PanelId target_;
    std::optional<Panel> snapshot_;   // для revert
};

// UpdatePanelRoleParams: меняет role_params панели. Preview-совместимая.
// role остаётся прежней.
class UpdatePanelRoleParams : public PreviewableCommand {
public:
    UpdatePanelRoleParams(PanelId target, RoleParams new_value);
    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    ChangeSet update(Project& project, const std::any& new_value) override;
    std::string_view label() const noexcept override { return "Update panel parameters"; }
    CommandKind kind() const noexcept override { return CommandKind::UpdatePanelRoleParams; }
private:
    PanelId target_;
    RoleParams new_value_;
    RoleParams old_value_;
    bool applied_ = false;
};

class SetPanelMaterial : public PreviewableCommand {
public:
    SetPanelMaterial(PanelId target, std::optional<MaterialId> new_value);
    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    ChangeSet update(Project& project, const std::any& new_value) override;
    std::string_view label() const noexcept override { return "Set panel material"; }
    CommandKind kind() const noexcept override { return CommandKind::SetPanelMaterial; }
private:
    PanelId target_;
    std::optional<MaterialId> new_value_;
    std::optional<MaterialId> old_value_;
    bool applied_ = false;
};

class SetPanelThickness : public PreviewableCommand {
public:
    SetPanelThickness(PanelId target, std::optional<Millimeters> new_value);
    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    ChangeSet update(Project& project, const std::any& new_value) override;
    std::string_view label() const noexcept override { return "Set panel thickness"; }
    CommandKind kind() const noexcept override { return CommandKind::SetPanelThickness; }
private:
    PanelId target_;
    std::optional<Millimeters> new_value_;
    std::optional<Millimeters> old_value_;
    bool applied_ = false;
};

// Side — какая из четырёх сторон панели.
enum class PanelSide { Front, Back, Left, Right };

class SetPanelEdgeBanding : public Command {
public:
    SetPanelEdgeBanding(PanelId target, PanelSide side,
                        std::optional<EdgeBanding> new_value);
    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return "Set panel edge banding"; }
    CommandKind kind() const noexcept override { return CommandKind::SetPanelEdgeBanding; }
private:
    PanelId target_;
    PanelSide side_;
    std::optional<EdgeBanding> new_value_;
    std::optional<EdgeBanding> old_value_;
    bool applied_ = false;
};

class SetPanelLabel : public Command {
public:
    SetPanelLabel(PanelId target, std::optional<std::string> new_value);
    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return "Set panel label"; }
    CommandKind kind() const noexcept override { return CommandKind::SetPanelLabel; }
private:
    PanelId target_;
    std::optional<std::string> new_value_;
    std::optional<std::string> old_value_;
    bool applied_ = false;
};

class SetPanelGrain : public Command {
public:
    SetPanelGrain(PanelId target, GrainDirection new_value);
    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return "Set panel grain"; }
    CommandKind kind() const noexcept override { return CommandKind::SetPanelGrain; }
private:
    PanelId target_;
    GrainDirection new_value_;
    GrainDirection old_value_{};
    bool applied_ = false;
};

}  // namespace coupecad::core
