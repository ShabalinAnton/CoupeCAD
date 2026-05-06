#pragma once

#include "coupecad/core/id.h"
#include "coupecad/core/project.h"
#include "coupecad/renderer/entity_id.h"

#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Solid.hxx>

#include <unordered_map>
#include <vector>

namespace coupecad::renderer::occt {

class ViewDriver;

// Владеет AIS_InteractiveContext, поддерживает словари
// PanelId/HardwareItemId → Handle(AIS_Shape) и обратный словарь
// raw-pointer → EntityId для picking/selection.
//
// Hardware-методы добавляются в Task 7.
//
// Не thread-safe.
class AisScene {
public:
    AisScene(ViewDriver& driver, const core::Project& project);
    ~AisScene();

    AisScene(const AisScene&) = delete;
    AisScene& operator=(const AisScene&) = delete;

    const Handle(AIS_InteractiveContext)& context() const noexcept { return context_; }

    // Panel side.
    void add_panel(const core::PanelId& id, const TopoDS_Solid& solid);
    void replace_panel(const core::PanelId& id, const TopoDS_Solid& solid);
    void remove_panel(const core::PanelId& id);
    bool has_panel(const core::PanelId& id) const noexcept;
    std::size_t panel_count() const noexcept { return panel_objects_.size(); }
    std::vector<core::PanelId> panel_ids() const;

    // Hardware side.
    void add_hardware(const core::HardwareItemId& id, const TopoDS_Compound& compound);
    void replace_hardware(const core::HardwareItemId& id, const TopoDS_Compound& compound);
    void remove_hardware(const core::HardwareItemId& id);
    bool has_hardware(const core::HardwareItemId& id) const noexcept;
    std::size_t hardware_count() const noexcept { return hardware_objects_.size(); }
    std::vector<core::HardwareItemId> hardware_ids() const;

    // Re-resolve color for all panels whose effective material == mat_id.
    // If mat_id is the cabinet's default_panel_material, all panels without
    // a material_override get refreshed.
    void refresh_colors_for_material(const core::MaterialId& mat_id);

    // Clear all panels + hardware from the scene (used by rebuild_all in Task 9).
    void clear();

    // Test helpers (not part of stable API).
    Handle(AIS_Shape) raw_ais_handle_for_panel(const core::PanelId& id) const;
    const AIS_InteractiveObject* raw_ais_pointer_for_panel(
        const core::PanelId& id) const;
    const AIS_InteractiveObject* raw_ais_pointer_for_hardware(
        const core::HardwareItemId& id) const;

private:
    void erase_panel_internal(const core::PanelId& id);
    void erase_hardware_internal(const core::HardwareItemId& id);

    ViewDriver&                          driver_;
    const core::Project&                 project_;
    Handle(AIS_InteractiveContext)       context_;
    std::unordered_map<core::PanelId, Handle(AIS_Shape)> panel_objects_;
    std::unordered_map<core::HardwareItemId, Handle(AIS_Shape)> hardware_objects_;
    std::unordered_map<const AIS_InteractiveObject*, EntityId> ais_to_entity_;
};

}  // namespace coupecad::renderer::occt
