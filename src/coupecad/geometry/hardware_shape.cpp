#include "coupecad/geometry/hardware_shape.h"

#include "coupecad/core/cabinet.h"
#include "coupecad/core/errors.h"
#include "coupecad/core/geometry.h"
#include "coupecad/geometry/occt_helpers.h"

#include <BRepBuilderAPI_Transform.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRep_Builder.hxx>
#include <gp_Pnt.hxx>

namespace coupecad::geometry {

TopoDS_Compound build_hardware_compound(const core::Project& project,
                                        const core::HardwareItem& item) {
    const auto& catalog = project.hardware_catalog();
    const auto spec_it = catalog.find(item.ref);
    if (spec_it == catalog.end()) {
        throw core::DomainError{"HARDWARE_SPEC_NOT_FOUND",
                                "Hardware spec not found: " + item.ref.value()};
    }
    const core::Vec3& bbox_size = spec_it->second.bbox;
    const auto box = to_box_dims(bbox_size);

    TopoDS_Compound compound;
    BRep_Builder bb;
    bb.MakeCompound(compound);

    const auto& panels = project.cabinet().panels;
    for (const auto& att : item.attachments) {
        const auto panel_it = panels.find(att.panel_id);
        if (panel_it == panels.end()) {
            throw core::DomainError{
                "HARDWARE_ATTACHMENT_PANEL_NOT_FOUND",
                "Attachment refers to unknown panel: " + att.panel_id.to_string()};
        }
        const auto pg = core::compute_panel_geometry(project.cabinet(),
                                                     panel_it->second);

        const gp_Trsf panel_trsf = to_occt_transform(pg.origin, pg.orientation);
        const gp_Trsf attach_trsf = to_occt_transform(att.local_position,
                                                     att.orientation);
        const gp_Trsf world_trsf = panel_trsf.Multiplied(attach_trsf);

        BRepPrimAPI_MakeBox mk(gp_Pnt(0.0, 0.0, 0.0), box.dx, box.dy, box.dz);
        const TopoDS_Shape positioned =
            BRepBuilderAPI_Transform(mk.Solid(), world_trsf, /*Copy=*/false).Shape();
        bb.Add(compound, positioned);
    }
    return compound;
}

}  // namespace coupecad::geometry
