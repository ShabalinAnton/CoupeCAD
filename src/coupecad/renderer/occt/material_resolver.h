#pragma once

#include "coupecad/core/panel.h"
#include "coupecad/core/project.h"

#include <Quantity_Color.hxx>

namespace coupecad::renderer::occt {

// Возвращает Quantity_Color для панели по её эффективному материалу:
// material_override → cabinet.default_panel_material.
// Бросает core::DomainError{"renderer.material_not_found"}, если
// material id не найден в project.materials().
Quantity_Color resolve_panel_color(const core::Project& project,
                                   const core::Panel& panel);

// Фиксированный металлический серый (см. spec §4.4).
Quantity_Color resolve_hardware_color();

}  // namespace coupecad::renderer::occt
