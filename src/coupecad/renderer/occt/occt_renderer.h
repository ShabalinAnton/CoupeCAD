#pragma once

#include "coupecad/renderer/i_renderer.h"
#include "coupecad/renderer/occt/ais_scene.h"
#include "coupecad/renderer/occt/view_driver.h"

#include <memory>

namespace coupecad::core { class Project; }
namespace coupecad::geometry { class GeometryBuilder; }

namespace coupecad::renderer::occt {

class OcctRenderer : public IRenderer {
public:
    OcctRenderer(const core::Project& project,
                 geometry::GeometryBuilder& builder);
    ~OcctRenderer() override;

    OcctRenderer(const OcctRenderer&) = delete;
    OcctRenderer& operator=(const OcctRenderer&) = delete;

    // IRenderer.
    void sync(const core::ChangeSet& cs) override;
    void rebuild_all() override;

    void           set_camera(const CameraState& state) override;
    CameraState    camera() const override;
    void           fit_all() override;

    std::optional<EntityId> pick(int x, int y) override;

    void                          select(const EntityId& id) override;
    void                          deselect(const EntityId& id) override;
    void                          clear_selection() override;
    std::vector<EntityId>         selection() const override;

    void           set_viewport_size(ViewportSize size) override;
    ViewportSize   viewport_size() const override;

    std::vector<std::uint8_t> render_to_image() override;

    // Test inspectors (not part of IRenderer).
    const AisScene&   scene() const noexcept { return scene_; }
    const ViewDriver& driver() const noexcept { return driver_; }

private:
    const core::Project&        project_;
    geometry::GeometryBuilder&  builder_;
    ViewDriver                  driver_;
    AisScene                    scene_;
};

// Factory.
std::unique_ptr<IRenderer> make_occt_renderer(const core::Project& project,
                                              geometry::GeometryBuilder& builder);

}  // namespace coupecad::renderer::occt
