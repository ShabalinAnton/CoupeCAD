#include "coupecad/core/errors.h"
#include "coupecad/core/io/schema_migration.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

namespace {
class V0ToV1 : public SchemaMigration {
public:
    int from_version() const override { return 0; }
    int to_version() const override { return 1; }
    nlohmann::json apply(nlohmann::json root) override {
        // Синтетическая миграция: добавляем поле `migrated: true` и
        // поле `meta` если отсутствует.
        root["migrated"] = true;
        if (!root.contains("meta")) root["meta"] = nlohmann::json::object();
        return root;
    }
};
}  // namespace

TEST(SchemaMigration, NoOpWhenCurrent) {
    MigrationChain chain;
    nlohmann::json root{{"schema_version", 1}};
    auto out = chain.run(std::move(root), 1);
    EXPECT_EQ(out.at("schema_version").get<int>(), 1);
}

TEST(SchemaMigration, RunsSingleMigration) {
    MigrationChain chain;
    chain.register_migration(std::make_unique<V0ToV1>());
    nlohmann::json root{{"schema_version", 0}};
    auto out = chain.run(std::move(root), 1);
    EXPECT_EQ(out.at("schema_version").get<int>(), 1);
    EXPECT_TRUE(out.at("migrated").get<bool>());
}

TEST(SchemaMigration, FutureVersionThrows) {
    MigrationChain chain;
    nlohmann::json root{{"schema_version", 99}};
    EXPECT_THROW(chain.run(std::move(root), 1), UnsupportedVersion);
}

TEST(SchemaMigration, MissingMigrationThrows) {
    MigrationChain chain;
    // нет миграции 0->1
    nlohmann::json root{{"schema_version", 0}};
    EXPECT_THROW(chain.run(std::move(root), 1), InvalidData);
}

TEST(SchemaMigration, DuplicateRegistrationThrows) {
    MigrationChain chain;
    chain.register_migration(std::make_unique<V0ToV1>());
    EXPECT_THROW(chain.register_migration(std::make_unique<V0ToV1>()),
                 std::logic_error);
}

TEST(SchemaMigration, MissingSchemaVersionThrows) {
    MigrationChain chain;
    nlohmann::json root{{"no_version", 0}};
    EXPECT_THROW(chain.run(std::move(root), 1), InvalidData);
}
