#pragma once

#include "coupecad/core/commands/command.h"
#include "coupecad/core/id.h"
#include "coupecad/core/panel.h"

#include <optional>

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

}  // namespace coupecad::core
