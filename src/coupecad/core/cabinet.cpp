#include "coupecad/core/cabinet.h"

#include "coupecad/core/errors.h"

namespace coupecad::core {

void Cabinet::validate() const {
    if (!id.is_valid()) {
        throw DomainError{"cabinet.invalid_id", "Cabinet has invalid id"};
    }
    if (dimensions.width.value() <= 0 ||
        dimensions.depth.value() <= 0 ||
        dimensions.height.value() <= 0) {
        throw DomainError{"cabinet.nonpositive_dimensions",
                          "Cabinet dimensions must all be > 0"};
    }
    if (!default_panel_material.is_valid()) {
        throw DomainError{"cabinet.invalid_default_material",
                          "Cabinet default_panel_material is invalid"};
    }
    if (default_panel_thickness.value() <= 0) {
        throw DomainError{"cabinet.nonpositive_panel_thickness",
                          "Cabinet default_panel_thickness must be > 0"};
    }
    if (default_back_thickness.value() <= 0) {
        throw DomainError{"cabinet.nonpositive_back_thickness",
                          "Cabinet default_back_thickness must be > 0"};
    }
    for (const auto& [key, panel] : panels) {
        if (key != panel.id) {
            throw DomainError{"cabinet.panel_key_mismatch",
                              "Cabinet.panels key does not match panel.id"};
        }
        panel.validate();
    }
    for (const auto& [key, hw] : hardware) {
        if (key != hw.id) {
            throw DomainError{"cabinet.hardware_key_mismatch",
                              "Cabinet.hardware key does not match hardware.id"};
        }
        hw.validate();
        // Дополнительная проверка: каждый attachment ссылается на
        // существующую в этом же шкафу панель.
        for (const auto& a : hw.attachments) {
            if (panels.find(a.panel_id) == panels.end()) {
                throw DomainError{"cabinet.hardware_unknown_panel",
                                  "HardwareItem references unknown PanelId"};
            }
        }
    }
}

}  // namespace coupecad::core
