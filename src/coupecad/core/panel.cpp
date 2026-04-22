#include "coupecad/core/panel.h"

#include "coupecad/core/errors.h"

namespace coupecad::core {

const char* panel_role_name(PanelRole role) noexcept {
    switch (role) {
        case PanelRole::Top:               return "Top";
        case PanelRole::Bottom:            return "Bottom";
        case PanelRole::SideLeft:          return "SideLeft";
        case PanelRole::SideRight:         return "SideRight";
        case PanelRole::Back:              return "Back";
        case PanelRole::Shelf:             return "Shelf";
        case PanelRole::DividerVertical:   return "DividerVertical";
        case PanelRole::DividerHorizontal: return "DividerHorizontal";
        case PanelRole::Facade:            return "Facade";
        case PanelRole::DrawerBottom:      return "DrawerBottom";
        case PanelRole::DrawerFront:       return "DrawerFront";
        case PanelRole::DrawerSide:        return "DrawerSide";
        case PanelRole::DrawerBack:        return "DrawerBack";
        case PanelRole::Plinth:            return "Plinth";
        case PanelRole::Custom:            return "Custom";
    }
    return "?";
}

const char* hinge_side_name(HingeSide side) noexcept {
    switch (side) {
        case HingeSide::Left:   return "Left";
        case HingeSide::Right:  return "Right";
        case HingeSide::Top:    return "Top";
        case HingeSide::Bottom: return "Bottom";
        case HingeSide::None:   return "None";
    }
    return "?";
}

const char* grain_direction_name(GrainDirection g) noexcept {
    switch (g) {
        case GrainDirection::Horizontal: return "Horizontal";
        case GrainDirection::Vertical:   return "Vertical";
        case GrainDirection::None:       return "None";
    }
    return "?";
}

bool is_role_params_valid_for(PanelRole role, const RoleParams& params) noexcept {
    switch (role) {
        case PanelRole::Top:
        case PanelRole::Bottom:
        case PanelRole::Back:
        case PanelRole::SideLeft:
        case PanelRole::SideRight:
            return std::holds_alternative<NoRoleParams>(params);
        case PanelRole::Shelf:
            return std::holds_alternative<ShelfParams>(params);
        case PanelRole::DividerVertical:
            return std::holds_alternative<DividerVerticalParams>(params);
        case PanelRole::DividerHorizontal:
            return std::holds_alternative<DividerHorizontalParams>(params);
        case PanelRole::Facade:
            return std::holds_alternative<FacadeParams>(params);
        case PanelRole::DrawerBottom:
            return std::holds_alternative<DrawerBottomParams>(params);
        case PanelRole::DrawerFront:
            return std::holds_alternative<DrawerFrontParams>(params);
        case PanelRole::DrawerSide:
            return std::holds_alternative<DrawerSideParams>(params);
        case PanelRole::DrawerBack:
            return std::holds_alternative<DrawerBackParams>(params);
        case PanelRole::Plinth:
            return std::holds_alternative<PlinthParams>(params);
        case PanelRole::Custom:
            return std::holds_alternative<CustomParams>(params);
    }
    return false;
}

void Panel::validate() const {
    if (!id.is_valid()) {
        throw DomainError{"panel.invalid_id", "Panel has invalid id"};
    }
    if (!is_role_params_valid_for(role, role_params)) {
        throw DomainError{"panel.role_params_mismatch",
                          "Panel role and role_params variant don't match"};
    }
    if (thickness_override && thickness_override->value() <= 0) {
        throw DomainError{"panel.nonpositive_thickness",
                          "Panel thickness_override must be > 0"};
    }
    auto check_edge = [](const std::optional<EdgeBanding>& eb,
                         const char* code) {
        if (!eb) return;
        if (!eb->material_id.is_valid()) {
            throw DomainError{code, "EdgeBanding.material_id is invalid"};
        }
        if (eb->thickness.value() <= 0) {
            throw DomainError{code, "EdgeBanding.thickness must be > 0"};
        }
    };
    check_edge(edge_banding.front, "panel.edge_front_invalid");
    check_edge(edge_banding.back,  "panel.edge_back_invalid");
    check_edge(edge_banding.left,  "panel.edge_left_invalid");
    check_edge(edge_banding.right, "panel.edge_right_invalid");
}

}  // namespace coupecad::core
