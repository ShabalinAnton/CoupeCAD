#pragma once

#include "coupecad/core/commands/command.h"
#include "coupecad/core/id.h"
#include "coupecad/core/material.h"

#include <optional>

namespace coupecad::core {

class AddMaterial : public Command {
public:
    // Если у material.id уже valid — сохраняется (для redo); иначе назначается
    // новый из UuidGenerator при первом apply.
    explicit AddMaterial(Material material);
    MaterialId assigned_id() const noexcept { return assigned_id_; }
    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return "Add material"; }
    CommandKind kind() const noexcept override { return CommandKind::AddMaterial; }
private:
    Material material_;
    MaterialId assigned_id_{};
    bool applied_ = false;
};

class UpdateMaterial : public Command {
public:
    UpdateMaterial(MaterialId target, Material new_value);
    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return "Update material"; }
    CommandKind kind() const noexcept override { return CommandKind::UpdateMaterial; }
private:
    MaterialId target_;
    Material new_value_;
    Material old_value_;
    bool applied_ = false;
};

class RemoveMaterial : public Command {
public:
    explicit RemoveMaterial(MaterialId target);
    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return "Remove material"; }
    CommandKind kind() const noexcept override { return CommandKind::RemoveMaterial; }
private:
    MaterialId target_;
    std::optional<Material> snapshot_;
};

}  // namespace coupecad::core
