#include "coupecad/viewport/viewport_controller.h"

#include "coupecad/geometry/geometry_builder.h"
#include "coupecad/logging/logger.h"

#include <cmath>
#include <cstdint>

namespace coupecad::viewport {

ViewportController::ViewportController(core::Project& project,
                                       geometry::GeometryBuilder& builder,
                                       renderer::IRenderer& renderer_in)
    : project_(project), builder_(builder), renderer_(renderer_in) {
    coupecad::logging::Logger::instance().info(
        "viewport", "ViewportController constructed");
}

ViewportController::~ViewportController() = default;

void ViewportController::on_changed(const core::Project&,
                                    const core::ChangeSet& cs) {
    if (cs.empty()) return;
    builder_.apply_changes(cs);
    renderer_.sync(cs);
    dirty_ = true;
}

void ViewportController::rebuild_from_scratch() {
    coupecad::logging::Logger::instance().info(
        "viewport", "rebuild_from_scratch");
    builder_.rebuild_all();
    renderer_.rebuild_all();
    renderer_.fit_all();
    dirty_ = true;
}

void ViewportController::on_mouse_press(int x, int y, MouseButton btn) {
    active_drag_ = btn;
    drag_last_x_ = x;
    drag_last_y_ = y;
    drag_total_dx_ = 0;
    drag_total_dy_ = 0;
}

void ViewportController::on_mouse_move(int x, int y, MouseButton btn) {
    if (btn != active_drag_ || active_drag_ == MouseButton::None) return;

    const int dx = x - drag_last_x_;
    const int dy = y - drag_last_y_;
    drag_total_dx_ += dx >= 0 ? dx : -dx;
    drag_total_dy_ += dy >= 0 ? dy : -dy;
    drag_last_x_ = x;
    drag_last_y_ = y;

    if (active_drag_ == MouseButton::Left) {
        // Orbit: 0.5° per pixel around target.
        constexpr double kDegPerPixel = 0.5;
        const double az_deg = -static_cast<double>(dx) * kDegPerPixel;
        const double el_deg = -static_cast<double>(dy) * kDegPerPixel;

        renderer::CameraState s = renderer_.camera();
        const double tx = static_cast<double>(s.target.x.value());
        const double ty = static_cast<double>(s.target.y.value());
        const double tz = static_cast<double>(s.target.z.value());
        const double ex = static_cast<double>(s.eye.x.value()) - tx;
        const double ey = static_cast<double>(s.eye.y.value()) - ty;
        const double ez = static_cast<double>(s.eye.z.value()) - tz;
        const double dist = std::sqrt(ex * ex + ey * ey + ez * ez);
        if (dist < 1e-6) return;

        // Current spherical coords (Z-up frame): elevation from XY-plane.
        const double current_az = std::atan2(ey, ex);
        const double current_el = std::asin(ez / dist);

        constexpr double kPi = 3.14159265358979323846;
        const double new_az = current_az + az_deg * kPi / 180.0;
        double new_el = current_el + el_deg * kPi / 180.0;
        constexpr double kMaxEl = 89.0 * kPi / 180.0;
        if (new_el > kMaxEl)  new_el = kMaxEl;
        if (new_el < -kMaxEl) new_el = -kMaxEl;

        const double cos_el = std::cos(new_el);
        const double new_ex = dist * cos_el * std::cos(new_az);
        const double new_ey = dist * cos_el * std::sin(new_az);
        const double new_ez = dist * std::sin(new_el);

        s.eye = core::Vec3{
            core::Millimeters{static_cast<std::int32_t>(tx + new_ex)},
            core::Millimeters{static_cast<std::int32_t>(ty + new_ey)},
            core::Millimeters{static_cast<std::int32_t>(tz + new_ez)}};
        renderer_.set_camera(s);
        dirty_ = true;
        return;
    }

    if (active_drag_ == MouseButton::Middle) {
        // Pan: translate eye AND target by world-space vector along right/up.
        renderer::CameraState s = renderer_.camera();
        const double ex = static_cast<double>(s.eye.x.value() - s.target.x.value());
        const double ey = static_cast<double>(s.eye.y.value() - s.target.y.value());
        const double ez = static_cast<double>(s.eye.z.value() - s.target.z.value());
        const double dist = std::sqrt(ex * ex + ey * ey + ez * ez);
        if (dist < 1e-6) return;

        // Forward = (target - eye) / dist; up_world = s.up; right = up × forward.
        const double fx = -ex / dist, fy = -ey / dist, fz = -ez / dist;
        const double ux_w = static_cast<double>(s.up.x.value());
        const double uy_w = static_cast<double>(s.up.y.value());
        const double uz_w = static_cast<double>(s.up.z.value());
        // right = up × forward.
        const double rx = uy_w * fz - uz_w * fy;
        const double ry = uz_w * fx - ux_w * fz;
        const double rz = ux_w * fy - uy_w * fx;
        // Re-normalize.
        const double rlen = std::sqrt(rx * rx + ry * ry + rz * rz);
        if (rlen < 1e-6) return;
        const double rxn = rx / rlen, ryn = ry / rlen, rzn = rz / rlen;
        // Real up = forward × right.
        const double upx = fy * rzn - fz * ryn;
        const double upy = fz * rxn - fx * rzn;
        const double upz = fx * ryn - fy * rxn;

        // Pan magnitude scales with view distance and viewport (rough heuristic
        // — keeps pan feel constant regardless of zoom).
        constexpr double kPanScale = 0.002;  // mm-per-pixel-per-mm-distance
        const double pan_x_mm = -static_cast<double>(dx) * dist * kPanScale;
        const double pan_y_mm = +static_cast<double>(dy) * dist * kPanScale;
        // Apply along right + up.
        const double world_dx = rxn * pan_x_mm + upx * pan_y_mm;
        const double world_dy = ryn * pan_x_mm + upy * pan_y_mm;
        const double world_dz = rzn * pan_x_mm + upz * pan_y_mm;

        s.eye = core::Vec3{
            core::Millimeters{s.eye.x.value() + static_cast<std::int32_t>(world_dx)},
            core::Millimeters{s.eye.y.value() + static_cast<std::int32_t>(world_dy)},
            core::Millimeters{s.eye.z.value() + static_cast<std::int32_t>(world_dz)}};
        s.target = core::Vec3{
            core::Millimeters{s.target.x.value() + static_cast<std::int32_t>(world_dx)},
            core::Millimeters{s.target.y.value() + static_cast<std::int32_t>(world_dy)},
            core::Millimeters{s.target.z.value() + static_cast<std::int32_t>(world_dz)}};
        renderer_.set_camera(s);
        dirty_ = true;
        return;
    }

    // Right-button drag accumulates delta but does nothing during the
    // motion — pick happens on release if drag was tiny.
}

void ViewportController::on_mouse_release(int x, int y, MouseButton btn) {
    // Pick-on-click logic comes in Task 10.
    (void)x;
    (void)y;
    (void)btn;
    active_drag_ = MouseButton::None;
}

void ViewportController::on_wheel(int /*x*/, int /*y*/, double delta_steps) {
    renderer::CameraState s = renderer_.camera();
    const double factor = std::pow(1.1, -delta_steps);

    const double tx = static_cast<double>(s.target.x.value());
    const double ty = static_cast<double>(s.target.y.value());
    const double tz = static_cast<double>(s.target.z.value());

    const double ex = static_cast<double>(s.eye.x.value()) - tx;
    const double ey = static_cast<double>(s.eye.y.value()) - ty;
    const double ez = static_cast<double>(s.eye.z.value()) - tz;

    s.eye = core::Vec3{
        core::Millimeters{static_cast<std::int32_t>(tx + ex * factor)},
        core::Millimeters{static_cast<std::int32_t>(ty + ey * factor)},
        core::Millimeters{static_cast<std::int32_t>(tz + ez * factor)}};

    renderer_.set_camera(s);
    dirty_ = true;
}

void ViewportController::fit_all() {
    renderer_.fit_all();
    dirty_ = true;
}

renderer::CameraState ViewportController::camera() const {
    return renderer_.camera();
}

std::vector<renderer::EntityId> ViewportController::selection() const {
    return renderer_.selection();
}

bool ViewportController::selection_empty() const {
    return renderer_.selection().empty();
}

std::size_t ViewportController::selection_count() const {
    return renderer_.selection().size();
}

void ViewportController::set_viewport_size(int width, int height) {
    renderer_.set_viewport_size(coupecad::renderer::ViewportSize{width, height});
    dirty_ = true;
}

}  // namespace coupecad::viewport
