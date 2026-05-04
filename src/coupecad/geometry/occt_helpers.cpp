#include "coupecad/geometry/occt_helpers.h"

#include <gp_Quaternion.hxx>
#include <gp_Vec.hxx>

namespace coupecad::geometry {

gp_Pnt to_occt_point(const core::Vec3& v) noexcept {
    return gp_Pnt(static_cast<double>(v.x.value()),
                  static_cast<double>(v.y.value()),
                  static_cast<double>(v.z.value()));
}

BoxDims to_box_dims(const core::Vec3& size) noexcept {
    return BoxDims{static_cast<double>(size.x.value()),
                   static_cast<double>(size.y.value()),
                   static_cast<double>(size.z.value())};
}

gp_Trsf to_occt_transform(const core::Vec3& origin,
                          const core::Quat& orientation) noexcept {
    gp_Trsf trsf;
    // Поворот первым в локальной СК, потом перенос в мировую.
    const gp_Quaternion q(orientation.x, orientation.y, orientation.z, orientation.w);
    trsf.SetRotation(q);
    trsf.SetTranslationPart(gp_Vec(static_cast<double>(origin.x.value()),
                                   static_cast<double>(origin.y.value()),
                                   static_cast<double>(origin.z.value())));
    return trsf;
}

}  // namespace coupecad::geometry
