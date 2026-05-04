#include "coupecad/geometry/cabinet_shape.h"

#include "coupecad/core/cabinet.h"

#include <BRep_Builder.hxx>

namespace coupecad::geometry {

TopoDS_Compound build_cabinet_compound(
    const core::Project& project,
    const std::function<const TopoDS_Solid&(const core::PanelId&)>& get_panel_solid,
    const std::function<const TopoDS_Compound&(const core::HardwareItemId&)>&
        get_hardware_compound) {

    TopoDS_Compound compound;
    BRep_Builder bb;
    bb.MakeCompound(compound);

    for (const auto& [panel_id, _] : project.cabinet().panels) {
        bb.Add(compound, get_panel_solid(panel_id));
    }
    for (const auto& [hw_id, _] : project.cabinet().hardware) {
        bb.Add(compound, get_hardware_compound(hw_id));
    }
    return compound;
}

}  // namespace coupecad::geometry
