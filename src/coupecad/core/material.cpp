#include "coupecad/core/material.h"

#include "coupecad/core/errors.h"

namespace coupecad::core {

const char* material_kind_name(MaterialKind kind) noexcept {
    switch (kind) {
        case MaterialKind::ChipboardLaminated: return "ChipboardLaminated";
        case MaterialKind::Mdf:                return "Mdf";
        case MaterialKind::Hdf:                return "Hdf";
        case MaterialKind::Plywood:            return "Plywood";
        case MaterialKind::SolidWood:          return "SolidWood";
        case MaterialKind::Glass:              return "Glass";
        case MaterialKind::Metal:              return "Metal";
        case MaterialKind::Other:              return "Other";
    }
    return "?";
}

void Material::validate() const {
    if (!id.is_valid()) {
        throw DomainError{"material.invalid_id", "Material has invalid id"};
    }
    if (name.empty()) {
        throw DomainError{"material.empty_name", "Material name is empty"};
    }
    if (default_thickness.value() <= 0) {
        throw DomainError{"material.nonpositive_thickness",
                          "Material default_thickness must be > 0"};
    }
}

}  // namespace coupecad::core
