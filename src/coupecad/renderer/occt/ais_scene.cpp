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

void AisScene::add_hardware(const core::HardwareItemId& id,
                            const TopoDS_Compound& compound) {
    Handle(AIS_Shape) ais = new AIS_Shape(compound);
    ais->SetColor(resolve_hardware_color());

    // Inserts first — rollback on failure (same pattern as add_panel).
    auto [hw_it, hw_inserted] = hardware_objects_.emplace(id, ais);
    try {
        ais_to_entity_.emplace(ais.get(), EntityId{id});
    } catch (...) {
        hardware_objects_.erase(hw_it);
        throw;
    }
    try {
        context_->Display(ais, /*updateViewer=*/Standard_False);
    } catch (...) {
        ais_to_entity_.erase(ais.get());
        hardware_objects_.erase(hw_it);
        throw;
    }

    coupecad::logging::Logger::instance().trace(
        "renderer", "ais_scene.add_hardware id={}", id.to_string());
}

void AisScene::replace_hardware(const core::HardwareItemId& id,
                                const TopoDS_Compound& compound) {
    erase_hardware_internal(id);
    add_hardware(id, compound);
}

void AisScene::remove_hardware(const core::HardwareItemId& id) {
    erase_hardware_internal(id);
    coupecad::logging::Logger::instance().trace(
        "renderer", "ais_scene.remove_hardware id={}", id.to_string());
}

bool AisScene::has_hardware(const core::HardwareItemId& id) const noexcept {
    return hardware_objects_.find(id) != hardware_objects_.end();
}

std::vector<core::HardwareItemId> AisScene::hardware_ids() const {
    std::vector<core::HardwareItemId> result;
    result.reserve(hardware_objects_.size());
    for (const auto& [id, ais] : hardware_objects_) result.push_back(id);
    return result;
}

void AisScene::refresh_colors_for_material(const core::MaterialId& mat_id) {
    const auto& cabinet = project_.cabinet();
    for (const auto& [panel_id, ais] : panel_objects_) {
        const auto& panel = cabinet.panels.at(panel_id);
        const core::MaterialId effective =
            panel.material_override.value_or(cabinet.default_panel_material);
        if (effective != mat_id) continue;
        ais->SetColor(resolve_panel_color(project_, panel));
        context_->Redisplay(ais, Standard_False, Standard_False);
    }
}

void AisScene::clear() {
    for (auto& [id, ais] : panel_objects_) {
        context_->Remove(ais, Standard_False);
    }
    for (auto& [id, ais] : hardware_objects_) {
        context_->Remove(ais, Standard_False);
    }
    panel_objects_.clear();
    hardware_objects_.clear();
    ais_to_entity_.clear();
}

void AisScene::erase_hardware_internal(const core::HardwareItemId& id) {
    auto it = hardware_objects_.find(id);
    if (it == hardware_objects_.end()) return;
    ais_to_entity_.erase(it->second.get());
    context_->Remove(it->second, Standard_False);
    hardware_objects_.erase(it);
}

Handle(AIS_Shape) AisScene::raw_ais_handle_for_panel(
    const core::PanelId& id) const {
    auto it = panel_objects_.find(id);
    return it == panel_objects_.end() ? Handle(AIS_Shape){} : it->second;
}

const AIS_InteractiveObject* AisScene::raw_ais_pointer_for_hardware(
    const core::HardwareItemId& id) const {
    auto it = hardware_objects_.find(id);
    return it == hardware_objects_.end() ? nullptr : it->second.get();
}

}  // namespace coupecad::renderer::occt
