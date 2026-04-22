#include "coupecad/core/hardware.h"

#include "coupecad/core/errors.h"

namespace coupecad::core {

const char* hardware_kind_name(HardwareKind kind) noexcept {
    switch (kind) {
        case HardwareKind::Hinge:        return "Hinge";
        case HardwareKind::DrawerSlide:  return "DrawerSlide";
        case HardwareKind::Handle:       return "Handle";
        case HardwareKind::ShelfSupport: return "ShelfSupport";
        case HardwareKind::Connector:    return "Connector";
        case HardwareKind::GasLift:      return "GasLift";
        case HardwareKind::Other:        return "Other";
    }
    return "?";
}

void HardwareSpec::validate() const {
    if (ref.value().empty()) {
        throw DomainError{"hardware_spec.empty_ref",
                          "HardwareSpec ref is empty"};
    }
    if (name.empty()) {
        throw DomainError{"hardware_spec.empty_name",
                          "HardwareSpec name is empty"};
    }
    if (bbox.x.value() <= 0 || bbox.y.value() <= 0 || bbox.z.value() <= 0) {
        throw DomainError{"hardware_spec.nonpositive_bbox",
                          "HardwareSpec bbox dimensions must be > 0"};
    }
}

void HardwareItem::validate() const {
    if (!id.is_valid()) {
        throw DomainError{"hardware_item.invalid_id",
                          "HardwareItem has invalid id"};
    }
    if (ref.value().empty()) {
        throw DomainError{"hardware_item.empty_ref",
                          "HardwareItem ref is empty"};
    }
    if (attachments.empty()) {
        throw DomainError{"hardware_item.no_attachments",
                          "HardwareItem must have at least one attachment"};
    }
    for (const auto& a : attachments) {
        if (!a.panel_id.is_valid()) {
            throw DomainError{"hardware_item.invalid_attachment_panel",
                              "PanelAttachment has invalid panel_id"};
        }
    }
}

}  // namespace coupecad::core
