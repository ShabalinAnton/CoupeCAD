#include "coupecad/geometry/geometry_builder.h"

#include "coupecad/geometry/cabinet_shape.h"
#include "coupecad/geometry/hardware_shape.h"
#include "coupecad/geometry/panel_shape.h"
#include "coupecad/logging/logger.h"

namespace coupecad::geometry {

GeometryBuilder::GeometryBuilder(const core::Project& project)
    : project_(project) {}

void GeometryBuilder::rebuild_all() {
    coupecad::logging::Logger::instance().info(
        "geometry", "rebuild_all: dropping all caches");
    panel_cache_.clear();
    hardware_cache_.clear();
    compound_dirty_ = true;
}

void GeometryBuilder::apply_changes(const core::ChangeSet& cs) {
    if (!cs.empty()) {
        coupecad::logging::Logger::instance().debug(
            "geometry",
            "apply_changes: panels(+/-/u)={}/{}/{} hardware(+/-/u)={}/{}/{} cabinet={}",
            cs.added_panels.size(), cs.removed_panels.size(), cs.updated_panels.size(),
            cs.added_hardware.size(), cs.removed_hardware.size(), cs.updated_hardware.size(),
            cs.cabinet_changed ? 1 : 0);
    }
    // Удалённые / изменённые panels и hardware — выселяем из кешей.
    for (const auto& id : cs.removed_panels)    panel_cache_.erase(id);
    for (const auto& id : cs.updated_panels)    panel_cache_.erase(id);
    for (const auto& id : cs.removed_hardware)  hardware_cache_.erase(id);
    for (const auto& id : cs.updated_hardware)  hardware_cache_.erase(id);

    // Hardware-attachments позиционируются относительно панели. Сдвинулась
    // панель → надо пересчитать привязанную фурнитуру. Без анализа графа
    // (panel → attached items) делаем консервативно: чистим hardware_cache
    // целиком, если что-то менялось среди панелей.
    if (!cs.updated_panels.empty() || !cs.removed_panels.empty() ||
        cs.cabinet_changed) {
        hardware_cache_.clear();
    }

    // cabinet_changed → role-based панели могут получить другую геометрию.
    // Без анализа разности — чистим весь panel_cache_.
    if (cs.cabinet_changed) {
        panel_cache_.clear();
    }

    // added_panels / added_hardware — ничего не делаем, build on demand.
    // materials (added/removed/updated) — на shape не влияют.

    if (!cs.empty()) {
        compound_dirty_ = true;
    }
}

const TopoDS_Solid& GeometryBuilder::panel_solid(const core::PanelId& id) {
    auto it = panel_cache_.find(id);
    if (it != panel_cache_.end()) {
        return it->second;
    }
    coupecad::logging::Logger::instance().trace(
        "geometry", "build panel solid {}", id.to_string());
    const auto& panel = project_.cabinet().panels.at(id);
    auto [inserted_it, _] = panel_cache_.emplace(
        id, build_panel_solid(project_.cabinet(), panel));
    return inserted_it->second;
}

const TopoDS_Compound& GeometryBuilder::hardware_compound(
    const core::HardwareItemId& id) {
    auto it = hardware_cache_.find(id);
    if (it != hardware_cache_.end()) {
        return it->second;
    }
    coupecad::logging::Logger::instance().trace(
        "geometry", "build hardware compound {}", id.to_string());
    const auto& item = project_.cabinet().hardware.at(id);
    auto [inserted_it, _] = hardware_cache_.emplace(
        id, build_hardware_compound(project_, item));
    return inserted_it->second;
}

const TopoDS_Compound& GeometryBuilder::cabinet_compound() {
    if (!compound_dirty_) {
        return cabinet_compound_;
    }
    cabinet_compound_ = build_cabinet_compound(
        project_,
        [this](const core::PanelId& id) -> const TopoDS_Solid& {
            return panel_solid(id);
        },
        [this](const core::HardwareItemId& id) -> const TopoDS_Compound& {
            return hardware_compound(id);
        });
    compound_dirty_ = false;
    return cabinet_compound_;
}

bool GeometryBuilder::has_cached_panel(const core::PanelId& id) const noexcept {
    return panel_cache_.count(id) > 0;
}

bool GeometryBuilder::has_cached_hardware(
    const core::HardwareItemId& id) const noexcept {
    return hardware_cache_.count(id) > 0;
}

std::size_t GeometryBuilder::panel_cache_size() const noexcept {
    return panel_cache_.size();
}

std::size_t GeometryBuilder::hardware_cache_size() const noexcept {
    return hardware_cache_.size();
}

}  // namespace coupecad::geometry
