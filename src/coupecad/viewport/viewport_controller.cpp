#include "coupecad/viewport/viewport_controller.h"

#include "coupecad/geometry/geometry_builder.h"
#include "coupecad/logging/logger.h"

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

    // Camera deltas come in Task 9; for now just consume the event so
    // the drag-state machine is exercised by tests.
    (void)dx;
    (void)dy;
}

void ViewportController::on_mouse_release(int x, int y, MouseButton btn) {
    // Pick-on-click logic comes in Task 10.
    (void)x;
    (void)y;
    (void)btn;
    active_drag_ = MouseButton::None;
}

void ViewportController::on_wheel(int, int, double)              {}
void ViewportController::fit_all()                                {}

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
