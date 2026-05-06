#include "coupecad/renderer/occt/ais_scene.h"

#include "coupecad/logging/logger.h"
#include "coupecad/renderer/occt/material_resolver.h"
#include "coupecad/renderer/occt/view_driver.h"

namespace coupecad::renderer::occt {

AisScene::AisScene(ViewDriver& driver, const core::Project& project)
    : driver_(driver), project_(project) {
    context_ = new AIS_InteractiveContext(driver_.viewer());
    context_->SetAutomaticHilight(Standard_False);
}

AisScene::~AisScene() = default;

void AisScene::add_panel(const core::PanelId& id, const TopoDS_Solid& solid) {
    const auto& panel = project_.cabinet().panels.at(id);
    Handle(AIS_Shape) ais = new AIS_Shape(solid);
    ais->SetColor(resolve_panel_color(project_, panel));

    // Inserts first — rollback on failure keeps AIS context untouched.
    auto [panel_it, panel_inserted] = panel_objects_.emplace(id, ais);
    try {
        ais_to_entity_.emplace(ais.get(), EntityId{id});
    } catch (...) {
        panel_objects_.erase(panel_it);
        throw;
    }
    try {
        context_->Display(ais, /*updateViewer=*/Standard_False);
    } catch (...) {
        ais_to_entity_.erase(ais.get());
        panel_objects_.erase(panel_it);
        throw;
    }

    coupecad::logging::Logger::instance().trace(
        "renderer", "ais_scene.add_panel id={}", id.to_string());
}

void AisScene::replace_panel(const core::PanelId& id, const TopoDS_Solid& solid) {
    erase_panel_internal(id);
    add_panel(id, solid);
}

void AisScene::remove_panel(const core::PanelId& id) {
    erase_panel_internal(id);
    coupecad::logging::Logger::instance().trace(
        "renderer", "ais_scene.remove_panel id={}", id.to_string());
}

bool AisScene::has_panel(const core::PanelId& id) const noexcept {
    return panel_objects_.find(id) != panel_objects_.end();
}

std::vector<core::PanelId> AisScene::panel_ids() const {
    std::vector<core::PanelId> result;
    result.reserve(panel_objects_.size());
    for (const auto& [id, ais] : panel_objects_) result.push_back(id);
    return result;
}

const AIS_InteractiveObject* AisScene::raw_ais_pointer_for_panel(
    const core::PanelId& id) const {
    auto it = panel_objects_.find(id);
    return it == panel_objects_.end() ? nullptr : it->second.get();
}

void AisScene::erase_panel_internal(const core::PanelId& id) {
    auto it = panel_objects_.find(id);
    if (it == panel_objects_.end()) return;

    ais_to_entity_.erase(it->second.get());
    context_->Remove(it->second, /*updateViewer=*/Standard_False);
    panel_objects_.erase(it);
}

}  // namespace coupecad::renderer::occt
