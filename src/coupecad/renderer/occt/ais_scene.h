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

    // Только для тестов.
    const AIS_InteractiveObject* raw_ais_pointer_for_panel(
        const core::PanelId& id) const;

private:
    void erase_panel_internal(const core::PanelId& id);

    ViewDriver&                          driver_;
    const core::Project&                 project_;
    Handle(AIS_InteractiveContext)       context_;
    std::unordered_map<core::PanelId, Handle(AIS_Shape)> panel_objects_;
    std::unordered_map<const AIS_InteractiveObject*, EntityId> ais_to_entity_;
};

}  // namespace coupecad::renderer::occt
