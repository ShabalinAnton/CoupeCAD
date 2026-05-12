#include "coupecad/core/commands/cabinet_commands.h"

#include "coupecad/core/errors.h"
#include "coupecad/core/project.h"

namespace coupecad::core {

namespace {

void validate_dimensions(const Dimensions& d) {
    if (d.width.value() <= 0 || d.depth.value() <= 0 ||
        d.height.value() <= 0) {
        throw DomainError{"cabinet.nonpositive_dimensions",
                          "Cabinet dimensions must all be > 0"};
    }
}

}  // namespace

SetCabinetDimensions::SetCabinetDimensions(CabinetId target, Dimensions new_value)
    : target_(target), new_value_(new_value) {
    validate_dimensions(new_value);
}

ChangeSet SetCabinetDimensions::apply(Project& project) {
    auto& c = project.mutable_cabinet();
    if (c.id != target_) {
        throw DomainError{"cabinet.wrong_id", "SetCabinetDimensions target mismatch"};
    }
    if (!applied_) old_value_ = c.dimensions;
    c.dimensions = new_value_;
    applied_ = true;
    ChangeSet cs;
    cs.cabinet_changed = true;
    return cs;
}

ChangeSet SetCabinetDimensions::revert(Project& project) {
    auto& c = project.mutable_cabinet();
    if (c.id != target_) {
        throw DomainError{"cabinet.wrong_id", "SetCabinetDimensions revert target mismatch"};
    }
    c.dimensions = old_value_;
    ChangeSet cs;
    cs.cabinet_changed = true;
    return cs;
}

ChangeSet SetCabinetDimensions::update(Project& project, const std::any& new_value) {
    auto* casted = std::any_cast<Dimensions>(&new_value);
    if (!casted) {
        throw LogicError{"command.preview.type_mismatch",
                         "SetCabinetDimensions::update expects Dimensions"};
    }
    validate_dimensions(*casted);
    auto& c = project.mutable_cabinet();
    if (c.id != target_) {
        throw DomainError{"cabinet.wrong_id", "SetCabinetDimensions update target mismatch"};
    }
    new_value_ = *casted;
    c.dimensions = *casted;
    ChangeSet cs;
    cs.cabinet_changed = true;
    return cs;
}

SetCabinetDefaults::SetCabinetDefaults(CabinetId target,
                                       MaterialId panel_material,
                                       Millimeters panel_thickness,
                                       Millimeters back_thickness)
    : target_(target),
      new_material_(panel_material),
      new_panel_thickness_(panel_thickness),
      new_back_thickness_(back_thickness) {
    if (!panel_material.is_valid()) {
        throw DomainError{"cabinet.invalid_default_material",
                          "panel_material is invalid"};
    }
    if (panel_thickness.value() <= 0 || back_thickness.value() <= 0) {
        throw DomainError{"cabinet.nonpositive_thickness",
                          "Thicknesses must be > 0"};
    }
}

ChangeSet SetCabinetDefaults::apply(Project& project) {
    auto& c = project.mutable_cabinet();
    if (c.id != target_) {
        throw DomainError{"cabinet.wrong_id", "SetCabinetDefaults target mismatch"};
    }
    // Проверка, что материал существует в проекте.
    if (project.materials().find(new_material_) == project.materials().end()) {
        throw DomainError{"project.default_material_missing",
                          "panel_material not in project materials"};
    }
    if (!applied_) {
        old_material_ = c.default_panel_material;
        old_panel_thickness_ = c.default_panel_thickness;
        old_back_thickness_ = c.default_back_thickness;
    }
    c.default_panel_material = new_material_;
    c.default_panel_thickness = new_panel_thickness_;
    c.default_back_thickness = new_back_thickness_;
    applied_ = true;
    ChangeSet cs;
    cs.cabinet_changed = true;
    return cs;
}

ChangeSet SetCabinetDefaults::revert(Project& project) {
    auto& c = project.mutable_cabinet();
    c.default_panel_material = old_material_;
    c.default_panel_thickness = old_panel_thickness_;
    c.default_back_thickness = old_back_thickness_;
    ChangeSet cs;
    cs.cabinet_changed = true;
    return cs;
}

SetCabinetName::SetCabinetName(CabinetId target, std::string new_name)
    : target_(target), new_name_(std::move(new_name)) {
    if (new_name_.empty()) {
        throw DomainError{"cabinet.empty_name",
                          "Cabinet name must not be empty"};
    }
}

ChangeSet SetCabinetName::apply(Project& project) {
    auto& c = project.mutable_cabinet();
    if (c.id != target_) {
        throw DomainError{"cabinet.wrong_id",
                          "SetCabinetName target mismatch"};
    }
    if (!applied_) old_name_ = c.name;
    c.name = new_name_;
    applied_ = true;
    ChangeSet cs;
    cs.cabinet_changed = true;
    return cs;
}

ChangeSet SetCabinetName::revert(Project& project) {
    auto& c = project.mutable_cabinet();
    if (c.id != target_) {
        throw DomainError{"cabinet.wrong_id",
                          "SetCabinetName revert target mismatch"};
    }
    c.name = old_name_;
    ChangeSet cs;
    cs.cabinet_changed = true;
    return cs;
}

}  // namespace coupecad::core
