#pragma once

#include "coupecad/core/id.h"

#include <cstddef>
#include <variant>

namespace coupecad::renderer {

using EntityId = std::variant<core::PanelId, core::HardwareItemId>;

}  // namespace coupecad::renderer

namespace std {

template <>
struct hash<coupecad::renderer::EntityId> {
    std::size_t operator()(
        const coupecad::renderer::EntityId& id) const noexcept;
};

}  // namespace std
