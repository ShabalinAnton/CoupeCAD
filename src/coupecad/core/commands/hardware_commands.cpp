#include "coupecad/core/commands/hardware_commands.h"

#include "coupecad/core/errors.h"
#include "coupecad/core/project.h"

namespace coupecad::core {

namespace {
void check_attachments(const Project& p,
                       const std::vector<PanelAttachment>& atts) {
    if (atts.empty()) {
        throw DomainError{"hardware_item.no_attachments",
                          "At least one attachment required"};
    }
    for (const auto& a : atts) {
        if (!a.panel_id.is_valid()) {
            throw DomainError{"hardware_item.invalid_attachment_panel",
                              "attachment has invalid panel_id"};
        }
        if (p.cabinet().panels.find(a.panel_id) == p.cabinet().panels.end()) {
            throw DomainError{"cabinet.hardware_unknown_panel",
                              "attachment references unknown panel"};
        }
    }
}
}  // namespace

AddHardware::AddHardware(HardwareRef ref,
                         std::vector<PanelAttachment> attachments)
    : ref_(std::move(ref)), attachments_(std::move(attachments)) {
    if (ref_.value().empty()) {
        throw DomainError{"hardware_item.empty_ref",
                          "HardwareRef is empty"};
    }
}

ChangeSet AddHardware::apply(Project& project) {
    if (project.hardware_catalog().find(ref_) ==
        project.hardware_catalog().end()) {
        throw DomainError{"project.hardware_ref_missing",
                          "HardwareRef not in hardware_catalog"};
    }
    check_attachments(project, attachments_);
    if (!applied_) {
        assigned_id_ = project.uuid_gen().next_id<HardwareItemIdTag>();
    }
    HardwareItem hw;
    hw.id = assigned_id_;
    hw.ref = ref_;
    hw.attachments = attachments_;
    auto& cab = project.mutable_cabinet();
    cab.hardware.emplace(assigned_id_, std::move(hw));
    applied_ = true;
    ChangeSet cs;
    cs.added_hardware.push_back(assigned_id_);
    return cs;
}

ChangeSet AddHardware::revert(Project& project) {
    auto& cab = project.mutable_cabinet();
    cab.hardware.erase(assigned_id_);
    ChangeSet cs;
    cs.removed_hardware.push_back(assigned_id_);
    return cs;
}

RemoveHardware::RemoveHardware(HardwareItemId target) : target_(target) {}

ChangeSet RemoveHardware::apply(Project& project) {
    auto& cab = project.mutable_cabinet();
    auto it = cab.hardware.find(target_);
    if (it == cab.hardware.end()) {
        throw DomainError{"hardware_item.unknown_id",
                          "RemoveHardware: target not in cabinet"};
    }
    snapshot_ = it->second;
    cab.hardware.erase(it);
    ChangeSet cs;
    cs.removed_hardware.push_back(target_);
    return cs;
}

ChangeSet RemoveHardware::revert(Project& project) {
    if (!snapshot_) {
        throw LogicError{"hardware_item.no_snapshot_on_revert",
                         "RemoveHardware revert without apply"};
    }
    project.mutable_cabinet().hardware[target_] = *snapshot_;
    ChangeSet cs;
    cs.added_hardware.push_back(target_);
    return cs;
}

UpdateHardwareAttachments::UpdateHardwareAttachments(
    HardwareItemId target, std::vector<PanelAttachment> new_attachments)
    : target_(target), new_value_(std::move(new_attachments)) {}

ChangeSet UpdateHardwareAttachments::apply(Project& project) {
    auto& cab = project.mutable_cabinet();
    auto it = cab.hardware.find(target_);
    if (it == cab.hardware.end()) {
        throw DomainError{"hardware_item.unknown_id",
                          "UpdateHardwareAttachments: target not in cabinet"};
    }
    check_attachments(project, new_value_);
    if (!applied_) old_value_ = it->second.attachments;
    it->second.attachments = new_value_;
    applied_ = true;
    ChangeSet cs;
    cs.updated_hardware.push_back(target_);
    return cs;
}

ChangeSet UpdateHardwareAttachments::revert(Project& project) {
    project.mutable_cabinet().hardware.at(target_).attachments = old_value_;
    ChangeSet cs;
    cs.updated_hardware.push_back(target_);
    return cs;
}

}  // namespace coupecad::core
