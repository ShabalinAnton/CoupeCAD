#pragma once

#include "coupecad/core/cabinet.h"
#include "coupecad/core/commands/change_set.h"
#include "coupecad/core/hardware.h"
#include "coupecad/core/id.h"
#include "coupecad/core/material.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace coupecad::core {

// Метаданные проекта (имя, описание).
struct ProjectMeta {
    std::string name;
    std::string description;
    bool operator==(const ProjectMeta&) const = default;
};

class Project;

// Интерфейс наблюдателя за изменениями проекта.
class IProjectObserver {
public:
    virtual ~IProjectObserver() = default;
    virtual void on_changed(const Project& project, const ChangeSet& change) = 0;
};

class Project {
public:
    // Создаёт пустой проект с одним пустым Cabinet и одним базовым материалом.
    // Использует переданный UuidGenerator для всех новых id.
    static Project create_empty(std::string name,
                                std::unique_ptr<UuidGenerator> uuid_gen
                                    = make_random_uuid_generator());

    // Read-only доступ.
    const ProjectMeta& meta() const noexcept { return meta_; }
    const Cabinet& cabinet() const noexcept { return cabinet_; }
    const std::unordered_map<MaterialId, Material>& materials() const noexcept {
        return materials_;
    }
    const std::unordered_map<HardwareRef, HardwareSpec>&
        hardware_catalog() const noexcept { return hardware_catalog_; }

    UuidGenerator& uuid_gen() noexcept { return *uuid_gen_; }

    // Доступ к Cabinet для записи в Stage 1a (без команд) — возвращает
    // не-const ссылку. В Stage 1b эту лазейку закроют команды.
    Cabinet& mutable_cabinet() noexcept { return cabinet_; }
    std::unordered_map<MaterialId, Material>& mutable_materials() noexcept {
        return materials_;
    }
    std::unordered_map<HardwareRef, HardwareSpec>&
        mutable_hardware_catalog() noexcept { return hardware_catalog_; }
    ProjectMeta& mutable_meta() noexcept { return meta_; }

    // Полная валидация всего дерева. Бросает DomainError на первом
    // нарушении.
    void validate() const;

private:
    Project() = default;

    ProjectMeta meta_;
    Cabinet cabinet_;
    std::unordered_map<MaterialId, Material> materials_;
    std::unordered_map<HardwareRef, HardwareSpec> hardware_catalog_;
    std::unique_ptr<UuidGenerator> uuid_gen_;
};

}  // namespace coupecad::core
