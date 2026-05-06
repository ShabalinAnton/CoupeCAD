#include "coupecad/renderer/occt/occt_renderer.h"

#include "coupecad/core/errors.h"
#include "coupecad/geometry/geometry_builder.h"
#include "coupecad/logging/logger.h"

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

    // Удаления.
    for (const auto& id : cs.removed_panels)   scene_.remove_panel(id);
    for (const auto& id : cs.removed_hardware) scene_.remove_hardware(id);

    // Updates = replace. Держим старые Handle'ы живыми на время
    // вызовов replace_*: иначе OCCT-аллокатор (MMgrOpt) может вернуть
    // только что освобождённый слот следующему AIS_Shape, и raw-
    // указатели не сменятся — это ломает инвалидацию кэшей
    // наблюдателей сцены.
    std::vector<Handle(AIS_Shape)> keepalive;
    keepalive.reserve(cs.updated_panels.size());
    for (const auto& id : cs.updated_panels)
        keepalive.push_back(scene_.raw_ais_handle_for_panel(id));
    for (const auto& id : cs.updated_panels)
        scene_.replace_panel(id, builder_.panel_solid(id));
    for (const auto& id : cs.updated_hardware)
        scene_.replace_hardware(id, builder_.hardware_compound(id));

    // Adds.
    for (const auto& id : cs.added_panels)
        scene_.add_panel(id, builder_.panel_solid(id));
    for (const auto& id : cs.added_hardware)
        scene_.add_hardware(id, builder_.hardware_compound(id));

    // cabinet_changed → replace всё, что сейчас в сцене.
    if (cs.cabinet_changed) {
        const auto pids = scene_.panel_ids();
        const auto hids = scene_.hardware_ids();

        keepalive.reserve(keepalive.size() + pids.size());
        for (const auto& id : pids)
            keepalive.push_back(scene_.raw_ais_handle_for_panel(id));

        for (const auto& id : pids)
            scene_.replace_panel(id, builder_.panel_solid(id));
        for (const auto& id : hids)
            scene_.replace_hardware(id, builder_.hardware_compound(id));
    }
    // keepalive выходит из scope — старая память освобождается уже
    // после allocации новых AIS_Shape.

    // Material-only дельты.
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
