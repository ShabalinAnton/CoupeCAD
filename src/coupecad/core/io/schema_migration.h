#pragma once

#include <memory>
#include <nlohmann/json.hpp>
#include <vector>

namespace coupecad::core {

// Одна миграция: from_version -> to_version. apply получает JSON root
// со схемой `from_version`, возвращает JSON root со схемой `to_version`.
class SchemaMigration {
public:
    virtual ~SchemaMigration() = default;
    virtual int from_version() const = 0;
    virtual int to_version() const = 0;
    virtual nlohmann::json apply(nlohmann::json root) = 0;
};

// Цепочка миграций. Регистрируется по from-версии. `run()` получает
// корень JSON и прогоняет через необходимые step-by-step миграции до
// current_version.
class MigrationChain {
public:
    MigrationChain() = default;

    // Регистрация миграции. Каждая from_version должна быть уникальной —
    // дублирование бросает std::logic_error.
    void register_migration(std::unique_ptr<SchemaMigration> m);

    // Прогнать root через цепочку. root.schema_version должен быть <=
    // current_version; иначе UnsupportedVersion. После успешного прогона
    // root.schema_version == current_version.
    nlohmann::json run(nlohmann::json root, int current_version) const;

private:
    std::vector<std::unique_ptr<SchemaMigration>> migrations_;
};

// Production-цепочка (для использования CcadArchive). В v1 пустая.
MigrationChain default_migration_chain();

}  // namespace coupecad::core
