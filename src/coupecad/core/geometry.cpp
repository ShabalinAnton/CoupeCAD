#include "coupecad/core/geometry.h"

#include "coupecad/core/errors.h"

#include <algorithm>
#include <string>

namespace coupecad::core {

namespace {

Millimeters effective_thickness(const Cabinet& c, const Panel& p) {
    if (p.thickness_override) return *p.thickness_override;
    if (p.role == PanelRole::Back) return c.default_back_thickness;
    return c.default_panel_thickness;
}

void require_positive_size(const Vec3& size, const char* code) {
    if (size.x.value() <= 0 || size.y.value() <= 0 || size.z.value() <= 0) {
        throw DomainError{code, "Computed panel size has non-positive dimension"};
    }
}

template <class Params>
const Params& role_params_or_throw(const Panel& panel) {
    if (const auto* params = std::get_if<Params>(&panel.role_params)) {
        return *params;
    }
    throw DomainError{
        "geometry.role_params_mismatch",
        "Panel role_params do not match role " + std::string(panel_role_name(panel.role))};
}

}  // namespace

PanelGeometry compute_panel_geometry(const Cabinet& c, const Panel& p) {
    const auto W = c.dimensions.width;
    const auto D = c.dimensions.depth;
    const auto H = c.dimensions.height;
    const auto t = effective_thickness(c, p);
    const auto t_back = c.default_back_thickness;

    PanelGeometry g{};
    switch (p.role) {
        case PanelRole::Top: {
            g.origin = Vec3{Millimeters{0}, Millimeters{0}, H - t};
            g.size = Vec3{W, D - t_back, t};
            break;
        }
        case PanelRole::Bottom: {
            g.origin = Vec3{Millimeters{0}, Millimeters{0}, Millimeters{0}};
            g.size = Vec3{W, D - t_back, t};
            break;
        }
        case PanelRole::SideLeft: {
            g.origin = Vec3{Millimeters{0}, Millimeters{0}, Millimeters{0}};
            g.size = Vec3{t, D - t_back, H};
            break;
        }
        case PanelRole::SideRight: {
            g.origin = Vec3{W - t, Millimeters{0}, Millimeters{0}};
            g.size = Vec3{t, D - t_back, H};
            break;
        }
        case PanelRole::Back: {
            g.origin = Vec3{Millimeters{0}, D - t, Millimeters{0}};
            g.size = Vec3{W, t, H};
            break;
        }
        case PanelRole::Shelf: {
            const auto& sp = role_params_or_throw<ShelfParams>(p);
            if (sp.height_from_bottom.value() < 0 ||
                sp.height_from_bottom + t > H) {
                throw DomainError{"geometry.shelf_overflow",
                                  "Shelf height_from_bottom places shelf outside cabinet"};
            }
            // Side-thickness (использует default_panel_thickness, без оверрайдов
            // на side-панелях — упрощение Stage 1a).
            const auto side_t = c.default_panel_thickness;
            Millimeters from_x{0};
            Millimeters to_x{0};
            if (std::holds_alternative<ShelfFullWidth>(sp.extent)) {
                from_x = side_t;
                to_x = W - side_t;
            } else {
                const auto& bd = std::get<ShelfBetweenDividers>(sp.extent);
                auto fit = c.panels.find(bd.from);
                auto tit = c.panels.find(bd.to);
                if (fit == c.panels.end() || tit == c.panels.end()) {
                    throw DomainError{"geometry.shelf_divider_missing",
                                      "ShelfBetweenDividers references unknown PanelId"};
                }
                if (fit->second.role != PanelRole::DividerVertical ||
                    tit->second.role != PanelRole::DividerVertical) {
                    throw DomainError{"geometry.shelf_divider_role",
                                      "ShelfBetweenDividers references non-vertical-divider"};
                }
                const auto& f_dp = role_params_or_throw<DividerVerticalParams>(fit->second);
                const auto& t_dp = role_params_or_throw<DividerVerticalParams>(tit->second);
                from_x = f_dp.offset_from_left + side_t;  // правый край левого
                to_x   = t_dp.offset_from_left;           // левый край правого
            }
            g.origin = Vec3{from_x, Millimeters{0}, sp.height_from_bottom};
            g.size = Vec3{to_x - from_x, D - t_back, t};
            break;
        }
        case PanelRole::DividerVertical: {
            const auto& dp = role_params_or_throw<DividerVerticalParams>(p);
            Millimeters from_z{0};
            Millimeters to_z = H;
            if (std::holds_alternative<VerticalExtentRange>(dp.height_extent)) {
                const auto& r = std::get<VerticalExtentRange>(dp.height_extent);
                from_z = r.from_z;
                to_z = r.to_z;
            }
            g.origin = Vec3{dp.offset_from_left, Millimeters{0}, from_z};
            g.size = Vec3{t, D - t_back, to_z - from_z};
            break;
        }
        case PanelRole::DividerHorizontal: {
            const auto& dp = role_params_or_throw<DividerHorizontalParams>(p);
            Millimeters from_y{0};
            Millimeters to_y = D - t_back;
            if (std::holds_alternative<DepthExtentRange>(dp.depth_extent)) {
                const auto& r = std::get<DepthExtentRange>(dp.depth_extent);
                from_y = r.from_y;
                to_y = r.to_y;
            }
            const auto side_t = c.default_panel_thickness;
            g.origin = Vec3{side_t, from_y, dp.offset_from_bottom};
            g.size = Vec3{W - Millimeters{2 * side_t.value()}, to_y - from_y, t};
            break;
        }
        case PanelRole::Facade: {
            const auto& fp = role_params_or_throw<FacadeParams>(p);
            Millimeters from_x{0};
            Millimeters to_x = W;
            Millimeters from_z{0};
            Millimeters to_z = H;
            if (std::holds_alternative<FacadeRect>(fp.extent)) {
                const auto& r = std::get<FacadeRect>(fp.extent);
                from_x = r.from_x;
                to_x = r.to_x;
                from_z = r.from_z;
                to_z = r.to_z;
            }
            // Фасад лежит ПЕРЕД корпусом (y < 0): в Stage 1a условно
            // помещаем его в y = -t..0 (gap=0).
            g.origin = Vec3{from_x, -t, from_z};
            g.size = Vec3{to_x - from_x, t, to_z - from_z};
            break;
        }
        case PanelRole::DrawerBottom: {
            const auto& dp = role_params_or_throw<DrawerBottomParams>(p);
            const auto side_t = c.default_panel_thickness;
            g.origin = Vec3{side_t, Millimeters{0}, dp.height_from_bottom};
            g.size = Vec3{W - Millimeters{2 * side_t.value()}, dp.depth, t};
            break;
        }
        case PanelRole::DrawerFront: {
            const auto& dp = role_params_or_throw<DrawerFrontParams>(p);
            g.origin = Vec3{Millimeters{0}, -t, dp.height_from_bottom};
            g.size = Vec3{W, t, dp.height};
            break;
        }
        case PanelRole::DrawerSide: {
            const auto& dp = role_params_or_throw<DrawerSideParams>(p);
            const auto side_t = c.default_panel_thickness;
            Millimeters x = (dp.side == DrawerSideParams::Side::Left)
                                 ? side_t
                                 : W - side_t - t;
            g.origin = Vec3{x, Millimeters{0}, dp.height_from_bottom};
            g.size = Vec3{t, dp.depth, dp.height};
            break;
        }
        case PanelRole::DrawerBack: {
            const auto& dp = role_params_or_throw<DrawerBackParams>(p);
            const auto side_t = c.default_panel_thickness;
            g.origin = Vec3{side_t, D - t_back - t, dp.height_from_bottom};
            g.size = Vec3{W - Millimeters{2 * side_t.value()}, t, dp.height};
            break;
        }
        case PanelRole::Plinth: {
            const auto& pp = role_params_or_throw<PlinthParams>(p);
            g.origin = Vec3{Millimeters{0}, pp.setback, Millimeters{0}};
            g.size = Vec3{W, t, pp.height};
            break;
        }
        case PanelRole::Custom: {
            const auto& cp = role_params_or_throw<CustomParams>(p);
            g.origin = cp.position;
            g.size = cp.size;
            g.orientation = cp.orientation;
            break;
        }
    }
    require_positive_size(g.size, "geometry.computed_negative_size");
    return g;
}

}  // namespace coupecad::core
