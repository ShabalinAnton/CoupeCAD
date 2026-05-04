#pragma once

#include "coupecad/core/units.h"

#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

namespace coupecad::geometry {

// Размеры бокса в double-миллиметрах для BRepPrimAPI_MakeBox.
struct BoxDims {
    double dx = 0.0;
    double dy = 0.0;
    double dz = 0.0;
};

// Перевод доменного Vec3 (mm-int) в OCCT gp_Pnt (mm-double).
// Internal unit совпадает: оба — миллиметры, без масштабирования.
gp_Pnt to_occt_point(const core::Vec3& v) noexcept;

// Перевод size-вектора в три аргумента BRepPrimAPI_MakeBox.
BoxDims to_box_dims(const core::Vec3& size) noexcept;

// Композиция translation(origin) ∘ rotation(orientation).
// orientation — w-first quaternion из core::Quat. Identity orientation
// даёт чистую трансляцию.
gp_Trsf to_occt_transform(const core::Vec3& origin,
                          const core::Quat& orientation) noexcept;

}  // namespace coupecad::geometry
