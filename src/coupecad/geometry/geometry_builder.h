#pragma once

#include "coupecad/core/cabinet.h"
#include "coupecad/core/commands/change_set.h"
#include "coupecad/core/hardware.h"
#include "coupecad/core/id.h"
#include "coupecad/core/panel.h"
#include "coupecad/core/project.h"

#include <TopoDS_Compound.hxx>
#include <TopoDS_Solid.hxx>

#include <unordered_map>

namespace coupecad::geometry {

// Stateful builder с per-panel и per-hardware кешем. Pull-модель:
// внешний код после команды/undo вызывает apply_changes(cs).
//
// Не thread-safe. Все методы вызываются из одного потока (UI).
//
// Время жизни возвращаемых ссылок — до следующего apply_changes()
// или rebuild_all(). После любой инвалидации кеша старые ссылки
// использовать нельзя.
class GeometryBuilder {
public:
    explicit GeometryBuilder(const core::Project& project);

    // Полный сброс кеша. Используется после deserialize() (Stage 1c).
    void rebuild_all();

    // Применить дельту: инвалидировать соответствующие entries.
    // Алгоритм — см. spec §5.2.
    void apply_changes(const core::ChangeSet& cs);

    // Геометрия одной панели; lazy — строится при первом запросе.
    const TopoDS_Solid& panel_solid(const core::PanelId& id);

    // Геометрия одного hardware-item; compound с одним solid на каждый attachment.
    const TopoDS_Compound& hardware_compound(const core::HardwareItemId& id);

    // Compound всего шкафа. Lazy — пересобирается, если что-то менялось.
    const TopoDS_Compound& cabinet_compound();

    // Диагностика / тесты.
    bool has_cached_panel(const core::PanelId& id) const noexcept;
    bool has_cached_hardware(const core::HardwareItemId& id) const noexcept;
    std::size_t panel_cache_size() const noexcept;
    std::size_t hardware_cache_size() const noexcept;

private:
    const core::Project& project_;
    std::unordered_map<core::PanelId, TopoDS_Solid> panel_cache_;
    std::unordered_map<core::HardwareItemId, TopoDS_Compound> hardware_cache_;
    TopoDS_Compound cabinet_compound_;
    bool compound_dirty_ = true;
};

}  // namespace coupecad::geometry
