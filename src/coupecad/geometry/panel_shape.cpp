#include "coupecad/geometry/panel_shape.h"

#include "coupecad/core/errors.h"
#include "coupecad/core/geometry.h"
#include "coupecad/geometry/occt_helpers.h"

#include <BRepBuilderAPI_Transform.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <TopoDS.hxx>
#include <gp_Pnt.hxx>

namespace coupecad::geometry {

TopoDS_Solid build_panel_solid(const core::Cabinet& cabinet,
                               const core::Panel& panel) {
    const auto pg = core::compute_panel_geometry(cabinet, panel);
    const auto dims = to_box_dims(pg.size);

    if (dims.dx <= 0.0 || dims.dy <= 0.0 || dims.dz <= 0.0) {
        throw core::DomainError{"geometry.panel_box_nonpositive",
                                "Panel size has non-positive dimension"};
    }

    BRepPrimAPI_MakeBox box(gp_Pnt(0.0, 0.0, 0.0), dims.dx, dims.dy, dims.dz);
    const TopoDS_Solid local_solid = box.Solid();

    const gp_Trsf trsf = to_occt_transform(pg.origin, pg.orientation);
    return TopoDS::Solid(
        BRepBuilderAPI_Transform(local_solid, trsf, /*Copy=*/false).Shape());
}

}  // namespace coupecad::geometry
