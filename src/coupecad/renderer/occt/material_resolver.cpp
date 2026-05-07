#include "coupecad/renderer/occt/material_resolver.h"

#include "coupecad/core/errors.h"

#include <Quantity_TypeOfColor.hxx>

namespace coupecad::renderer::occt {

namespace {

Quantity_Color rgba_to_color(const core::RGBA& rgba) {
    return Quantity_Color(static_cast<Standard_Real>(rgba.r) / 255.0,
                          static_cast<Standard_Real>(rgba.g) / 255.0,
                          static_cast<Standard_Real>(rgba.b) / 255.0,
                          Quantity_TOC_RGB);
}

}  // namespace

Quantity_Color resolve_panel_color(const core::Project& project,
                                   const core::Panel& panel) {
    const core::MaterialId effective_id =
        panel.material_override.value_or(project.cabinet().default_panel_material);

    const auto it = project.materials().find(effective_id);
    if (it == project.materials().end()) {
        throw core::DomainError{
            "renderer.material_not_found",
            "Panel " + panel.id.to_string() +
                " references missing material " + effective_id.to_string()};
    }
    return rgba_to_color(it->second.color_hint);
}

Quantity_Color resolve_hardware_color() {
    return Quantity_Color(0.55, 0.57, 0.60, Quantity_TOC_RGB);
}

}  // namespace coupecad::renderer::occt
