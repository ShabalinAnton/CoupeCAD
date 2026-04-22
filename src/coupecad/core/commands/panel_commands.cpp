#include "coupecad/core/commands/panel_commands.h"

#include "coupecad/core/errors.h"
#include "coupecad/core/project.h"

namespace coupecad::core {

AddPanel::AddPanel(PanelRole role, RoleParams role_params)
    : role_(role), role_params_(std::move(role_params)) {
    if (!is_role_params_valid_for(role, role_params_)) {
        throw DomainError{"panel.role_params_mismatch",
                          "role/role_params variant don't match"};
    }
}

ChangeSet AddPanel::apply(Project& project) {
    if (!applied_) {
        assigned_id_ = project.uuid_gen().next_id<PanelIdTag>();
    }
    Panel p;
    p.id = assigned_id_;
    p.role = role_;
    p.role_params = role_params_;
    p.validate();
    auto& cab = project.mutable_cabinet();
    if (cab.panels.find(assigned_id_) != cab.panels.end()) {
        throw LogicError{"panel.duplicate_id",
                         "AddPanel apply for already-present id"};
    }
    cab.panels.emplace(assigned_id_, std::move(p));
    applied_ = true;
    ChangeSet cs;
    cs.added_panels.push_back(assigned_id_);
    return cs;
}

ChangeSet AddPanel::revert(Project& project) {
    auto& cab = project.mutable_cabinet();
    auto it = cab.panels.find(assigned_id_);
    if (it == cab.panels.end()) {
        throw LogicError{"panel.missing_on_revert",
                         "AddPanel revert: panel not present"};
    }
    cab.panels.erase(it);
    ChangeSet cs;
    cs.removed_panels.push_back(assigned_id_);
    return cs;
}

RemovePanel::RemovePanel(PanelId target) : target_(target) {}

ChangeSet RemovePanel::apply(Project& project) {
    auto& cab = project.mutable_cabinet();
    auto it = cab.panels.find(target_);
    if (it == cab.panels.end()) {
        throw DomainError{"panel.unknown_id",
                          "RemovePanel: panel not in cabinet"};
    }
    // Проверка: ни один HardwareItem не должен ссылаться на этот PanelId.
    for (const auto& [_, hw] : cab.hardware) {
        for (const auto& a : hw.attachments) {
            if (a.panel_id == target_) {
                throw DomainError{"panel.still_referenced_by_hardware",
                                  "Cannot remove panel while hardware references it"};
            }
        }
    }
    snapshot_ = it->second;
    cab.panels.erase(it);
    ChangeSet cs;
    cs.removed_panels.push_back(target_);
    return cs;
}

ChangeSet RemovePanel::revert(Project& project) {
    if (!snapshot_) {
        throw LogicError{"panel.no_snapshot_on_revert",
                         "RemovePanel revert without prior apply"};
    }
    auto& cab = project.mutable_cabinet();
    cab.panels.emplace(target_, *snapshot_);
    ChangeSet cs;
    cs.added_panels.push_back(target_);
    return cs;
}

}  // namespace coupecad::core
