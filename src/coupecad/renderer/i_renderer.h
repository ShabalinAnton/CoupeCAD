#pragma once

#include "coupecad/core/commands/change_set.h"
#include "coupecad/core/units.h"
#include "coupecad/renderer/entity_id.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace coupecad::renderer {

struct CameraState {
    core::Vec3  eye{};
    core::Vec3  target{};
    core::Vec3  up{core::Millimeters{0}, core::Millimeters{0}, core::Millimeters{1}};
    double      fov_deg = 45.0;   // 0 = orthographic, >0 = perspective
};

struct ViewportSize {
    int width  = 1;
    int height = 1;
};

// Stateful, single-threaded, не thread-safe. Хранит non-owning ссылки
// на Project и GeometryBuilder; оба должны пережить рендерер.
//
// Дизайн — см. docs/superpowers/specs/2026-05-04-stage-3-renderer-design.md
class IRenderer {
public:
    virtual ~IRenderer() = default;

    // §4 spec: ChangeSet-driven incremental sync. Тянет шейпы из
    // GeometryBuilder, переданного фабрикой.
    virtual void sync(const core::ChangeSet& cs) = 0;

    // Полная пересборка AIS-сцены с нуля. Вызывается после
    // GeometryBuilder::rebuild_all() (например, после deserialize).
    virtual void rebuild_all() = 0;

    // Камера.
    virtual void           set_camera(const CameraState& state) = 0;
    virtual CameraState    camera() const = 0;
    virtual void           fit_all() = 0;

    // Picking. Координаты — пиксели, origin = top-left.
    virtual std::optional<EntityId> pick(int x, int y) = 0;

    // Selection.
    virtual void                          select(const EntityId& id) = 0;
    virtual void                          deselect(const EntityId& id) = 0;
    virtual void                          clear_selection() = 0;
    virtual std::vector<EntityId>         selection() const = 0;

    // Viewport size.
    virtual void           set_viewport_size(ViewportSize size) = 0;
    virtual ViewportSize   viewport_size() const = 0;

    // Offscreen render → RGBA8, row-major, top-left origin.
    // Размер буфера = viewport_size().width * viewport_size().height * 4.
    // В headless-режиме без GL возвращает пустой vector + warning в лог.
    virtual std::vector<std::uint8_t> render_to_image() = 0;
};

}  // namespace coupecad::renderer
