#include "coupecad/core/commands/hardware_spec_commands.h"

#include "coupecad/core/errors.h"
#include "coupecad/core/project.h"

namespace coupecad::core {

AddHardwareSpec::AddHardwareSpec(HardwareSpec spec) : spec_(std::move(spec)) {
    spec_.validate();
}

ChangeSet AddHardwareSpec::apply(Project& project) {
    auto& cat = project.mutable_hardware_catalog();
    if (cat.find(spec_.ref) != cat.end()) {
        throw DomainError{"hardware_spec.duplicate_ref",
                          "HardwareSpec with this ref already exists"};
    }
    cat[spec_.ref] = spec_;
    applied_ = true;
    return ChangeSet{};
}

ChangeSet AddHardwareSpec::revert(Project& project) {
    project.mutable_hardware_catalog().erase(spec_.ref);
    return ChangeSet{};
}

UpdateHardwareSpec::UpdateHardwareSpec(HardwareRef target, HardwareSpec new_value)
    : target_(std::move(target)), new_value_(std::move(new_value)) {
    new_value_.ref = target_;   // ref неизменен
    new_value_.validate();
}

ChangeSet UpdateHardwareSpec::apply(Project& project) {
    auto& cat = project.mutable_hardware_catalog();
    auto it = cat.find(target_);
    if (it == cat.end()) {
        throw DomainError{"hardware_spec.unknown_ref",
                          "UpdateHardwareSpec: target not in catalog"};
    }
    if (!applied_) old_value_ = it->second;
    it->second = new_value_;
    applied_ = true;
    return ChangeSet{};
}

ChangeSet UpdateHardwareSpec::revert(Project& project) {
    project.mutable_hardware_catalog()[target_] = old_value_;
    return ChangeSet{};
}

RemoveHardwareSpec::RemoveHardwareSpec(HardwareRef target) : target_(std::move(target)) {}

ChangeSet RemoveHardwareSpec::apply(Project& project) {
    auto& cat = project.mutable_hardware_catalog();
    auto it = cat.find(target_);
    if (it == cat.end()) {
        throw DomainError{"hardware_spec.unknown_ref",
                          "RemoveHardwareSpec: target not in catalog"};
    }
    for (const auto& [_, hw] : project.cabinet().hardware) {
        if (hw.ref == target_) {
            throw DomainError{"hardware_spec.still_in_use",
                              "HardwareSpec in use by a HardwareItem"};
        }
    }
    snapshot_ = it->second;
    cat.erase(it);
    return ChangeSet{};
}

ChangeSet RemoveHardwareSpec::revert(Project& project) {
    if (!snapshot_) {
        throw LogicError{"hardware_spec.no_snapshot_on_revert",
                         "RemoveHardwareSpec revert without apply"};
    }
    project.mutable_hardware_catalog()[target_] = *snapshot_;
    return ChangeSet{};
}

}  // namespace coupecad::core
