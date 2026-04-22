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

namespace {

Panel& get_panel_or_throw(Project& p, PanelId id, const char* code) {
    auto& cab = p.mutable_cabinet();
    auto it = cab.panels.find(id);
    if (it == cab.panels.end()) {
        throw DomainError{code, "Panel not found in cabinet"};
    }
    return it->second;
}

ChangeSet panel_updated(PanelId id) {
    ChangeSet cs;
    cs.updated_panels.push_back(id);
    return cs;
}

}  // namespace

// --- UpdatePanelRoleParams ---

UpdatePanelRoleParams::UpdatePanelRoleParams(PanelId target, RoleParams new_value)
    : target_(target), new_value_(std::move(new_value)) {}

ChangeSet UpdatePanelRoleParams::apply(Project& project) {
    auto& pan = get_panel_or_throw(project, target_,
                                    "panel.unknown_id");
    if (!is_role_params_valid_for(pan.role, new_value_)) {
        throw DomainError{"panel.role_params_mismatch",
                          "Incompatible role_params variant"};
    }
    if (!applied_) old_value_ = pan.role_params;
    pan.role_params = new_value_;
    applied_ = true;
    return panel_updated(target_);
}

ChangeSet UpdatePanelRoleParams::revert(Project& project) {
    auto& pan = get_panel_or_throw(project, target_, "panel.unknown_id");
    pan.role_params = old_value_;
    return panel_updated(target_);
}

ChangeSet UpdatePanelRoleParams::update(Project& project, const std::any& v) {
    auto* casted = std::any_cast<RoleParams>(&v);
    if (!casted) {
        throw LogicError{"command.preview.type_mismatch",
                         "UpdatePanelRoleParams::update expects RoleParams"};
    }
    auto& pan = get_panel_or_throw(project, target_, "panel.unknown_id");
    if (!is_role_params_valid_for(pan.role, *casted)) {
        throw DomainError{"panel.role_params_mismatch",
                          "Incompatible role_params variant"};
    }
    new_value_ = *casted;
    pan.role_params = *casted;
    return panel_updated(target_);
}

// --- SetPanelMaterial ---

SetPanelMaterial::SetPanelMaterial(PanelId target,
                                    std::optional<MaterialId> new_value)
    : target_(target), new_value_(new_value) {}

ChangeSet SetPanelMaterial::apply(Project& project) {
    if (new_value_ &&
        project.materials().find(*new_value_) == project.materials().end()) {
        throw DomainError{"project.panel_material_missing",
                          "material_override references unknown material"};
    }
    auto& pan = get_panel_or_throw(project, target_, "panel.unknown_id");
    if (!applied_) old_value_ = pan.material_override;
    pan.material_override = new_value_;
    applied_ = true;
    return panel_updated(target_);
}

ChangeSet SetPanelMaterial::revert(Project& project) {
    auto& pan = get_panel_or_throw(project, target_, "panel.unknown_id");
    pan.material_override = old_value_;
    return panel_updated(target_);
}

ChangeSet SetPanelMaterial::update(Project& project, const std::any& v) {
    auto* casted = std::any_cast<std::optional<MaterialId>>(&v);
    if (!casted) {
        throw LogicError{"command.preview.type_mismatch",
                         "SetPanelMaterial::update expects std::optional<MaterialId>"};
    }
    if (*casted &&
        project.materials().find(**casted) == project.materials().end()) {
        throw DomainError{"project.panel_material_missing",
                          "material_override references unknown material"};
    }
    auto& pan = get_panel_or_throw(project, target_, "panel.unknown_id");
    new_value_ = *casted;
    pan.material_override = *casted;
    return panel_updated(target_);
}

// --- SetPanelThickness ---

SetPanelThickness::SetPanelThickness(PanelId target,
                                      std::optional<Millimeters> new_value)
    : target_(target), new_value_(new_value) {
    if (new_value && new_value->value() <= 0) {
        throw DomainError{"panel.nonpositive_thickness",
                          "thickness must be > 0"};
    }
}

ChangeSet SetPanelThickness::apply(Project& project) {
    auto& pan = get_panel_or_throw(project, target_, "panel.unknown_id");
    if (!applied_) old_value_ = pan.thickness_override;
    pan.thickness_override = new_value_;
    applied_ = true;
    return panel_updated(target_);
}

ChangeSet SetPanelThickness::revert(Project& project) {
    auto& pan = get_panel_or_throw(project, target_, "panel.unknown_id");
    pan.thickness_override = old_value_;
    return panel_updated(target_);
}

ChangeSet SetPanelThickness::update(Project& project, const std::any& v) {
    auto* casted = std::any_cast<std::optional<Millimeters>>(&v);
    if (!casted) {
        throw LogicError{"command.preview.type_mismatch",
                         "SetPanelThickness::update expects std::optional<Millimeters>"};
    }
    if (*casted && (*casted)->value() <= 0) {
        throw DomainError{"panel.nonpositive_thickness",
                          "thickness must be > 0"};
    }
    auto& pan = get_panel_or_throw(project, target_, "panel.unknown_id");
    new_value_ = *casted;
    pan.thickness_override = *casted;
    return panel_updated(target_);
}

// --- SetPanelEdgeBanding ---

namespace {
std::optional<EdgeBanding>& edge_side(PanelEdgeBanding& eb, PanelSide side) {
    switch (side) {
        case PanelSide::Front: return eb.front;
        case PanelSide::Back:  return eb.back;
        case PanelSide::Left:  return eb.left;
        case PanelSide::Right: return eb.right;
    }
    // unreachable
    return eb.front;
}
}  // namespace

SetPanelEdgeBanding::SetPanelEdgeBanding(PanelId target, PanelSide side,
                                          std::optional<EdgeBanding> new_value)
    : target_(target), side_(side), new_value_(std::move(new_value)) {
    if (new_value_ &&
        (new_value_->thickness.value() <= 0 ||
         !new_value_->material_id.is_valid())) {
        throw DomainError{"panel.edge_banding_invalid",
                          "edge banding thickness or material invalid"};
    }
}

ChangeSet SetPanelEdgeBanding::apply(Project& project) {
    if (new_value_ &&
        project.materials().find(new_value_->material_id) ==
            project.materials().end()) {
        throw DomainError{"project.edge_banding_material_missing",
                          "edge banding material not in project"};
    }
    auto& pan = get_panel_or_throw(project, target_, "panel.unknown_id");
    auto& slot = edge_side(pan.edge_banding, side_);
    if (!applied_) old_value_ = slot;
    slot = new_value_;
    applied_ = true;
    return panel_updated(target_);
}

ChangeSet SetPanelEdgeBanding::revert(Project& project) {
    auto& pan = get_panel_or_throw(project, target_, "panel.unknown_id");
    edge_side(pan.edge_banding, side_) = old_value_;
    return panel_updated(target_);
}

// --- SetPanelLabel ---

SetPanelLabel::SetPanelLabel(PanelId target, std::optional<std::string> new_value)
    : target_(target), new_value_(std::move(new_value)) {}

ChangeSet SetPanelLabel::apply(Project& project) {
    auto& pan = get_panel_or_throw(project, target_, "panel.unknown_id");
    if (!applied_) old_value_ = pan.label;
    pan.label = new_value_;
    applied_ = true;
    return panel_updated(target_);
}

ChangeSet SetPanelLabel::revert(Project& project) {
    auto& pan = get_panel_or_throw(project, target_, "panel.unknown_id");
    pan.label = old_value_;
    return panel_updated(target_);
}

// --- SetPanelGrain ---

SetPanelGrain::SetPanelGrain(PanelId target, GrainDirection new_value)
    : target_(target), new_value_(new_value) {}

ChangeSet SetPanelGrain::apply(Project& project) {
    auto& pan = get_panel_or_throw(project, target_, "panel.unknown_id");
    if (!applied_) old_value_ = pan.grain_direction;
    pan.grain_direction = new_value_;
    applied_ = true;
    return panel_updated(target_);
}

ChangeSet SetPanelGrain::revert(Project& project) {
    auto& pan = get_panel_or_throw(project, target_, "panel.unknown_id");
    pan.grain_direction = old_value_;
    return panel_updated(target_);
}

}  // namespace coupecad::core
