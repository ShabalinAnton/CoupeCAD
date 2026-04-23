#pragma once

#include "coupecad/core/cabinet.h"
#include "coupecad/core/panel.h"

#include <TopoDS_Solid.hxx>

namespace coupecad::geometry {

// Построить TopoDS_Solid одной панели в мировой СК шкафа.
// Использует core::compute_panel_geometry для получения origin/size/orientation
// (он же делает валидацию role-based параметров).
//
// Бросает core::DomainError, если compute_panel_geometry падает или
// рассчитанный размер имеет нулевое измерение (BRepPrimAPI_MakeBox требует
// положительные размеры).
TopoDS_Solid build_panel_solid(const core::Cabinet& cabinet,
                               const core::Panel& panel);

}  // namespace coupecad::geometry
