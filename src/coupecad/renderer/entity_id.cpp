#include "coupecad/renderer/entity_id.h"

namespace std {

std::size_t hash<coupecad::renderer::EntityId>::operator()(
    const coupecad::renderer::EntityId& id) const noexcept {
    // Включаем index() в хеш, чтобы PanelId{X} и HardwareItemId{X}
    // c одинаковыми UUID давали разные значения hash.
    const std::size_t base = std::visit(
        [](const auto& v) -> std::size_t {
            using T = std::decay_t<decltype(v)>;
            return std::hash<T>{}(v);
        },
        id);
    const std::size_t mix = id.index() * 0x9E3779B97F4A7C15ULL;
    return base ^ (mix + 0x9E3779B9 + (base << 6) + (base >> 2));
}

}  // namespace std
