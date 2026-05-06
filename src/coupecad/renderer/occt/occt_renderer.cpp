#include "coupecad/renderer/occt/occt_renderer.h"

#include "coupecad/core/errors.h"
#include "coupecad/geometry/geometry_builder.h"
#include "coupecad/logging/logger.h"

#include <string>

namespace coupecad::renderer::occt {

namespace {

[[noreturn]] void not_implemented_yet(const char* where) {
    throw core::DomainError{"renderer.not_implemented_yet",
                            std::string{"OcctRenderer::"} + where +
                                " не реализовано (заполняется в плане Stage 3)"};
}

}  // namespace

OcctRenderer::OcctRenderer(const core::Project& project,
                           geometry::GeometryBuilder& builder)
    : project_(project), builder_(builder), scene_(driver_, project_) {
    coupecad::logging::Logger::instance().info(
        "renderer", "OcctRenderer constructed (gl_available={})",
        driver_.gl_available());
}

OcctRenderer::~OcctRenderer() = default;

void OcctRenderer::sync(const core::ChangeSet& /*cs*/)  { not_implemented_yet("sync"); }
void OcctRenderer::rebuild_all()                        { not_implemented_yet("rebuild_all"); }

void           OcctRenderer::set_camera(const CameraState&) { not_implemented_yet("set_camera"); }
CameraState    OcctRenderer::camera() const                 { not_implemented_yet("camera"); }
void           OcctRenderer::fit_all()                      { not_implemented_yet("fit_all"); }

std::optional<EntityId> OcctRenderer::pick(int /*x*/, int /*y*/) {
    not_implemented_yet("pick");
}

void                  OcctRenderer::select(const EntityId&)   { not_implemented_yet("select"); }
void                  OcctRenderer::deselect(const EntityId&) { not_implemented_yet("deselect"); }
void                  OcctRenderer::clear_selection()         { not_implemented_yet("clear_selection"); }
std::vector<EntityId> OcctRenderer::selection() const         { not_implemented_yet("selection"); }

void          OcctRenderer::set_viewport_size(ViewportSize) { not_implemented_yet("set_viewport_size"); }
ViewportSize  OcctRenderer::viewport_size() const           { not_implemented_yet("viewport_size"); }

std::vector<std::uint8_t> OcctRenderer::render_to_image() { not_implemented_yet("render_to_image"); }

std::unique_ptr<IRenderer> make_occt_renderer(const core::Project& project,
                                              geometry::GeometryBuilder& builder) {
    return std::make_unique<OcctRenderer>(project, builder);
}

}  // namespace coupecad::renderer::occt
