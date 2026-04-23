#pragma once

#include "coupecad/core/id.h"
#include "coupecad/core/project.h"

#include <TopoDS_Compound.hxx>
#include <TopoDS_Solid.hxx>

#include <functional>

namespace coupecad::geometry {

// Собрать TopoDS_Compound шкафа: для каждой панели получаем TopoDS_Solid
// через get_panel_solid, для каждого hardware-item — TopoDS_Compound через
// get_hardware_compound. Колбэки нужны, чтобы assembler тестировался без
// зависимости от GeometryBuilder.
//
// Колбэки могут бросать DomainError — пробрасывается дальше.
TopoDS_Compound build_cabinet_compound(
    const core::Project& project,
    const std::function<const TopoDS_Solid&(const core::PanelId&)>& get_panel_solid,
    const std::function<const TopoDS_Compound&(const core::HardwareItemId&)>&
        get_hardware_compound);

}  // namespace coupecad::geometry
