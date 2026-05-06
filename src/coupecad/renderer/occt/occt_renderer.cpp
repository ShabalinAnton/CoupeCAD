#include "coupecad/renderer/occt/occt_renderer.h"

#include "coupecad/core/errors.h"
#include "coupecad/geometry/geometry_builder.h"
#include "coupecad/logging/logger.h"

#include <Graphic3d_Camera.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

#include <string>
#include <vector>

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

void OcctRenderer::sync(const core::ChangeSet& cs) {
    if (cs.empty()) return;

    coupecad::logging::Logger::instance().debug(
        "renderer",
        "sync: panels(+/-/u)={}/{}/{} hardware(+/-/u)={}/{}/{} cabinet={} materials(+/-/u)={}/{}/{}",
        cs.added_panels.size(), cs.removed_panels.size(), cs.updated_panels.size(),
        cs.added_hardware.size(), cs.removed_hardware.size(), cs.updated_hardware.size(),
        cs.cabinet_changed ? 1 : 0,
        cs.added_materials.size(), cs.removed_materials.size(), cs.updated_materials.size());

    for (const auto& id : cs.removed_panels)   scene_.remove_panel(id);
    for (const auto& id : cs.removed_hardware) scene_.remove_hardware(id);

    for (const auto& id : cs.updated_panels)
        scene_.replace_panel(id, builder_.panel_solid(id));
    for (const auto& id : cs.updated_hardware)
        scene_.replace_hardware(id, builder_.hardware_compound(id));

    for (const auto& id : cs.added_panels)
        scene_.add_panel(id, builder_.panel_solid(id));
    for (const auto& id : cs.added_hardware)
        scene_.add_hardware(id, builder_.hardware_compound(id));

    if (cs.cabinet_changed) {
        for (const auto& id : scene_.panel_ids())
            scene_.replace_panel(id, builder_.panel_solid(id));
        for (const auto& id : scene_.hardware_ids())
            scene_.replace_hardware(id, builder_.hardware_compound(id));
    }

    for (const auto& mat_id : cs.added_materials)   scene_.refresh_colors_for_material(mat_id);
    for (const auto& mat_id : cs.removed_materials) scene_.refresh_colors_for_material(mat_id);
    for (const auto& mat_id : cs.updated_materials) scene_.refresh_colors_for_material(mat_id);
}

void OcctRenderer::rebuild_all() {
    coupecad::logging::Logger::instance().info("renderer", "rebuild_all");
    scene_.clear();
    for (const auto& [pid, _] : project_.cabinet().panels) {
        scene_.add_panel(pid, builder_.panel_solid(pid));
    }
    for (const auto& [hid, _] : project_.cabinet().hardware) {
        scene_.add_hardware(hid, builder_.hardware_compound(hid));
    }
}

void OcctRenderer::set_camera(const CameraState& s) {
    auto& view = driver_.view();
    auto camera = view->Camera();
    camera->SetEye(gp_Pnt(static_cast<Standard_Real>(s.eye.x.value()),
                          static_cast<Standard_Real>(s.eye.y.value()),
                          static_cast<Standard_Real>(s.eye.z.value())));
    camera->SetCenter(gp_Pnt(static_cast<Standard_Real>(s.target.x.value()),
                             static_cast<Standard_Real>(s.target.y.value()),
                             static_cast<Standard_Real>(s.target.z.value())));
    camera->SetUp(gp_Dir(static_cast<Standard_Real>(s.up.x.value()),
                         static_cast<Standard_Real>(s.up.y.value()),
                         static_cast<Standard_Real>(s.up.z.value())));
    if (s.fov_deg <= 0.0) {
        camera->SetProjectionType(Graphic3d_Camera::Projection_Orthographic);
    } else {
        camera->SetProjectionType(Graphic3d_Camera::Projection_Perspective);
        camera->SetFOVy(s.fov_deg);
    }
    view->Update();
}

CameraState OcctRenderer::camera() const {
    const auto& view = driver_.view();
    const auto camera = view->Camera();
    const gp_Pnt eye    = camera->Eye();
    const gp_Pnt center = camera->Center();
    const gp_Dir up     = camera->Up();

    auto to_vec = [](const gp_Pnt& p) {
        return core::Vec3{
            core::Millimeters{static_cast<std::int32_t>(p.X())},
            core::Millimeters{static_cast<std::int32_t>(p.Y())},
            core::Millimeters{static_cast<std::int32_t>(p.Z())}};
    };
    auto to_vec_dir = [](const gp_Dir& d) {
        return core::Vec3{
            core::Millimeters{static_cast<std::int32_t>(d.X())},
            core::Millimeters{static_cast<std::int32_t>(d.Y())},
            core::Millimeters{static_cast<std::int32_t>(d.Z())}};
    };

    CameraState s;
    s.eye    = to_vec(eye);
    s.target = to_vec(center);
    s.up     = to_vec_dir(up);
    s.fov_deg =
        camera->ProjectionType() == Graphic3d_Camera::Projection_Orthographic
            ? 0.0
            : camera->FOVy();
    return s;
}

void OcctRenderer::fit_all() {
    if (scene_.panel_count() == 0 && scene_.hardware_count() == 0) {
        // Empty scene — nothing to frame; don't crash either.
        return;
    }
    driver_.view()->FitAll();
    driver_.view()->Update();
}

std::optional<EntityId> OcctRenderer::pick(int /*x*/, int /*y*/) {
    not_implemented_yet("pick");
}

void OcctRenderer::select(const EntityId& id)   { scene_.select(id); }
void OcctRenderer::deselect(const EntityId& id) { scene_.deselect(id); }
void OcctRenderer::clear_selection()            { scene_.clear_selection(); }
std::vector<EntityId> OcctRenderer::selection() const { return scene_.selection(); }

void          OcctRenderer::set_viewport_size(ViewportSize) { not_implemented_yet("set_viewport_size"); }
ViewportSize  OcctRenderer::viewport_size() const           { not_implemented_yet("viewport_size"); }

std::vector<std::uint8_t> OcctRenderer::render_to_image() { not_implemented_yet("render_to_image"); }

std::unique_ptr<IRenderer> make_occt_renderer(const core::Project& project,
                                              geometry::GeometryBuilder& builder) {
    return std::make_unique<OcctRenderer>(project, builder);
}

}  // namespace coupecad::renderer::occt
