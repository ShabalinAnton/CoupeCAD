#pragma once

#include "coupecad/core/id.h"
#include "coupecad/core/units.h"

#include <optional>
#include <string>
#include <vector>

namespace coupecad::core {

enum class HardwareKind {
    Hinge,         // петля
    DrawerSlide,   // направляющая ящика
    Handle,        // ручка
    ShelfSupport,  // полкодержатель
    Connector,     // конфирмат / эксцентрик / минификс
    GasLift,       // газовая пружина
    Other,
};

const char* hardware_kind_name(HardwareKind kind) noexcept;

// Карточка фурнитуры в hardware_catalog проекта.
struct HardwareSpec {
    HardwareRef ref;
    HardwareKind kind = HardwareKind::Other;
    std::string name;
    std::optional<std::string> sku;
    Vec3 bbox{};                       // габариты (мм)
    std::optional<Money> price_each;

    bool operator==(const HardwareSpec&) const = default;

    // Бросает DomainError если:
    //  - ref пустой
    //  - name пустое
    //  - bbox имеет неположительное измерение
    void validate() const;
};

// Привязка фурнитуры к панели.
struct PanelAttachment {
    PanelId panel_id;
    Vec3 local_position{};
    Quat orientation = Quat::identity();

    bool operator==(const PanelAttachment&) const = default;
};

// Экземпляр фурнитуры в шкафу.
struct HardwareItem {
    HardwareItemId id;
    HardwareRef ref;
    std::vector<PanelAttachment> attachments;
    std::optional<std::string> label;

    bool operator==(const HardwareItem&) const = default;

    // Бросает DomainError если:
    //  - id или ref не valid
    //  - attachments пустой
    //  - какой-либо attachment.panel_id не valid
    void validate() const;
};

}  // namespace coupecad::core
