#pragma once

#include "coupecad/core/id.h"

#include <vector>

namespace coupecad::core {

// ChangeSet: какие сущности затронуты последней правкой. См. spec §3.5.
// UndoStack транслирует его наблюдателям после apply/revert/update.
struct ChangeSet {
    std::vector<PanelId> added_panels;
    std::vector<PanelId> removed_panels;
    std::vector<PanelId> updated_panels;

    std::vector<HardwareItemId> added_hardware;
    std::vector<HardwareItemId> removed_hardware;
    std::vector<HardwareItemId> updated_hardware;

    std::vector<MaterialId> added_materials;
    std::vector<MaterialId> removed_materials;
    std::vector<MaterialId> updated_materials;

    bool cabinet_changed = false;

    bool empty() const noexcept {
        return added_panels.empty() && removed_panels.empty() &&
               updated_panels.empty() && added_hardware.empty() &&
               removed_hardware.empty() && updated_hardware.empty() &&
               added_materials.empty() && removed_materials.empty() &&
               updated_materials.empty() && !cabinet_changed;
    }

    // Слияние дельт (для макросов): добавляет все списки other к this.
    void merge(const ChangeSet& other);
};

}  // namespace coupecad::core
