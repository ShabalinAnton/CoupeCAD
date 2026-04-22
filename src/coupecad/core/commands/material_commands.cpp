#include "coupecad/core/commands/material_commands.h"

#include "coupecad/core/errors.h"
#include "coupecad/core/project.h"

namespace coupecad::core {

AddMaterial::AddMaterial(Material m) : material_(std::move(m)) {}

ChangeSet AddMaterial::apply(Project& project) {
    if (!applied_) {
        if (!material_.id.is_valid()) {
            material_.id = project.uuid_gen().next_id<MaterialIdTag>();
        }
        assigned_id_ = material_.id;
    }
    material_.validate();
    auto& mats = project.mutable_materials();
    if (mats.find(assigned_id_) != mats.end()) {
        throw LogicError{"material.duplicate_id",
                         "AddMaterial apply for already-present id"};
    }
    mats.emplace(assigned_id_, material_);
    applied_ = true;
    ChangeSet cs;
    cs.added_materials.push_back(assigned_id_);
    return cs;
}

ChangeSet AddMaterial::revert(Project& project) {
    auto& mats = project.mutable_materials();
    auto it = mats.find(assigned_id_);
    if (it == mats.end()) {
        throw LogicError{"material.missing_on_revert",
                         "AddMaterial revert: not present"};
    }
    mats.erase(it);
    ChangeSet cs;
    cs.removed_materials.push_back(assigned_id_);
    return cs;
}

UpdateMaterial::UpdateMaterial(MaterialId target, Material new_value)
    : target_(target), new_value_(std::move(new_value)) {
    new_value_.id = target_;   // нельзя менять id через update
}

ChangeSet UpdateMaterial::apply(Project& project) {
    auto& mats = project.mutable_materials();
    auto it = mats.find(target_);
    if (it == mats.end()) {
        throw DomainError{"material.unknown_id",
                          "UpdateMaterial: target not in project"};
    }
    new_value_.validate();
    if (!applied_) old_value_ = it->second;
    it->second = new_value_;
    applied_ = true;
    ChangeSet cs;
    cs.updated_materials.push_back(target_);
    return cs;
}

ChangeSet UpdateMaterial::revert(Project& project) {
    auto& mats = project.mutable_materials();
    mats[target_] = old_value_;
    ChangeSet cs;
    cs.updated_materials.push_back(target_);
    return cs;
}

RemoveMaterial::RemoveMaterial(MaterialId target) : target_(target) {}

ChangeSet RemoveMaterial::apply(Project& project) {
    auto& mats = project.mutable_materials();
    auto it = mats.find(target_);
    if (it == mats.end()) {
        throw DomainError{"material.unknown_id",
                          "RemoveMaterial: target not in project"};
    }
    // Проверка: никакая панель и не default_panel_material не ссылаются.
    if (project.cabinet().default_panel_material == target_) {
        throw DomainError{"material.still_used_as_cabinet_default",
                          "Material in use as cabinet.default_panel_material"};
    }
    for (const auto& [_, pan] : project.cabinet().panels) {
        if (pan.material_override == target_) {
            throw DomainError{"material.still_used_by_panel_override",
                              "Material in use as panel.material_override"};
        }
        auto eb_uses = [&](const std::optional<EdgeBanding>& eb) {
            return eb && eb->material_id == target_;
        };
        if (eb_uses(pan.edge_banding.front) || eb_uses(pan.edge_banding.back) ||
            eb_uses(pan.edge_banding.left)  || eb_uses(pan.edge_banding.right)) {
            throw DomainError{"material.still_used_by_edge_banding",
                              "Material in use by edge banding"};
        }
    }
    snapshot_ = it->second;
    mats.erase(it);
    ChangeSet cs;
    cs.removed_materials.push_back(target_);
    return cs;
}

ChangeSet RemoveMaterial::revert(Project& project) {
    if (!snapshot_) {
        throw LogicError{"material.no_snapshot_on_revert",
                         "RemoveMaterial revert without prior apply"};
    }
    project.mutable_materials()[target_] = *snapshot_;
    ChangeSet cs;
    cs.added_materials.push_back(target_);
    return cs;
}

}  // namespace coupecad::core
