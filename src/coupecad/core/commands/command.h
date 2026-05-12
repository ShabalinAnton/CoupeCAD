#pragma once

#include "coupecad/core/commands/change_set.h"

#include <any>
#include <string_view>

namespace coupecad::core {

class Project;

// Категория команды. Используется тестами и для диагностики; группировка
// родственных операций вокруг одной сущности.
enum class CommandKind {
    CabinetDimensions,
    CabinetDefaults,
    SetCabinetName,
    AddPanel,
    RemovePanel,
    UpdatePanelRoleParams,
    SetPanelMaterial,
    SetPanelThickness,
    SetPanelEdgeBanding,
    SetPanelLabel,
    SetPanelGrain,
    AddHardware,
    RemoveHardware,
    UpdateHardwareAttachments,
    AddMaterial,
    UpdateMaterial,
    RemoveMaterial,
    AddHardwareSpec,
    UpdateHardwareSpec,
    RemoveHardwareSpec,
    Macro,
};

// Базовый интерфейс. Команды хранят UUIDs + before/after и
// НЕ держат ссылок на Project.
class Command {
public:
    virtual ~Command() = default;
    virtual ChangeSet apply(Project& project) = 0;
    virtual ChangeSet revert(Project& project) = 0;
    virtual std::string_view label() const noexcept = 0;
    virtual CommandKind kind() const noexcept = 0;
};

// Команды, поддерживающие интерактивную правку. См. spec §3.3.2.
class PreviewableCommand : public Command {
public:
    // Изменить значение «на лету» в рамках активного preview-сеанса.
    virtual ChangeSet update(Project& project, const std::any& new_value) = 0;
};

}  // namespace coupecad::core
