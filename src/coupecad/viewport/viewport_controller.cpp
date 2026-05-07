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

void ViewportController::on_mouse_press(int, int, MouseButton)   {}
void ViewportController::on_mouse_move(int, int, MouseButton)    {}
void ViewportController::on_mouse_release(int, int, MouseButton) {}
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
