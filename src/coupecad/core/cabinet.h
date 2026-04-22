#pragma once

#include "coupecad/core/hardware.h"
#include "coupecad/core/id.h"
#include "coupecad/core/panel.h"
#include "coupecad/core/units.h"

#include <string>
#include <unordered_map>

namespace coupecad::core {

struct Cabinet {
    CabinetId id;
    std::string name = "Cabinet";
    Dimensions dimensions{};
    MaterialId default_panel_material;        // обязательно (см. validate)
    Millimeters default_panel_thickness{16};
    Millimeters default_back_thickness{4};
    std::unordered_map<PanelId, Panel> panels;
    std::unordered_map<HardwareItemId, HardwareItem> hardware;

    bool operator==(const Cabinet&) const = default;

    // Бросает DomainError если:
    //  - id не valid
    //  - dimensions имеет неположительное измерение
    //  - default_panel_material не valid
    //  - default_panel_thickness <= 0 или default_back_thickness <= 0
    //  - любая Panel или HardwareItem не проходят свой validate()
    //  - ключи map'ов не совпадают с id значений (внутренняя инвариантность)
    void validate() const;
};

}  // namespace coupecad::core
