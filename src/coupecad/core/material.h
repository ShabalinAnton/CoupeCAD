#pragma once

#include "coupecad/core/id.h"
#include "coupecad/core/units.h"

#include <optional>
#include <string>

namespace coupecad::core {

enum class MaterialKind {
    ChipboardLaminated,  // ЛДСП
    Mdf,
    Hdf,
    Plywood,             // фанера
    SolidWood,           // массив дерева
    Glass,
    Metal,
    Other,
};

const char* material_kind_name(MaterialKind kind) noexcept;

// Plain-data материал. Инварианты валидируются Material::validate().
struct Material {
    MaterialId id;
    std::string name;
    MaterialKind kind = MaterialKind::ChipboardLaminated;
    Millimeters default_thickness{16};
    RGBA color_hint{};
    std::optional<std::string> texture_ref;
    std::optional<Money> price_per_sqm;

    bool operator==(const Material&) const = default;

    // Кидает DomainError при нарушении (см. spec §3.4):
    // - default_thickness <= 0
    // - name пустое
    // - id не valid
    void validate() const;
};

}  // namespace coupecad::core
