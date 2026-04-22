#include "coupecad/core/io/schema_migration.h"

#include "coupecad/core/errors.h"

#include <algorithm>
#include <stdexcept>

namespace coupecad::core {

void MigrationChain::register_migration(std::unique_ptr<SchemaMigration> m) {
    for (const auto& existing : migrations_) {
        if (existing->from_version() == m->from_version()) {
            throw std::logic_error{"Duplicate migration for from_version"};
        }
    }
    migrations_.push_back(std::move(m));
}

nlohmann::json MigrationChain::run(nlohmann::json root, int current_version) const {
    if (!root.is_object() || !root.contains("schema_version")) {
        throw InvalidData{"migration.missing_schema_version",
                          "root missing schema_version"};
    }
    int sv = root.at("schema_version").get<int>();
    if (sv > current_version) {
        throw UnsupportedVersion{"migration.future_version",
                                  "File schema_version is newer than application"};
    }
    while (sv < current_version) {
        auto it = std::find_if(migrations_.begin(), migrations_.end(),
                                [sv](const auto& m) { return m->from_version() == sv; });
        if (it == migrations_.end()) {
            throw InvalidData{"migration.no_path",
                              "No migration registered for from_version " +
                              std::to_string(sv)};
        }
        root = (*it)->apply(std::move(root));
        int next = (*it)->to_version();
        root["schema_version"] = next;
        sv = next;
    }
    return root;
}

MigrationChain default_migration_chain() {
    // В v1 пустая; миграции добавляются при bump'е схемы.
    return MigrationChain{};
}

}  // namespace coupecad::core
