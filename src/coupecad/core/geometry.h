#pragma once

#include "coupecad/core/cabinet.h"
#include "coupecad/core/panel.h"
#include "coupecad/core/units.h"

namespace coupecad::core {

// Производная геометрия панели в локальной СК шкафа.
// origin — координаты левого-нижнего-переднего угла бокса панели;
// size  — габариты бокса (всегда положительные при валидной модели);
// orientation — для role-based панелей всегда identity, для Custom —
// из CustomParams.orientation.
struct PanelGeometry {
    Vec3 origin{};
    Vec3 size{};
    Quat orientation = Quat::identity();
};

// Возвращает фактические координаты и размеры панели.
// Бросает DomainError если role/role_params несовместимы с cabinet
// (например, Shelf.height_from_bottom выходит за высоту шкафа).
PanelGeometry compute_panel_geometry(const Cabinet& cabinet,
                                     const Panel& panel);

}  // namespace coupecad::core
