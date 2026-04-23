#pragma once

#include "coupecad/core/hardware.h"
#include "coupecad/core/project.h"

#include <TopoDS_Compound.hxx>

namespace coupecad::geometry {

// Построить TopoDS_Compound для одного HardwareItem.
// Compound содержит по одному TopoDS_Solid (bbox-параллелепипед) на каждый
// PanelAttachment в item.attachments. Позиция каждого solid'а — в мировой
// СК шкафа: world_trsf = panel_trsf ∘ attachment_trsf.
//
// Бросает core::DomainError, если HardwareSpec для item.ref не найден
// в project.hardware_catalog(), или если какой-то attachment.panel_id
// отсутствует в project.cabinet().panels.
TopoDS_Compound build_hardware_compound(const core::Project& project,
                                        const core::HardwareItem& item);

}  // namespace coupecad::geometry
