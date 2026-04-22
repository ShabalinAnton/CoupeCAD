# Stage 1c — `.ccad` I/O Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Реализовать чтение и запись проектного формата `.ccad` (ZIP-контейнер + JSON-сериализация + миграции схемы + assets). Это завершает Stage 1 — после него Core полностью замкнут: Project можно создать/мутировать через команды/undo и сохранить в файл / открыть из файла.

**Architecture:** Слой в `src/coupecad/core/io/`. `ProjectSerializer` — интерфейс (serialize/deserialize bytes). Единственная реализация в v1 — `JsonProjectSerializer` поверх `nlohmann::json`. `SchemaMigration` — интерфейс для chained migrations (v1 пустой список, framework готов). `CcadArchive` — ZIP-обёртка поверх libzip с содержимым `meta.json` + `project.json` + `assets/`. Timestamps инъектируются через `IClock` для детерминированных тестов.

**Tech Stack:** C++20. Новые Conan-зависимости: `nlohmann_json/3.11.3` (уже подключён для logging, добавляется PUBLIC в coupecad_core), `libzip/1.10.1`. Всё остальное — как раньше. Qt не используется.

**Definition of Done.**
- `JsonProjectSerializer::serialize(Project) → bytes` → `deserialize(bytes) → Project` даёт bit-identical Project (при детерминированных seed UUID и clock).
- `CcadArchive::save(project, path, serializer)` пишет валидный ZIP с `meta.json` + `<data_file>` + `assets/`.
- `CcadArchive::load(path, serializer)` читает обратно, прогоняет migration chain, возвращает Project.
- Golden JSON fixture в `tests/core/io/golden/v1_reference.json` — любое изменение формата требует осознанного обновления.
- Synthetic migration (`v0 → v1`) в тестах проверяет pipeline.
- Покрытие `coupecad_core` ≥ 70% сохранено.
- CI зелёный (Linux + Windows).

---

## File Structure

После Stage 1c новые/изменённые файлы:

```
conanfile.py                              M  (+libzip)
src/coupecad/core/io/                     +  (новый подкаталог)
├── project_serializer.h                  +  (interface)
├── json_project_serializer.h/.cpp        +  (все entity <-> JSON)
├── schema_migration.h/.cpp               +  (interface + empty v1 chain)
└── ccad_archive.h/.cpp                   +  (ZIP + meta.json + orchestration)
src/coupecad/core/CMakeLists.txt          M  (+ 3 .cpp файла, + libzip PRIVATE link, + nlohmann_json PUBLIC)

tests/core/io/                            +
├── CMakeLists.txt                        +
├── json_roundtrip_test.cpp               +  (round-trip через serializer)
├── json_variant_test.cpp                 +  (RoleParams variant углы)
├── schema_migration_test.cpp             +  (synthetic v0→v1)
├── ccad_archive_test.cpp                 +  (save/load + errors)
├── assets_test.cpp                       +  (PNG в assets/)
└── golden/
    └── v1_reference.json                 +  (snapshot формата)
tests/core/CMakeLists.txt                 M  (+add_subdirectory(io))
```

**Ответственности:**
- `project_serializer.h` — чистый интерфейс для swap реализаций (будущий binary encoding из §4.8 спеки).
- `json_project_serializer.h/.cpp` — конверторы units ↔ JSON, entity ↔ JSON, детерминированная сортировка, чтение с migration.
- `schema_migration.h/.cpp` — `SchemaMigration` interface + `MigrationChain` с регистрацией и прогоном.
- `ccad_archive.h/.cpp` — `CcadArchive::save/load` — ZIP-обёртка поверх libzip, `meta.json` сборка/разбор, вызов serializer.

---

## Task 1: libzip dep + ProjectSerializer interface + FileFormatError subclasses

**Files:**
- Modify: `conanfile.py`
- Create: `src/coupecad/core/io/project_serializer.h`
- Modify: `src/coupecad/core/errors.h` (добавить FileFormatError subclasses)
- Modify: `src/coupecad/core/CMakeLists.txt` (пока без новых cpp)
- Create: `tests/core/io/CMakeLists.txt`
- Create: `tests/core/io/serializer_interface_test.cpp`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1: Дополнить `conanfile.py`** — добавить libzip и nlohmann_json.

Открыть `conanfile.py`. Найти метод `requirements` и заменить на:

```python
    def requirements(self):
        self.requires("spdlog/1.13.0")
        self.requires("fmt/10.2.1")
        self.requires("stduuid/1.2.3")
        self.requires("nlohmann_json/3.11.3")
        self.requires("libzip/1.10.1")
        self.test_requires("gtest/1.14.0")
```

- [ ] **Step 2: `conan install`**.

```sh
conan install . --build=missing -s build_type=Debug -s compiler.cppstd=20
```

Ожидается: `Install finished successfully`. Если libzip собирается из исходников — это несколько минут, нормально.

- [ ] **Step 3: Создать `src/coupecad/core/io/project_serializer.h`.**

```cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace coupecad::core {

class Project;

// Интерфейс сериализатора проекта. В v1 — одна реализация
// JsonProjectSerializer. Задел под binary форматы (CBOR/MessagePack/…) в §4.8.
class ProjectSerializer {
public:
    virtual ~ProjectSerializer() = default;

    // Человекочитаемое имя encoding'а (попадает в meta.json content_encoding).
    // Для JsonProjectSerializer вернёт "json".
    virtual const char* content_encoding() const noexcept = 0;

    // Сериализовать Project в байты. Байты — полезная нагрузка для записи
    // в ZIP под именем `project.<ext>` (ext выбирает CcadArchive по encoding).
    virtual std::vector<std::uint8_t> serialize(const Project& project) const = 0;

    // Десериализовать байты в Project. Бросает FileFormatError::InvalidData
    // при несоответствии схеме. Schema-миграции выполняются CcadArchive'ом
    // ДО вызова deserialize — сюда приходит актуальная current-схема.
    virtual Project deserialize(const std::vector<std::uint8_t>& bytes) const = 0;
};

}  // namespace coupecad::core
```

- [ ] **Step 4: Дополнить `src/coupecad/core/errors.h`** — добавить `FileFormatError` subclasses. Найти конец namespace перед `}  // namespace coupecad::core` и вставить перед ним:

```cpp
// Конкретные ошибки формата `.ccad`. Все наследуют FileFormatError.
class CorruptedArchive     : public FileFormatError { using FileFormatError::FileFormatError; };
class MissingManifest      : public FileFormatError { using FileFormatError::FileFormatError; };
class ChecksumMismatch     : public FileFormatError { using FileFormatError::FileFormatError; };
class UnsupportedVersion   : public FileFormatError { using FileFormatError::FileFormatError; };
class UnsupportedEncoding  : public FileFormatError { using FileFormatError::FileFormatError; };
class InvalidData          : public FileFormatError { using FileFormatError::FileFormatError; };
```

- [ ] **Step 5: Создать `tests/core/io/CMakeLists.txt`.**

```cmake
add_executable(coupecad_io_test
    serializer_interface_test.cpp
)

target_link_libraries(coupecad_io_test
    PRIVATE
        coupecad_core
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_io_test)
```

- [ ] **Step 6: Создать `tests/core/io/serializer_interface_test.cpp`.**

```cpp
#include "coupecad/core/errors.h"
#include "coupecad/core/io/project_serializer.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

TEST(FileFormatError, SubclassesExist) {
    EXPECT_STREQ(CorruptedArchive{"cf.corrupt", "x"}.what(), "x");
    EXPECT_STREQ(MissingManifest{"cf.missing", "y"}.what(), "y");
    EXPECT_STREQ(ChecksumMismatch{"cf.checksum", "z"}.what(), "z");
    EXPECT_STREQ(UnsupportedVersion{"cf.ver", "w"}.what(), "w");
    EXPECT_STREQ(UnsupportedEncoding{"cf.enc", "u"}.what(), "u");
    EXPECT_STREQ(InvalidData{"cf.data", "v"}.what(), "v");
}

TEST(FileFormatError, InheritsFileFormatError) {
    try {
        throw CorruptedArchive{"cf.corrupt", "x"};
    } catch (const FileFormatError&) {
        SUCCEED();
    } catch (...) {
        FAIL() << "CorruptedArchive not a FileFormatError";
    }
}

TEST(ProjectSerializer, InterfaceIsAbstract) {
    static_assert(!std::is_default_constructible_v<ProjectSerializer>,
                  "ProjectSerializer must be abstract");
}
```

- [ ] **Step 7: Обновить `tests/core/CMakeLists.txt`** — добавить `add_subdirectory(io)` ПОСЛЕ `add_subdirectory(commands)`:

```cmake
# ... существующее содержимое coupecad_core_test
add_subdirectory(commands)
add_subdirectory(io)
```

- [ ] **Step 8: Build + test.**

```sh
export Qt6_DIR=$HOME/Qt/6.7.3/macos/lib/cmake/Qt6
cmake --preset default
cmake --build --preset default --target coupecad_io_test
ctest --preset default -R "FileFormatError|ProjectSerializer" --output-on-failure
```

Ожидается: 3 теста passed.

- [ ] **Step 9: Commit.**

```sh
git add conanfile.py src/coupecad/core/errors.h \
        src/coupecad/core/io/project_serializer.h \
        tests/core/io/ tests/core/CMakeLists.txt
git commit -m "feat(core/io): add libzip + nlohmann_json deps, ProjectSerializer interface, FileFormatError subclasses"
```

---

## Task 2: JsonProjectSerializer — примитивы (Millimeters, Vec3, Quat, Money, RGBA, Dimensions, IDs)

**Files:**
- Create: `src/coupecad/core/io/json_project_serializer.h`
- Create: `src/coupecad/core/io/json_project_serializer.cpp` (только примитивы; остальные entity в следующих тасках)
- Modify: `src/coupecad/core/CMakeLists.txt`
- Create: `tests/core/io/json_primitives_test.cpp`
- Modify: `tests/core/io/CMakeLists.txt`

- [ ] **Step 1: Создать `src/coupecad/core/io/json_project_serializer.h`.**

```cpp
#pragma once

#include "coupecad/core/io/project_serializer.h"

#include <nlohmann/json.hpp>

namespace coupecad::core {

class Project;

// JSON-реализация ProjectSerializer поверх nlohmann::json.
// Сериализация детерминированная: массивы сортируются по id/ref,
// ключи объектов в фиксированном порядке.
class JsonProjectSerializer : public ProjectSerializer {
public:
    const char* content_encoding() const noexcept override { return "json"; }

    std::vector<std::uint8_t> serialize(const Project& project) const override;
    Project deserialize(const std::vector<std::uint8_t>& bytes) const override;
};

// Внутренние converter'ы (header-visible для unit-тестов отдельных типов).
// Все to_json / from_json работают с nlohmann::json напрямую.
namespace detail {

nlohmann::json to_json_millimeters(class Millimeters m);
class Millimeters from_json_millimeters(const nlohmann::json& j);

nlohmann::json to_json_dimensions(const struct Dimensions& d);
struct Dimensions from_json_dimensions(const nlohmann::json& j);

nlohmann::json to_json_vec3(const struct Vec3& v);
struct Vec3 from_json_vec3(const nlohmann::json& j);

nlohmann::json to_json_quat(const struct Quat& q);
struct Quat from_json_quat(const nlohmann::json& j);

nlohmann::json to_json_money(const struct Money& m);
struct Money from_json_money(const nlohmann::json& j);

nlohmann::json to_json_rgba(const struct RGBA& c);
struct RGBA from_json_rgba(const nlohmann::json& j);

// IDs — как hex-строки.
template <class Tag>
nlohmann::json to_json_id(const class Id<Tag>& id);
template <class Tag>
class Id<Tag> from_json_id(const nlohmann::json& j);

}  // namespace detail

}  // namespace coupecad::core
```

- [ ] **Step 2: Создать `src/coupecad/core/io/json_project_serializer.cpp`** (сейчас только примитивы + заглушки serialize/deserialize).

```cpp
#include "coupecad/core/io/json_project_serializer.h"

#include "coupecad/core/errors.h"
#include "coupecad/core/id.h"
#include "coupecad/core/units.h"

namespace coupecad::core {

namespace detail {

nlohmann::json to_json_millimeters(Millimeters m) { return m.value(); }

Millimeters from_json_millimeters(const nlohmann::json& j) {
    if (!j.is_number_integer()) {
        throw InvalidData{"json.millimeters_not_int",
                          "Expected integer for Millimeters"};
    }
    return Millimeters{j.get<std::int32_t>()};
}

nlohmann::json to_json_dimensions(const Dimensions& d) {
    return nlohmann::json{
        {"width", d.width.value()},
        {"depth", d.depth.value()},
        {"height", d.height.value()},
    };
}

Dimensions from_json_dimensions(const nlohmann::json& j) {
    if (!j.is_object()) {
        throw InvalidData{"json.dimensions_not_object",
                          "Dimensions must be an object"};
    }
    return Dimensions{
        .width = from_json_millimeters(j.at("width")),
        .depth = from_json_millimeters(j.at("depth")),
        .height = from_json_millimeters(j.at("height")),
    };
}

nlohmann::json to_json_vec3(const Vec3& v) {
    return nlohmann::json::array({v.x.value(), v.y.value(), v.z.value()});
}

Vec3 from_json_vec3(const nlohmann::json& j) {
    if (!j.is_array() || j.size() != 3) {
        throw InvalidData{"json.vec3_shape",
                          "Vec3 must be array of 3 integers"};
    }
    return Vec3{from_json_millimeters(j[0]),
                from_json_millimeters(j[1]),
                from_json_millimeters(j[2])};
}

nlohmann::json to_json_quat(const Quat& q) {
    return nlohmann::json::array({q.w, q.x, q.y, q.z});
}

Quat from_json_quat(const nlohmann::json& j) {
    if (!j.is_array() || j.size() != 4) {
        throw InvalidData{"json.quat_shape",
                          "Quat must be array of 4 doubles"};
    }
    return Quat{j[0].get<double>(), j[1].get<double>(),
                j[2].get<double>(), j[3].get<double>()};
}

nlohmann::json to_json_money(const Money& m) { return m.minor_units; }

Money from_json_money(const nlohmann::json& j) {
    if (!j.is_number_integer()) {
        throw InvalidData{"json.money_not_int", "Money must be integer"};
    }
    return Money{.minor_units = j.get<std::int64_t>()};
}

nlohmann::json to_json_rgba(const RGBA& c) {
    return nlohmann::json::array({c.r, c.g, c.b, c.a});
}

RGBA from_json_rgba(const nlohmann::json& j) {
    if (!j.is_array() || j.size() != 4) {
        throw InvalidData{"json.rgba_shape",
                          "RGBA must be array of 4 uint8s"};
    }
    return RGBA{j[0].get<std::uint8_t>(), j[1].get<std::uint8_t>(),
                j[2].get<std::uint8_t>(), j[3].get<std::uint8_t>()};
}

// Template instantiations for Id<Tag> — определяются в .cpp через явные
// специализации через тэги, которые мы знаем (Cabinet/Panel/Material/Hardware).
template <class Tag>
nlohmann::json to_json_id(const Id<Tag>& id) {
    return id.is_valid() ? nlohmann::json(id.to_string()) : nlohmann::json(nullptr);
}

template <class Tag>
Id<Tag> from_json_id(const nlohmann::json& j) {
    if (j.is_null()) return Id<Tag>{};
    if (!j.is_string()) {
        throw InvalidData{"json.id_not_string",
                          "Id must be canonical UUID hex string or null"};
    }
    auto parsed = Id<Tag>::from_string(j.get<std::string>());
    if (!parsed.is_valid() && !j.get<std::string>().empty()) {
        throw InvalidData{"json.id_parse_failed",
                          "Id string is not a valid UUID"};
    }
    return parsed;
}

// Явные инстанциации для всех id-типов, которые будут сериализованы.
template nlohmann::json to_json_id(const CabinetId&);
template nlohmann::json to_json_id(const PanelId&);
template nlohmann::json to_json_id(const MaterialId&);
template nlohmann::json to_json_id(const HardwareItemId&);
template CabinetId from_json_id(const nlohmann::json&);
template PanelId from_json_id(const nlohmann::json&);
template MaterialId from_json_id(const nlohmann::json&);
template HardwareItemId from_json_id(const nlohmann::json&);

}  // namespace detail

// Заглушки для будущих Task 3-5.
std::vector<std::uint8_t> JsonProjectSerializer::serialize(const Project&) const {
    throw InvalidData{"json.not_implemented_yet",
                      "JsonProjectSerializer::serialize pending Task 5"};
}

Project JsonProjectSerializer::deserialize(const std::vector<std::uint8_t>&) const {
    throw InvalidData{"json.not_implemented_yet",
                      "JsonProjectSerializer::deserialize pending Task 5"};
}

}  // namespace coupecad::core
```

- [ ] **Step 3: Обновить `src/coupecad/core/CMakeLists.txt`** — добавить `io/json_project_serializer.cpp` в sources и `nlohmann_json::nlohmann_json` PUBLIC:

```cmake
find_package(stduuid REQUIRED)
find_package(fmt REQUIRED)
find_package(nlohmann_json REQUIRED)

add_library(coupecad_core STATIC
    id.cpp
    material.cpp
    hardware.cpp
    panel.cpp
    cabinet.cpp
    project.cpp
    geometry.cpp
    commands/change_set.cpp
    commands/cabinet_commands.cpp
    commands/panel_commands.cpp
    commands/material_commands.cpp
    commands/hardware_commands.cpp
    commands/hardware_spec_commands.cpp
    commands/macro_command.cpp
    undo_stack.cpp
    io/json_project_serializer.cpp
)

target_include_directories(coupecad_core
    PUBLIC ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(coupecad_core
    PUBLIC
        coupecad_logging
        fmt::fmt
        stduuid::stduuid
        nlohmann_json::nlohmann_json
)

target_compile_features(coupecad_core PUBLIC cxx_std_20)
```

- [ ] **Step 4: Создать `tests/core/io/json_primitives_test.cpp`.**

```cpp
#include "coupecad/core/errors.h"
#include "coupecad/core/id.h"
#include "coupecad/core/io/json_project_serializer.h"
#include "coupecad/core/units.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using namespace coupecad::core;
using namespace coupecad::core::detail;

TEST(JsonPrimitives, MillimetersRoundTrip) {
    auto j = to_json_millimeters(Millimeters{1500});
    EXPECT_EQ(j.get<int>(), 1500);
    EXPECT_EQ(from_json_millimeters(j), Millimeters{1500});
}

TEST(JsonPrimitives, MillimetersNegative) {
    auto j = to_json_millimeters(Millimeters{-7});
    EXPECT_EQ(from_json_millimeters(j), Millimeters{-7});
}

TEST(JsonPrimitives, MillimetersRejectsNonInt) {
    nlohmann::json j = 3.14;
    EXPECT_THROW(from_json_millimeters(j), InvalidData);
}

TEST(JsonPrimitives, DimensionsRoundTrip) {
    Dimensions d{.width = Millimeters{2400},
                 .depth = Millimeters{600},
                 .height = Millimeters{2500}};
    EXPECT_EQ(from_json_dimensions(to_json_dimensions(d)), d);
}

TEST(JsonPrimitives, Vec3RoundTrip) {
    Vec3 v{Millimeters{10}, Millimeters{-20}, Millimeters{30}};
    EXPECT_EQ(from_json_vec3(to_json_vec3(v)), v);
}

TEST(JsonPrimitives, Vec3RejectsWrongShape) {
    nlohmann::json j = nlohmann::json::array({1, 2});
    EXPECT_THROW(from_json_vec3(j), InvalidData);
}

TEST(JsonPrimitives, QuatRoundTrip) {
    Quat q{0.7071, 0.0, 0.7071, 0.0};
    auto j = to_json_quat(q);
    auto back = from_json_quat(j);
    EXPECT_DOUBLE_EQ(back.w, 0.7071);
    EXPECT_DOUBLE_EQ(back.y, 0.7071);
}

TEST(JsonPrimitives, MoneyRoundTrip) {
    EXPECT_EQ(from_json_money(to_json_money(Money{.minor_units = 12345})).minor_units, 12345);
}

TEST(JsonPrimitives, RGBARoundTrip) {
    RGBA c{240, 100, 50, 255};
    EXPECT_EQ(from_json_rgba(to_json_rgba(c)), c);
}

TEST(JsonPrimitives, IdRoundTrip) {
    auto gen = make_seeded_uuid_generator(42);
    PanelId id{gen->next()};
    auto j = to_json_id(id);
    EXPECT_TRUE(j.is_string());
    EXPECT_EQ(from_json_id<PanelIdTag>(j), id);
}

TEST(JsonPrimitives, NullIdIsInvalid) {
    nlohmann::json j = nullptr;
    auto parsed = from_json_id<PanelIdTag>(j);
    EXPECT_FALSE(parsed.is_valid());
}

TEST(JsonPrimitives, MalformedIdThrows) {
    nlohmann::json j = "not-a-uuid";
    EXPECT_THROW(from_json_id<PanelIdTag>(j), InvalidData);
}
```

- [ ] **Step 5: Обновить `tests/core/io/CMakeLists.txt`** — добавить `json_primitives_test.cpp`:

```cmake
add_executable(coupecad_io_test
    serializer_interface_test.cpp
    json_primitives_test.cpp
)

target_link_libraries(coupecad_io_test
    PRIVATE
        coupecad_core
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_io_test)
```

- [ ] **Step 6: Build + test.**

```sh
cmake --build --preset default --target coupecad_io_test
ctest --preset default -R "JsonPrimitives" --output-on-failure
```

Ожидается: 12 тестов passed.

- [ ] **Step 7: Commit.**

```sh
git add src/coupecad/core/io/ src/coupecad/core/CMakeLists.txt \
        tests/core/io/json_primitives_test.cpp tests/core/io/CMakeLists.txt
git commit -m "feat(core/io): JsonProjectSerializer primitives (Millimeters/Vec3/Quat/Money/RGBA/Id)"
```

---

## Task 3: JsonProjectSerializer — Material, EdgeBanding, HardwareSpec, HardwareItem

**Files:**
- Modify: `src/coupecad/core/io/json_project_serializer.h` (+ declarations)
- Modify: `src/coupecad/core/io/json_project_serializer.cpp` (+ impls)
- Create: `tests/core/io/json_entities_test.cpp`
- Modify: `tests/core/io/CMakeLists.txt`

- [ ] **Step 1: Дополнить `json_project_serializer.h`** — в `namespace detail` перед закрывающей скобкой добавить:

```cpp
// --- Material ---
nlohmann::json to_json_material(const class Material& m);
class Material from_json_material(const nlohmann::json& j);

// --- EdgeBanding ---
nlohmann::json to_json_edge_banding(const struct EdgeBanding& eb);
struct EdgeBanding from_json_edge_banding(const nlohmann::json& j);

nlohmann::json to_json_panel_edge_banding(const struct PanelEdgeBanding& eb);
struct PanelEdgeBanding from_json_panel_edge_banding(const nlohmann::json& j);

// --- HardwareSpec ---
nlohmann::json to_json_hardware_spec(const struct HardwareSpec& s);
struct HardwareSpec from_json_hardware_spec(const nlohmann::json& j);

// --- HardwareItem / PanelAttachment ---
nlohmann::json to_json_panel_attachment(const struct PanelAttachment& a);
struct PanelAttachment from_json_panel_attachment(const nlohmann::json& j);

nlohmann::json to_json_hardware_item(const struct HardwareItem& h);
struct HardwareItem from_json_hardware_item(const nlohmann::json& j);

// Вспомогательные перечисления <-> строки.
const char* material_kind_to_str(enum class MaterialKind k);
enum class MaterialKind material_kind_from_str(const std::string& s);
const char* hardware_kind_to_str(enum class HardwareKind k);
enum class HardwareKind hardware_kind_from_str(const std::string& s);
```

Добавить в top-of-file includes:
```cpp
#include "coupecad/core/hardware.h"
#include "coupecad/core/material.h"
#include "coupecad/core/panel.h"
```

- [ ] **Step 2: Дополнить `json_project_serializer.cpp`** — перед `JsonProjectSerializer::serialize` (но внутри `detail` namespace для внутренних функций, или как в файле) добавить:

```cpp
namespace detail {

const char* material_kind_to_str(MaterialKind k) {
    return material_kind_name(k);   // используем канонич. имена из material.cpp
}

MaterialKind material_kind_from_str(const std::string& s) {
    if (s == "ChipboardLaminated") return MaterialKind::ChipboardLaminated;
    if (s == "Mdf")               return MaterialKind::Mdf;
    if (s == "Hdf")               return MaterialKind::Hdf;
    if (s == "Plywood")           return MaterialKind::Plywood;
    if (s == "SolidWood")         return MaterialKind::SolidWood;
    if (s == "Glass")             return MaterialKind::Glass;
    if (s == "Metal")             return MaterialKind::Metal;
    if (s == "Other")             return MaterialKind::Other;
    throw InvalidData{"json.material_kind_unknown",
                      "unknown MaterialKind: " + s};
}

const char* hardware_kind_to_str(HardwareKind k) {
    return hardware_kind_name(k);
}

HardwareKind hardware_kind_from_str(const std::string& s) {
    if (s == "Hinge")        return HardwareKind::Hinge;
    if (s == "DrawerSlide")  return HardwareKind::DrawerSlide;
    if (s == "Handle")       return HardwareKind::Handle;
    if (s == "ShelfSupport") return HardwareKind::ShelfSupport;
    if (s == "Connector")    return HardwareKind::Connector;
    if (s == "GasLift")      return HardwareKind::GasLift;
    if (s == "Other")        return HardwareKind::Other;
    throw InvalidData{"json.hardware_kind_unknown",
                      "unknown HardwareKind: " + s};
}

nlohmann::json to_json_material(const Material& m) {
    nlohmann::json j;
    j["id"] = to_json_id(m.id);
    j["name"] = m.name;
    j["kind"] = material_kind_to_str(m.kind);
    j["default_thickness_mm"] = m.default_thickness.value();
    j["color_hint"] = to_json_rgba(m.color_hint);
    j["texture_ref"] = m.texture_ref ? nlohmann::json(*m.texture_ref) : nlohmann::json(nullptr);
    j["price_per_sqm"] = m.price_per_sqm ? to_json_money(*m.price_per_sqm) : nlohmann::json(nullptr);
    return j;
}

Material from_json_material(const nlohmann::json& j) {
    Material m;
    m.id = from_json_id<MaterialIdTag>(j.at("id"));
    m.name = j.at("name").get<std::string>();
    m.kind = material_kind_from_str(j.at("kind").get<std::string>());
    m.default_thickness = from_json_millimeters(j.at("default_thickness_mm"));
    m.color_hint = from_json_rgba(j.at("color_hint"));
    if (!j.at("texture_ref").is_null()) {
        m.texture_ref = j.at("texture_ref").get<std::string>();
    }
    if (!j.at("price_per_sqm").is_null()) {
        m.price_per_sqm = from_json_money(j.at("price_per_sqm"));
    }
    return m;
}

nlohmann::json to_json_edge_banding(const EdgeBanding& eb) {
    nlohmann::json j;
    j["material_id"] = to_json_id(eb.material_id);
    j["thickness_mm"] = eb.thickness.value();
    return j;
}

EdgeBanding from_json_edge_banding(const nlohmann::json& j) {
    EdgeBanding eb;
    eb.material_id = from_json_id<MaterialIdTag>(j.at("material_id"));
    eb.thickness = from_json_millimeters(j.at("thickness_mm"));
    return eb;
}

nlohmann::json to_json_panel_edge_banding(const PanelEdgeBanding& eb) {
    auto side = [](const std::optional<EdgeBanding>& s) -> nlohmann::json {
        return s ? to_json_edge_banding(*s) : nlohmann::json(nullptr);
    };
    return nlohmann::json{
        {"front", side(eb.front)},
        {"back", side(eb.back)},
        {"left", side(eb.left)},
        {"right", side(eb.right)},
    };
}

PanelEdgeBanding from_json_panel_edge_banding(const nlohmann::json& j) {
    auto side = [&](const char* key) -> std::optional<EdgeBanding> {
        const auto& v = j.at(key);
        if (v.is_null()) return std::nullopt;
        return from_json_edge_banding(v);
    };
    return PanelEdgeBanding{
        .front = side("front"),
        .back = side("back"),
        .left = side("left"),
        .right = side("right"),
    };
}

nlohmann::json to_json_hardware_spec(const HardwareSpec& s) {
    nlohmann::json j;
    j["ref"] = s.ref.value();
    j["kind"] = hardware_kind_to_str(s.kind);
    j["name"] = s.name;
    j["sku"] = s.sku ? nlohmann::json(*s.sku) : nlohmann::json(nullptr);
    j["bbox_mm"] = to_json_vec3(s.bbox);
    j["price_each"] = s.price_each ? to_json_money(*s.price_each) : nlohmann::json(nullptr);
    return j;
}

HardwareSpec from_json_hardware_spec(const nlohmann::json& j) {
    HardwareSpec s;
    s.ref = HardwareRef{j.at("ref").get<std::string>()};
    s.kind = hardware_kind_from_str(j.at("kind").get<std::string>());
    s.name = j.at("name").get<std::string>();
    if (!j.at("sku").is_null()) s.sku = j.at("sku").get<std::string>();
    s.bbox = from_json_vec3(j.at("bbox_mm"));
    if (!j.at("price_each").is_null()) s.price_each = from_json_money(j.at("price_each"));
    return s;
}

nlohmann::json to_json_panel_attachment(const PanelAttachment& a) {
    return nlohmann::json{
        {"panel_id", to_json_id(a.panel_id)},
        {"local_position_mm", to_json_vec3(a.local_position)},
        {"orientation", to_json_quat(a.orientation)},
    };
}

PanelAttachment from_json_panel_attachment(const nlohmann::json& j) {
    return PanelAttachment{
        .panel_id = from_json_id<PanelIdTag>(j.at("panel_id")),
        .local_position = from_json_vec3(j.at("local_position_mm")),
        .orientation = from_json_quat(j.at("orientation")),
    };
}

nlohmann::json to_json_hardware_item(const HardwareItem& h) {
    nlohmann::json atts = nlohmann::json::array();
    for (const auto& a : h.attachments) atts.push_back(to_json_panel_attachment(a));
    return nlohmann::json{
        {"id", to_json_id(h.id)},
        {"ref", h.ref.value()},
        {"attachments", atts},
        {"label", h.label ? nlohmann::json(*h.label) : nlohmann::json(nullptr)},
    };
}

HardwareItem from_json_hardware_item(const nlohmann::json& j) {
    HardwareItem h;
    h.id = from_json_id<HardwareItemIdTag>(j.at("id"));
    h.ref = HardwareRef{j.at("ref").get<std::string>()};
    for (const auto& a : j.at("attachments")) {
        h.attachments.push_back(from_json_panel_attachment(a));
    }
    if (!j.at("label").is_null()) h.label = j.at("label").get<std::string>();
    return h;
}

}  // namespace detail
```

- [ ] **Step 2: Создать `tests/core/io/json_entities_test.cpp`** (тесты round-trip для Material/EdgeBanding/HardwareSpec/HardwareItem).

```cpp
#include "coupecad/core/hardware.h"
#include "coupecad/core/io/json_project_serializer.h"
#include "coupecad/core/material.h"
#include "coupecad/core/panel.h"

#include <gtest/gtest.h>

using namespace coupecad::core;
using namespace coupecad::core::detail;

namespace {
auto gen = make_seeded_uuid_generator(777);

Material make_material() {
    return Material{
        .id = MaterialId{gen->next()},
        .name = "Test Material",
        .kind = MaterialKind::Mdf,
        .default_thickness = Millimeters{18},
        .color_hint = RGBA{200, 150, 100, 255},
        .texture_ref = std::nullopt,
        .price_per_sqm = Money{.minor_units = 120000},
    };
}
}  // namespace

TEST(JsonEntities, MaterialRoundTrip) {
    auto m = make_material();
    auto j = to_json_material(m);
    auto back = from_json_material(j);
    EXPECT_EQ(back, m);
}

TEST(JsonEntities, MaterialKindStrings) {
    EXPECT_EQ(material_kind_from_str("Mdf"), MaterialKind::Mdf);
    EXPECT_THROW(material_kind_from_str("BogusKind"), InvalidData);
}

TEST(JsonEntities, EdgeBandingRoundTrip) {
    EdgeBanding eb{.material_id = MaterialId{gen->next()},
                    .thickness = Millimeters{2}};
    EXPECT_EQ(from_json_edge_banding(to_json_edge_banding(eb)), eb);
}

TEST(JsonEntities, PanelEdgeBandingRoundTrip) {
    PanelEdgeBanding eb;
    eb.front = EdgeBanding{.material_id = MaterialId{gen->next()},
                            .thickness = Millimeters{2}};
    auto j = to_json_panel_edge_banding(eb);
    auto back = from_json_panel_edge_banding(j);
    EXPECT_EQ(back, eb);
}

TEST(JsonEntities, HardwareSpecRoundTrip) {
    HardwareSpec s{.ref = HardwareRef{"hinge.test"},
                    .kind = HardwareKind::Hinge,
                    .name = "Hinge 90",
                    .sku = std::nullopt,
                    .bbox = Vec3{Millimeters{35}, Millimeters{14}, Millimeters{60}},
                    .price_each = Money{.minor_units = 4500}};
    EXPECT_EQ(from_json_hardware_spec(to_json_hardware_spec(s)), s);
}

TEST(JsonEntities, HardwareKindStrings) {
    EXPECT_EQ(hardware_kind_from_str("Connector"), HardwareKind::Connector);
    EXPECT_THROW(hardware_kind_from_str("WtfKind"), InvalidData);
}

TEST(JsonEntities, HardwareItemRoundTrip) {
    HardwareItem h;
    h.id = HardwareItemId{gen->next()};
    h.ref = HardwareRef{"test.ref"};
    h.attachments.push_back(PanelAttachment{
        .panel_id = PanelId{gen->next()},
        .local_position = Vec3{Millimeters{5}, Millimeters{10}, Millimeters{15}},
        .orientation = Quat::identity(),
    });
    h.label = "My hinge";
    EXPECT_EQ(from_json_hardware_item(to_json_hardware_item(h)), h);
}
```

- [ ] **Step 3: Обновить `tests/core/io/CMakeLists.txt`** — добавить `json_entities_test.cpp`.

- [ ] **Step 4: Build + test.**

```sh
cmake --build --preset default --target coupecad_io_test
ctest --preset default -R "JsonEntities" --output-on-failure
```

Ожидается: 7 тестов passed.

- [ ] **Step 5: Commit.**

```sh
git add src/coupecad/core/io/json_project_serializer.* tests/core/io/
git commit -m "feat(core/io): JSON round-trip for Material/EdgeBanding/HardwareSpec/HardwareItem"
```

---

## Task 4: JsonProjectSerializer — Panel + RoleParams variant

**Files:**
- Modify: `src/coupecad/core/io/json_project_serializer.h/.cpp` — добавить Panel/RoleParams
- Create: `tests/core/io/json_panel_test.cpp`
- Modify: `tests/core/io/CMakeLists.txt`

Это объёмная задача: RoleParams — `std::variant` из 11 альтернатив, каждую надо сериализовать с discriminator и специфичными полями.

- [ ] **Step 1: Добавить в `json_project_serializer.h` в `detail`**:

```cpp
nlohmann::json to_json_role_params(const class RoleParams& rp);
class RoleParams from_json_role_params(const nlohmann::json& j);

nlohmann::json to_json_panel(const struct Panel& p);
struct Panel from_json_panel(const nlohmann::json& j);

const char* panel_role_to_str(enum class PanelRole r);
enum class PanelRole panel_role_from_str(const std::string& s);

const char* grain_direction_to_str(enum class GrainDirection g);
enum class GrainDirection grain_direction_from_str(const std::string& s);

const char* hinge_side_to_str(enum class HingeSide h);
enum class HingeSide hinge_side_from_str(const std::string& s);
```

- [ ] **Step 2: В `json_project_serializer.cpp` в namespace detail добавить** (длинно, но механически — 11 вариантов роли):

```cpp
const char* panel_role_to_str(PanelRole r) { return panel_role_name(r); }

PanelRole panel_role_from_str(const std::string& s) {
    static const std::pair<const char*, PanelRole> table[] = {
        {"Top", PanelRole::Top}, {"Bottom", PanelRole::Bottom},
        {"SideLeft", PanelRole::SideLeft}, {"SideRight", PanelRole::SideRight},
        {"Back", PanelRole::Back}, {"Shelf", PanelRole::Shelf},
        {"DividerVertical", PanelRole::DividerVertical},
        {"DividerHorizontal", PanelRole::DividerHorizontal},
        {"Facade", PanelRole::Facade}, {"DrawerBottom", PanelRole::DrawerBottom},
        {"DrawerFront", PanelRole::DrawerFront},
        {"DrawerSide", PanelRole::DrawerSide},
        {"DrawerBack", PanelRole::DrawerBack},
        {"Plinth", PanelRole::Plinth}, {"Custom", PanelRole::Custom},
    };
    for (const auto& [n, r] : table) if (s == n) return r;
    throw InvalidData{"json.panel_role_unknown", "Unknown PanelRole: " + s};
}

const char* grain_direction_to_str(GrainDirection g) { return grain_direction_name(g); }

GrainDirection grain_direction_from_str(const std::string& s) {
    if (s == "Horizontal") return GrainDirection::Horizontal;
    if (s == "Vertical")   return GrainDirection::Vertical;
    if (s == "None")       return GrainDirection::None;
    throw InvalidData{"json.grain_unknown", "Unknown GrainDirection: " + s};
}

const char* hinge_side_to_str(HingeSide h) { return hinge_side_name(h); }

HingeSide hinge_side_from_str(const std::string& s) {
    if (s == "Left")   return HingeSide::Left;
    if (s == "Right")  return HingeSide::Right;
    if (s == "Top")    return HingeSide::Top;
    if (s == "Bottom") return HingeSide::Bottom;
    if (s == "None")   return HingeSide::None;
    throw InvalidData{"json.hinge_side_unknown", "Unknown HingeSide: " + s};
}

// RoleParams сериализуется как объект с kind-discriminator.
nlohmann::json to_json_role_params(const RoleParams& rp) {
    return std::visit([](const auto& p) -> nlohmann::json {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::is_same_v<T, NoRoleParams>) {
            return nlohmann::json{{"kind", "None"}};
        } else if constexpr (std::is_same_v<T, ShelfParams>) {
            nlohmann::json ext;
            if (std::holds_alternative<ShelfFullWidth>(p.extent)) {
                ext = nlohmann::json{{"kind", "FullWidth"}};
            } else {
                const auto& bd = std::get<ShelfBetweenDividers>(p.extent);
                ext = nlohmann::json{{"kind", "BetweenDividers"},
                                      {"from", to_json_id(bd.from)},
                                      {"to", to_json_id(bd.to)}};
            }
            return nlohmann::json{
                {"kind", "Shelf"},
                {"height_from_bottom_mm", p.height_from_bottom.value()},
                {"extent", ext},
            };
        } else if constexpr (std::is_same_v<T, DividerVerticalParams>) {
            nlohmann::json ext;
            if (std::holds_alternative<VerticalExtentFull>(p.height_extent)) {
                ext = nlohmann::json{{"kind", "Full"}};
            } else {
                const auto& r = std::get<VerticalExtentRange>(p.height_extent);
                ext = nlohmann::json{{"kind", "Range"},
                                      {"from_z_mm", r.from_z.value()},
                                      {"to_z_mm", r.to_z.value()}};
            }
            return nlohmann::json{
                {"kind", "DividerVertical"},
                {"offset_from_left_mm", p.offset_from_left.value()},
                {"height_extent", ext},
            };
        } else if constexpr (std::is_same_v<T, DividerHorizontalParams>) {
            nlohmann::json ext;
            if (std::holds_alternative<DepthExtentFull>(p.depth_extent)) {
                ext = nlohmann::json{{"kind", "Full"}};
            } else {
                const auto& r = std::get<DepthExtentRange>(p.depth_extent);
                ext = nlohmann::json{{"kind", "Range"},
                                      {"from_y_mm", r.from_y.value()},
                                      {"to_y_mm", r.to_y.value()}};
            }
            return nlohmann::json{
                {"kind", "DividerHorizontal"},
                {"offset_from_bottom_mm", p.offset_from_bottom.value()},
                {"depth_extent", ext},
            };
        } else if constexpr (std::is_same_v<T, FacadeParams>) {
            nlohmann::json ext;
            if (std::holds_alternative<FacadeFullFront>(p.extent)) {
                ext = nlohmann::json{{"kind", "FullFront"}};
            } else {
                const auto& r = std::get<FacadeRect>(p.extent);
                ext = nlohmann::json{{"kind", "Rect"},
                                      {"from_x_mm", r.from_x.value()},
                                      {"to_x_mm", r.to_x.value()},
                                      {"from_z_mm", r.from_z.value()},
                                      {"to_z_mm", r.to_z.value()}};
            }
            return nlohmann::json{
                {"kind", "Facade"},
                {"extent", ext},
                {"hinge_side", hinge_side_to_str(p.hinge_side)},
            };
        } else if constexpr (std::is_same_v<T, DrawerBottomParams>) {
            return nlohmann::json{{"kind", "DrawerBottom"},
                                   {"height_from_bottom_mm", p.height_from_bottom.value()},
                                   {"depth_mm", p.depth.value()}};
        } else if constexpr (std::is_same_v<T, DrawerFrontParams>) {
            return nlohmann::json{{"kind", "DrawerFront"},
                                   {"height_from_bottom_mm", p.height_from_bottom.value()},
                                   {"height_mm", p.height.value()}};
        } else if constexpr (std::is_same_v<T, DrawerSideParams>) {
            return nlohmann::json{{"kind", "DrawerSide"},
                                   {"height_from_bottom_mm", p.height_from_bottom.value()},
                                   {"height_mm", p.height.value()},
                                   {"depth_mm", p.depth.value()},
                                   {"side", p.side == DrawerSideParams::Side::Left ? "Left" : "Right"}};
        } else if constexpr (std::is_same_v<T, DrawerBackParams>) {
            return nlohmann::json{{"kind", "DrawerBack"},
                                   {"height_from_bottom_mm", p.height_from_bottom.value()},
                                   {"height_mm", p.height.value()}};
        } else if constexpr (std::is_same_v<T, PlinthParams>) {
            return nlohmann::json{{"kind", "Plinth"},
                                   {"height_mm", p.height.value()},
                                   {"setback_mm", p.setback.value()}};
        } else if constexpr (std::is_same_v<T, CustomParams>) {
            return nlohmann::json{{"kind", "Custom"},
                                   {"position_mm", to_json_vec3(p.position)},
                                   {"size_mm", to_json_vec3(p.size)},
                                   {"orientation", to_json_quat(p.orientation)}};
        }
    }, rp);
}

RoleParams from_json_role_params(const nlohmann::json& j) {
    std::string kind = j.at("kind").get<std::string>();
    if (kind == "None")              return NoRoleParams{};
    if (kind == "Shelf") {
        ShelfParams sp;
        sp.height_from_bottom = from_json_millimeters(j.at("height_from_bottom_mm"));
        const auto& ext = j.at("extent");
        std::string ekind = ext.at("kind").get<std::string>();
        if (ekind == "FullWidth")       sp.extent = ShelfFullWidth{};
        else if (ekind == "BetweenDividers") {
            sp.extent = ShelfBetweenDividers{
                .from = from_json_id<PanelIdTag>(ext.at("from")),
                .to = from_json_id<PanelIdTag>(ext.at("to")),
            };
        } else throw InvalidData{"json.shelf_extent_unknown", ekind};
        return sp;
    }
    if (kind == "DividerVertical") {
        DividerVerticalParams dp;
        dp.offset_from_left = from_json_millimeters(j.at("offset_from_left_mm"));
        const auto& ext = j.at("height_extent");
        std::string ekind = ext.at("kind").get<std::string>();
        if (ekind == "Full") dp.height_extent = VerticalExtentFull{};
        else if (ekind == "Range") {
            dp.height_extent = VerticalExtentRange{
                .from_z = from_json_millimeters(ext.at("from_z_mm")),
                .to_z = from_json_millimeters(ext.at("to_z_mm"))};
        } else throw InvalidData{"json.vext_unknown", ekind};
        return dp;
    }
    if (kind == "DividerHorizontal") {
        DividerHorizontalParams dp;
        dp.offset_from_bottom = from_json_millimeters(j.at("offset_from_bottom_mm"));
        const auto& ext = j.at("depth_extent");
        std::string ekind = ext.at("kind").get<std::string>();
        if (ekind == "Full") dp.depth_extent = DepthExtentFull{};
        else if (ekind == "Range") {
            dp.depth_extent = DepthExtentRange{
                .from_y = from_json_millimeters(ext.at("from_y_mm")),
                .to_y = from_json_millimeters(ext.at("to_y_mm"))};
        } else throw InvalidData{"json.dext_unknown", ekind};
        return dp;
    }
    if (kind == "Facade") {
        FacadeParams fp;
        const auto& ext = j.at("extent");
        std::string ekind = ext.at("kind").get<std::string>();
        if (ekind == "FullFront") fp.extent = FacadeFullFront{};
        else if (ekind == "Rect") {
            fp.extent = FacadeRect{
                .from_x = from_json_millimeters(ext.at("from_x_mm")),
                .to_x = from_json_millimeters(ext.at("to_x_mm")),
                .from_z = from_json_millimeters(ext.at("from_z_mm")),
                .to_z = from_json_millimeters(ext.at("to_z_mm"))};
        } else throw InvalidData{"json.facade_extent_unknown", ekind};
        fp.hinge_side = hinge_side_from_str(j.at("hinge_side").get<std::string>());
        return fp;
    }
    if (kind == "DrawerBottom") {
        return DrawerBottomParams{
            .height_from_bottom = from_json_millimeters(j.at("height_from_bottom_mm")),
            .depth = from_json_millimeters(j.at("depth_mm"))};
    }
    if (kind == "DrawerFront") {
        return DrawerFrontParams{
            .height_from_bottom = from_json_millimeters(j.at("height_from_bottom_mm")),
            .height = from_json_millimeters(j.at("height_mm"))};
    }
    if (kind == "DrawerSide") {
        DrawerSideParams p;
        p.height_from_bottom = from_json_millimeters(j.at("height_from_bottom_mm"));
        p.height = from_json_millimeters(j.at("height_mm"));
        p.depth = from_json_millimeters(j.at("depth_mm"));
        std::string side = j.at("side").get<std::string>();
        if (side == "Left") p.side = DrawerSideParams::Side::Left;
        else if (side == "Right") p.side = DrawerSideParams::Side::Right;
        else throw InvalidData{"json.drawer_side_unknown", side};
        return p;
    }
    if (kind == "DrawerBack") {
        return DrawerBackParams{
            .height_from_bottom = from_json_millimeters(j.at("height_from_bottom_mm")),
            .height = from_json_millimeters(j.at("height_mm"))};
    }
    if (kind == "Plinth") {
        return PlinthParams{
            .height = from_json_millimeters(j.at("height_mm")),
            .setback = from_json_millimeters(j.at("setback_mm"))};
    }
    if (kind == "Custom") {
        return CustomParams{
            .position = from_json_vec3(j.at("position_mm")),
            .size = from_json_vec3(j.at("size_mm")),
            .orientation = from_json_quat(j.at("orientation"))};
    }
    throw InvalidData{"json.role_params_unknown_kind", "Unknown RoleParams kind: " + kind};
}

nlohmann::json to_json_panel(const Panel& p) {
    nlohmann::json j;
    j["id"] = to_json_id(p.id);
    j["role"] = panel_role_to_str(p.role);
    j["role_params"] = to_json_role_params(p.role_params);
    j["material_override"] = p.material_override ? to_json_id(*p.material_override) : nlohmann::json(nullptr);
    j["thickness_override_mm"] = p.thickness_override ? nlohmann::json(p.thickness_override->value())
                                                      : nlohmann::json(nullptr);
    j["edge_banding"] = to_json_panel_edge_banding(p.edge_banding);
    j["grain_direction"] = grain_direction_to_str(p.grain_direction);
    j["label"] = p.label ? nlohmann::json(*p.label) : nlohmann::json(nullptr);
    return j;
}

Panel from_json_panel(const nlohmann::json& j) {
    Panel p;
    p.id = from_json_id<PanelIdTag>(j.at("id"));
    p.role = panel_role_from_str(j.at("role").get<std::string>());
    p.role_params = from_json_role_params(j.at("role_params"));
    if (!j.at("material_override").is_null()) {
        p.material_override = from_json_id<MaterialIdTag>(j.at("material_override"));
    }
    if (!j.at("thickness_override_mm").is_null()) {
        p.thickness_override = from_json_millimeters(j.at("thickness_override_mm"));
    }
    p.edge_banding = from_json_panel_edge_banding(j.at("edge_banding"));
    p.grain_direction = grain_direction_from_str(j.at("grain_direction").get<std::string>());
    if (!j.at("label").is_null()) p.label = j.at("label").get<std::string>();
    return p;
}
```

- [ ] **Step 3: Создать `tests/core/io/json_panel_test.cpp`** — round-trip для каждой роли + вариантов extent'ов. Минимум 12 тестов:

```cpp
#include "coupecad/core/io/json_project_serializer.h"
#include "coupecad/core/panel.h"

#include <gtest/gtest.h>

using namespace coupecad::core;
using namespace coupecad::core::detail;

namespace {
Panel make_panel(PanelRole role, RoleParams rp) {
    auto gen = make_seeded_uuid_generator(111);
    Panel p;
    p.id = PanelId{gen->next()};
    p.role = role;
    p.role_params = std::move(rp);
    return p;
}

void round_trip(const Panel& p) {
    auto j = to_json_panel(p);
    auto back = from_json_panel(j);
    EXPECT_EQ(back, p);
}
}  // namespace

TEST(JsonPanel, TopBottomBackSides) {
    round_trip(make_panel(PanelRole::Top, NoRoleParams{}));
    round_trip(make_panel(PanelRole::Bottom, NoRoleParams{}));
    round_trip(make_panel(PanelRole::Back, NoRoleParams{}));
    round_trip(make_panel(PanelRole::SideLeft, NoRoleParams{}));
    round_trip(make_panel(PanelRole::SideRight, NoRoleParams{}));
}

TEST(JsonPanel, ShelfFullWidth) {
    round_trip(make_panel(PanelRole::Shelf,
                ShelfParams{.height_from_bottom = Millimeters{800},
                            .extent = ShelfFullWidth{}}));
}

TEST(JsonPanel, ShelfBetweenDividers) {
    auto gen = make_seeded_uuid_generator(1);
    round_trip(make_panel(PanelRole::Shelf,
                ShelfParams{.height_from_bottom = Millimeters{1000},
                            .extent = ShelfBetweenDividers{
                                .from = PanelId{gen->next()},
                                .to = PanelId{gen->next()}}}));
}

TEST(JsonPanel, DividerVerticalFull) {
    round_trip(make_panel(PanelRole::DividerVertical,
                DividerVerticalParams{.offset_from_left = Millimeters{600},
                                       .height_extent = VerticalExtentFull{}}));
}

TEST(JsonPanel, DividerVerticalRange) {
    round_trip(make_panel(PanelRole::DividerVertical,
                DividerVerticalParams{.offset_from_left = Millimeters{700},
                                       .height_extent = VerticalExtentRange{
                                           .from_z = Millimeters{200},
                                           .to_z = Millimeters{1800}}}));
}

TEST(JsonPanel, DividerHorizontalRange) {
    round_trip(make_panel(PanelRole::DividerHorizontal,
                DividerHorizontalParams{.offset_from_bottom = Millimeters{1200},
                                         .depth_extent = DepthExtentRange{
                                             .from_y = Millimeters{0},
                                             .to_y = Millimeters{500}}}));
}

TEST(JsonPanel, FacadeFullFront) {
    round_trip(make_panel(PanelRole::Facade,
                FacadeParams{.extent = FacadeFullFront{},
                              .hinge_side = HingeSide::Left}));
}

TEST(JsonPanel, FacadeRect) {
    round_trip(make_panel(PanelRole::Facade,
                FacadeParams{.extent = FacadeRect{
                                 .from_x = Millimeters{100},
                                 .to_x = Millimeters{700},
                                 .from_z = Millimeters{200},
                                 .to_z = Millimeters{1200}},
                              .hinge_side = HingeSide::Right}));
}

TEST(JsonPanel, DrawerBottomFrontSideBack) {
    round_trip(make_panel(PanelRole::DrawerBottom,
                DrawerBottomParams{.height_from_bottom = Millimeters{200},
                                    .depth = Millimeters{500}}));
    round_trip(make_panel(PanelRole::DrawerFront,
                DrawerFrontParams{.height_from_bottom = Millimeters{200},
                                   .height = Millimeters{180}}));
    round_trip(make_panel(PanelRole::DrawerSide,
                DrawerSideParams{.height_from_bottom = Millimeters{200},
                                  .height = Millimeters{180},
                                  .depth = Millimeters{500},
                                  .side = DrawerSideParams::Side::Right}));
    round_trip(make_panel(PanelRole::DrawerBack,
                DrawerBackParams{.height_from_bottom = Millimeters{200},
                                  .height = Millimeters{180}}));
}

TEST(JsonPanel, Plinth) {
    round_trip(make_panel(PanelRole::Plinth,
                PlinthParams{.height = Millimeters{100},
                              .setback = Millimeters{50}}));
}

TEST(JsonPanel, Custom) {
    round_trip(make_panel(PanelRole::Custom,
                CustomParams{.position = Vec3{Millimeters{10}, Millimeters{20}, Millimeters{30}},
                              .size = Vec3{Millimeters{100}, Millimeters{16}, Millimeters{200}},
                              .orientation = Quat{0.7071, 0.0, 0.7071, 0.0}}));
}

TEST(JsonPanel, UnknownRoleThrows) {
    EXPECT_THROW(panel_role_from_str("Nonsense"), InvalidData);
}

TEST(JsonPanel, UnknownRoleParamsKindThrows) {
    nlohmann::json j{{"kind", "BogusShape"}};
    EXPECT_THROW(from_json_role_params(j), InvalidData);
}
```

- [ ] **Step 4: tests CMakeLists update — add json_panel_test.cpp.**

- [ ] **Step 5: Build + test.**

```sh
cmake --build --preset default --target coupecad_io_test
ctest --preset default -R "JsonPanel" --output-on-failure
```

Ожидается: 14 тестов passed.

- [ ] **Step 6: Commit.**

```sh
git commit -am "feat(core/io): JSON round-trip for Panel + all RoleParams variants"
```

---

## Task 5: JsonProjectSerializer — Cabinet + Project (финальный serialize/deserialize)

**Files:**
- Modify: `src/coupecad/core/io/json_project_serializer.h/.cpp`
- Create: `tests/core/io/json_project_test.cpp`
- Modify: `tests/core/io/CMakeLists.txt`

- [ ] **Step 1: Добавить в `json_project_serializer.h` в namespace detail**:

```cpp
nlohmann::json to_json_cabinet(const struct Cabinet& c);
struct Cabinet from_json_cabinet(const nlohmann::json& j);

// Project сериализатор работает через публичный интерфейс; эти хелперы
// не экспонируются — см. impl.
```

- [ ] **Step 2: Дополнить `json_project_serializer.cpp`** — Cabinet и полный serialize/deserialize:

```cpp
namespace detail {

nlohmann::json to_json_cabinet(const Cabinet& c) {
    // Детерминированно: сортируем панели и hardware по id (to_string).
    std::vector<std::pair<std::string, const Panel*>> sorted_panels;
    sorted_panels.reserve(c.panels.size());
    for (const auto& [id, p] : c.panels) {
        sorted_panels.emplace_back(id.to_string(), &p);
    }
    std::sort(sorted_panels.begin(), sorted_panels.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    nlohmann::json panels = nlohmann::json::array();
    for (const auto& [_, p] : sorted_panels) panels.push_back(to_json_panel(*p));

    std::vector<std::pair<std::string, const HardwareItem*>> sorted_hw;
    sorted_hw.reserve(c.hardware.size());
    for (const auto& [id, h] : c.hardware) {
        sorted_hw.emplace_back(id.to_string(), &h);
    }
    std::sort(sorted_hw.begin(), sorted_hw.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    nlohmann::json hw = nlohmann::json::array();
    for (const auto& [_, h] : sorted_hw) hw.push_back(to_json_hardware_item(*h));

    return nlohmann::json{
        {"id", to_json_id(c.id)},
        {"name", c.name},
        {"dimensions_mm", to_json_dimensions(c.dimensions)},
        {"default_panel_material", to_json_id(c.default_panel_material)},
        {"default_panel_thickness_mm", c.default_panel_thickness.value()},
        {"default_back_thickness_mm", c.default_back_thickness.value()},
        {"panels", panels},
        {"hardware", hw},
    };
}

Cabinet from_json_cabinet(const nlohmann::json& j) {
    Cabinet c;
    c.id = from_json_id<CabinetIdTag>(j.at("id"));
    c.name = j.at("name").get<std::string>();
    c.dimensions = from_json_dimensions(j.at("dimensions_mm"));
    c.default_panel_material = from_json_id<MaterialIdTag>(j.at("default_panel_material"));
    c.default_panel_thickness = from_json_millimeters(j.at("default_panel_thickness_mm"));
    c.default_back_thickness = from_json_millimeters(j.at("default_back_thickness_mm"));
    for (const auto& pj : j.at("panels")) {
        Panel p = from_json_panel(pj);
        auto pid = p.id;
        c.panels.emplace(pid, std::move(p));
    }
    for (const auto& hj : j.at("hardware")) {
        HardwareItem h = from_json_hardware_item(hj);
        auto hid = h.id;
        c.hardware.emplace(hid, std::move(h));
    }
    return c;
}

}  // namespace detail
```

- [ ] **Step 3: Реализовать `JsonProjectSerializer::serialize/deserialize`** — заменить existing stubs:

```cpp
static constexpr int kCurrentSchemaVersion = 1;

std::vector<std::uint8_t> JsonProjectSerializer::serialize(const Project& project) const {
    using namespace detail;
    nlohmann::json root;
    root["schema_version"] = kCurrentSchemaVersion;
    root["meta"] = nlohmann::json{
        {"name", project.meta().name},
        {"description", project.meta().description},
    };
    root["cabinet"] = to_json_cabinet(project.cabinet());

    // Materials — отсортированный массив по id.
    std::vector<std::pair<std::string, const Material*>> sorted_mats;
    for (const auto& [id, m] : project.materials()) sorted_mats.emplace_back(id.to_string(), &m);
    std::sort(sorted_mats.begin(), sorted_mats.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    nlohmann::json mats = nlohmann::json::array();
    for (const auto& [_, m] : sorted_mats) mats.push_back(to_json_material(*m));
    root["materials"] = mats;

    // HardwareSpec — отсортированный по ref.
    std::vector<std::pair<std::string, const HardwareSpec*>> sorted_specs;
    for (const auto& [ref, s] : project.hardware_catalog()) sorted_specs.emplace_back(ref.value(), &s);
    std::sort(sorted_specs.begin(), sorted_specs.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    nlohmann::json specs = nlohmann::json::array();
    for (const auto& [_, s] : sorted_specs) specs.push_back(to_json_hardware_spec(*s));
    root["hardware_catalog"] = specs;

    std::string s = root.dump(2);   // отступ 2 пробела
    return std::vector<std::uint8_t>(s.begin(), s.end());
}

Project JsonProjectSerializer::deserialize(const std::vector<std::uint8_t>& bytes) const {
    using namespace detail;
    nlohmann::json root;
    try {
        root = nlohmann::json::parse(bytes.begin(), bytes.end());
    } catch (const nlohmann::json::parse_error& e) {
        throw InvalidData{"json.parse_failed", e.what()};
    }

    if (!root.is_object() || !root.contains("schema_version")) {
        throw InvalidData{"json.missing_schema_version",
                          "project.json missing schema_version"};
    }
    int sv = root.at("schema_version").get<int>();
    if (sv != kCurrentSchemaVersion) {
        throw UnsupportedVersion{"json.wrong_schema_after_migration",
                                  "Expected current schema after migration, got " + std::to_string(sv)};
    }

    Project p = Project::create_empty(root.at("meta").at("name").get<std::string>(),
                                        make_random_uuid_generator());
    p.mutable_meta().description = root.at("meta").at("description").get<std::string>();

    // Сначала materials, чтобы Cabinet.default_panel_material мог ссылаться.
    p.mutable_materials().clear();
    for (const auto& mj : root.at("materials")) {
        Material m = from_json_material(mj);
        p.mutable_materials().emplace(m.id, m);
    }

    p.mutable_hardware_catalog().clear();
    for (const auto& sj : root.at("hardware_catalog")) {
        HardwareSpec s = from_json_hardware_spec(sj);
        p.mutable_hardware_catalog().emplace(s.ref, s);
    }

    p.mutable_cabinet() = from_json_cabinet(root.at("cabinet"));

    p.validate();
    return p;
}
```

Добавить `#include <algorithm>` и `#include "coupecad/core/cabinet.h"`, `#include "coupecad/core/project.h"` в .cpp.

- [ ] **Step 4: Создать `tests/core/io/json_project_test.cpp`.**

```cpp
#include "coupecad/core/commands/cabinet_commands.h"
#include "coupecad/core/commands/hardware_commands.h"
#include "coupecad/core/commands/hardware_spec_commands.h"
#include "coupecad/core/commands/material_commands.h"
#include "coupecad/core/commands/panel_commands.h"
#include "coupecad/core/io/json_project_serializer.h"
#include "coupecad/core/project.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

namespace {
std::string bytes_to_string(const std::vector<std::uint8_t>& b) {
    return std::string(b.begin(), b.end());
}
}  // namespace

TEST(JsonProject, EmptyProjectRoundTrip) {
    auto p = Project::create_empty("Empty", make_seeded_uuid_generator(1));
    JsonProjectSerializer s;
    auto bytes = s.serialize(p);
    auto back = s.deserialize(bytes);
    EXPECT_EQ(back.meta().name, "Empty");
    EXPECT_EQ(back.cabinet().dimensions, p.cabinet().dimensions);
    EXPECT_EQ(back.materials().size(), p.materials().size());
}

TEST(JsonProject, NonEmptyProjectRoundTrip) {
    auto p = Project::create_empty("Full", make_seeded_uuid_generator(2));

    // добавим материал, панели, hardware
    AddMaterial am{Material{.name = "Oak", .kind = MaterialKind::SolidWood,
                              .default_thickness = Millimeters{20}}};
    am.apply(p);

    AddPanel ap1{PanelRole::Top, NoRoleParams{}};
    ap1.apply(p);
    AddPanel ap2{PanelRole::Shelf,
                  ShelfParams{.height_from_bottom = Millimeters{900},
                               .extent = ShelfFullWidth{}}};
    ap2.apply(p);

    AddHardwareSpec ahs{HardwareSpec{.ref = HardwareRef{"hinge.test"},
                                      .kind = HardwareKind::Hinge,
                                      .name = "Test",
                                      .bbox = Vec3{Millimeters{35},
                                                    Millimeters{14},
                                                    Millimeters{60}}}};
    ahs.apply(p);

    AddHardware ah{HardwareRef{"hinge.test"},
                    {PanelAttachment{.panel_id = ap1.assigned_id(),
                                      .local_position = Vec3{Millimeters{10},
                                                              Millimeters{20},
                                                              Millimeters{30}},
                                      .orientation = Quat::identity()}}};
    ah.apply(p);

    JsonProjectSerializer s;
    auto bytes = s.serialize(p);
    auto back = s.deserialize(bytes);

    EXPECT_EQ(back.meta().name, "Full");
    EXPECT_EQ(back.materials().size(), p.materials().size());
    EXPECT_EQ(back.cabinet().panels.size(), p.cabinet().panels.size());
    EXPECT_EQ(back.cabinet().hardware.size(), p.cabinet().hardware.size());
    EXPECT_EQ(back.hardware_catalog().size(), p.hardware_catalog().size());

    // Повторная сериализация должна дать тот же JSON (детерминизм).
    auto bytes2 = s.serialize(back);
    EXPECT_EQ(bytes_to_string(bytes), bytes_to_string(bytes2));
}

TEST(JsonProject, MissingSchemaVersionThrows) {
    JsonProjectSerializer s;
    std::string bad = R"({"meta":{}})";
    std::vector<std::uint8_t> bytes(bad.begin(), bad.end());
    EXPECT_THROW(s.deserialize(bytes), InvalidData);
}

TEST(JsonProject, MalformedJsonThrows) {
    JsonProjectSerializer s;
    std::string bad = "{not valid json";
    std::vector<std::uint8_t> bytes(bad.begin(), bad.end());
    EXPECT_THROW(s.deserialize(bytes), InvalidData);
}

TEST(JsonProject, WrongSchemaVersionThrows) {
    JsonProjectSerializer s;
    // Искусственно подменим version в валидном проекте.
    auto p = Project::create_empty("X", make_seeded_uuid_generator(3));
    auto bytes = s.serialize(p);
    std::string text(bytes.begin(), bytes.end());
    auto pos = text.find("\"schema_version\": 1");
    ASSERT_NE(pos, std::string::npos);
    text.replace(pos, std::string("\"schema_version\": 1").size(),
                  "\"schema_version\": 99");
    std::vector<std::uint8_t> corrupted(text.begin(), text.end());
    EXPECT_THROW(s.deserialize(corrupted), UnsupportedVersion);
}
```

- [ ] **Step 5: tests CMakeLists update.**

- [ ] **Step 6: Build + test.**

```sh
cmake --build --preset default --target coupecad_io_test
ctest --preset default -R "JsonProject" --output-on-failure
```

Ожидается: 5 тестов passed.

- [ ] **Step 7: Commit.**

```sh
git commit -am "feat(core/io): JsonProjectSerializer serialize/deserialize with deterministic output"
```

---

## Task 6: SchemaMigration — interface + synthetic v0→v1 test

**Files:**
- Create: `src/coupecad/core/io/schema_migration.h`
- Create: `src/coupecad/core/io/schema_migration.cpp`
- Modify: `src/coupecad/core/CMakeLists.txt`
- Create: `tests/core/io/schema_migration_test.cpp`
- Modify: `tests/core/io/CMakeLists.txt`

- [ ] **Step 1: Создать `schema_migration.h`.**

```cpp
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
```

- [ ] **Step 2: Создать `schema_migration.cpp`.**

```cpp
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
```

- [ ] **Step 3: Обновить `src/coupecad/core/CMakeLists.txt`** — добавить `io/schema_migration.cpp`:

```cmake
add_library(coupecad_core STATIC
    # ... предыдущие ...
    io/json_project_serializer.cpp
    io/schema_migration.cpp
)
```

- [ ] **Step 4: Создать `tests/core/io/schema_migration_test.cpp`.**

```cpp
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
```

- [ ] **Step 5: tests CMakeLists update.**

- [ ] **Step 6: Build + test.**

```sh
cmake --build --preset default --target coupecad_io_test
ctest --preset default -R "SchemaMigration" --output-on-failure
```

Ожидается: 6 тестов passed.

- [ ] **Step 7: Commit.**

```sh
git commit -am "feat(core/io): SchemaMigration interface + MigrationChain with synthetic v0->v1 test"
```

---

## Task 7: CcadArchive — ZIP + meta.json + save/load orchestration

**Files:**
- Create: `src/coupecad/core/io/ccad_archive.h`
- Create: `src/coupecad/core/io/ccad_archive.cpp`
- Modify: `src/coupecad/core/CMakeLists.txt` (+ libzip PRIVATE link, + ccad_archive.cpp source)
- Create: `tests/core/io/ccad_archive_test.cpp`
- Modify: `tests/core/io/CMakeLists.txt`

- [ ] **Step 1: Создать `ccad_archive.h`.**

```cpp
#pragma once

#include "coupecad/core/io/project_serializer.h"
#include "coupecad/core/io/schema_migration.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace coupecad::core {

class Project;

// Клок для тестов — позволяет подменить «сейчас» на детерминированное
// значение. В production — std::chrono::system_clock::now().
class IClock {
public:
    virtual ~IClock() = default;
    virtual std::chrono::system_clock::time_point now() const = 0;
};

std::unique_ptr<IClock> make_system_clock();

// Чтение и запись `.ccad` файлов.
class CcadArchive {
public:
    // Записать Project в указанный путь. encoding выбирается
    // serializer.content_encoding() — влияет на имя файла данных
    // внутри ZIP (`project.json` для "json"). Перезаписывает если файл уже есть.
    static void save(const Project& project,
                      const std::filesystem::path& path,
                      const ProjectSerializer& serializer,
                      const IClock& clock = *make_system_clock());

    // Прочитать Project из пути. Бросает FileFormatError subclasses при
    // ошибках формата. Migration chain прогоняется перед вызовом
    // serializer.deserialize.
    static Project load(const std::filesystem::path& path,
                         const ProjectSerializer& serializer,
                         const MigrationChain& chain = default_migration_chain());

    // Константа имени data-файла внутри архива для data encoding.
    // В v1 content_encoding всегда "json", так что имя — "project.json".
    static std::string data_file_name(const std::string& encoding);
};

}  // namespace coupecad::core
```

- [ ] **Step 2: Создать `ccad_archive.cpp`.**

```cpp
#include "coupecad/core/io/ccad_archive.h"

#include "coupecad/core/errors.h"
#include "coupecad/core/project.h"

#include <nlohmann/json.hpp>
#include <zip.h>

#include <chrono>
#include <cstring>
#include <ctime>
#include <sstream>

namespace coupecad::core {

namespace {

class SystemClock : public IClock {
public:
    std::chrono::system_clock::time_point now() const override {
        return std::chrono::system_clock::now();
    }
};

std::string iso8601_utc(std::chrono::system_clock::time_point tp) {
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm_buf{};
#ifdef _WIN32
    gmtime_s(&tm_buf, &t);
#else
    gmtime_r(&t, &tm_buf);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    return buf;
}

std::vector<std::uint8_t> read_entry(zip_t* z, const char* name) {
    struct zip_stat st;
    if (zip_stat(z, name, 0, &st) < 0) {
        throw MissingManifest{"ccad.missing_entry",
                              std::string{"Archive missing entry: "} + name};
    }
    zip_file_t* f = zip_fopen(z, name, 0);
    if (!f) {
        throw CorruptedArchive{"ccad.entry_open_failed",
                                std::string{"Cannot open entry: "} + name};
    }
    std::vector<std::uint8_t> out(static_cast<std::size_t>(st.size));
    zip_int64_t got = zip_fread(f, out.data(), out.size());
    zip_fclose(f);
    if (got != static_cast<zip_int64_t>(out.size())) {
        throw CorruptedArchive{"ccad.short_read", name};
    }
    return out;
}

void add_entry(zip_t* z, const char* name,
                const std::vector<std::uint8_t>& bytes) {
    zip_source_t* src = zip_source_buffer(z, bytes.data(), bytes.size(), 0);
    if (!src) {
        throw CorruptedArchive{"ccad.add_source_failed", name};
    }
    if (zip_file_add(z, name, src, ZIP_FL_OVERWRITE) < 0) {
        zip_source_free(src);
        throw CorruptedArchive{"ccad.add_entry_failed", name};
    }
}

}  // namespace

std::unique_ptr<IClock> make_system_clock() {
    return std::make_unique<SystemClock>();
}

std::string CcadArchive::data_file_name(const std::string& encoding) {
    if (encoding == "json") return "project.json";
    throw UnsupportedEncoding{"ccad.unknown_encoding",
                               "Unknown content_encoding: " + encoding};
}

void CcadArchive::save(const Project& project,
                        const std::filesystem::path& path,
                        const ProjectSerializer& serializer,
                        const IClock& clock) {
    auto payload = serializer.serialize(project);
    std::string encoding = serializer.content_encoding();
    std::string data_name = data_file_name(encoding);

    nlohmann::json meta{
        {"schema_version", 1},
        {"content_encoding", encoding},
        {"app_version", "0.1.0"},
        {"created_at", iso8601_utc(clock.now())},
        {"modified_at", iso8601_utc(clock.now())},
    };
    std::string meta_str = meta.dump(2);
    std::vector<std::uint8_t> meta_bytes(meta_str.begin(), meta_str.end());

    int err = 0;
    if (std::filesystem::exists(path)) {
        std::filesystem::remove(path);
    }
    zip_t* z = zip_open(path.string().c_str(), ZIP_CREATE | ZIP_EXCL, &err);
    if (!z) {
        throw CorruptedArchive{"ccad.open_for_write_failed",
                                "Cannot open archive for writing: " + path.string()};
    }
    try {
        add_entry(z, "meta.json", meta_bytes);
        add_entry(z, data_name.c_str(), payload);
    } catch (...) {
        zip_discard(z);
        throw;
    }
    if (zip_close(z) < 0) {
        throw CorruptedArchive{"ccad.close_failed",
                                "zip_close failed for: " + path.string()};
    }
}

Project CcadArchive::load(const std::filesystem::path& path,
                            const ProjectSerializer& serializer,
                            const MigrationChain& chain) {
    if (!std::filesystem::exists(path)) {
        throw CorruptedArchive{"ccad.file_not_found", path.string()};
    }
    int err = 0;
    zip_t* z = zip_open(path.string().c_str(), ZIP_RDONLY, &err);
    if (!z) {
        throw CorruptedArchive{"ccad.open_failed", path.string()};
    }
    try {
        auto meta_bytes = read_entry(z, "meta.json");
        nlohmann::json meta;
        try {
            meta = nlohmann::json::parse(meta_bytes.begin(), meta_bytes.end());
        } catch (const nlohmann::json::parse_error& e) {
            throw InvalidData{"ccad.meta_invalid_json", e.what()};
        }
        if (!meta.contains("schema_version") || !meta.contains("content_encoding")) {
            throw MissingManifest{"ccad.meta_fields_missing",
                                   "meta.json missing required fields"};
        }
        std::string encoding = meta.at("content_encoding").get<std::string>();
        if (encoding != serializer.content_encoding()) {
            throw UnsupportedEncoding{"ccad.encoding_mismatch",
                                        "Archive encoding '" + encoding +
                                        "' does not match serializer '" +
                                        serializer.content_encoding() + "'"};
        }
        std::string data_name = data_file_name(encoding);
        auto payload = read_entry(z, data_name.c_str());

        // Парсим payload как JSON, прогоняем миграции, затем передаём
        // serializer'у как bytes после migration.
        nlohmann::json root;
        try {
            root = nlohmann::json::parse(payload.begin(), payload.end());
        } catch (const nlohmann::json::parse_error& e) {
            throw InvalidData{"ccad.payload_parse_failed", e.what()};
        }
        root = chain.run(std::move(root), 1);
        std::string migrated = root.dump(2);
        std::vector<std::uint8_t> migrated_bytes(migrated.begin(), migrated.end());

        zip_close(z);
        return serializer.deserialize(migrated_bytes);
    } catch (...) {
        zip_close(z);
        throw;
    }
}

}  // namespace coupecad::core
```

- [ ] **Step 3: Обновить `src/coupecad/core/CMakeLists.txt`** — добавить `libzip` как PRIVATE и `ccad_archive.cpp` в sources:

```cmake
find_package(stduuid REQUIRED)
find_package(fmt REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(libzip REQUIRED)

add_library(coupecad_core STATIC
    id.cpp
    material.cpp
    hardware.cpp
    panel.cpp
    cabinet.cpp
    project.cpp
    geometry.cpp
    commands/change_set.cpp
    commands/cabinet_commands.cpp
    commands/panel_commands.cpp
    commands/material_commands.cpp
    commands/hardware_commands.cpp
    commands/hardware_spec_commands.cpp
    commands/macro_command.cpp
    undo_stack.cpp
    io/json_project_serializer.cpp
    io/schema_migration.cpp
    io/ccad_archive.cpp
)

target_include_directories(coupecad_core
    PUBLIC ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(coupecad_core
    PUBLIC
        coupecad_logging
        fmt::fmt
        stduuid::stduuid
        nlohmann_json::nlohmann_json
    PRIVATE
        libzip::zip
)

target_compile_features(coupecad_core PUBLIC cxx_std_20)
```

- [ ] **Step 4: Создать `tests/core/io/ccad_archive_test.cpp`.**

```cpp
#include "coupecad/core/commands/panel_commands.h"
#include "coupecad/core/errors.h"
#include "coupecad/core/io/ccad_archive.h"
#include "coupecad/core/io/json_project_serializer.h"
#include "coupecad/core/project.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

using namespace coupecad::core;

namespace {
class FixedClock : public IClock {
public:
    std::chrono::system_clock::time_point now() const override {
        // 2026-04-22T10:00:00Z (sec since epoch — произвольное фиксированное).
        return std::chrono::system_clock::from_time_t(1782360000);
    }
};

class CcadArchiveTest : public ::testing::Test {
protected:
    void SetUp() override {
        tmp_ = std::filesystem::temp_directory_path() /
               ("coupecad_test_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()));
        std::filesystem::create_directories(tmp_);
    }
    void TearDown() override { std::filesystem::remove_all(tmp_); }
    std::filesystem::path tmp_;
};
}  // namespace

TEST_F(CcadArchiveTest, EmptyProjectRoundTrip) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(1));
    JsonProjectSerializer s;
    auto path = tmp_ / "empty.ccad";
    FixedClock clock;
    CcadArchive::save(p, path, s, clock);
    ASSERT_TRUE(std::filesystem::exists(path));

    auto back = CcadArchive::load(path, s);
    EXPECT_EQ(back.meta().name, "T");
    EXPECT_EQ(back.cabinet().dimensions, p.cabinet().dimensions);
}

TEST_F(CcadArchiveTest, NonEmptyProjectRoundTrip) {
    auto p = Project::create_empty("Loaded", make_seeded_uuid_generator(2));
    AddPanel ap{PanelRole::Top, NoRoleParams{}};
    ap.apply(p);

    JsonProjectSerializer s;
    auto path = tmp_ / "full.ccad";
    FixedClock clock;
    CcadArchive::save(p, path, s, clock);
    auto back = CcadArchive::load(path, s);
    EXPECT_EQ(back.cabinet().panels.size(), 1u);
}

TEST_F(CcadArchiveTest, MissingFileThrows) {
    JsonProjectSerializer s;
    EXPECT_THROW(CcadArchive::load(tmp_ / "no_such.ccad", s),
                 CorruptedArchive);
}

TEST_F(CcadArchiveTest, NotAZipThrows) {
    auto path = tmp_ / "bad.ccad";
    {
        std::ofstream f(path, std::ios::binary);
        f << "not a zip";
    }
    JsonProjectSerializer s;
    EXPECT_THROW(CcadArchive::load(path, s), CorruptedArchive);
}

TEST_F(CcadArchiveTest, DataFileNameFromEncoding) {
    EXPECT_EQ(CcadArchive::data_file_name("json"), "project.json");
    EXPECT_THROW(CcadArchive::data_file_name("proprietary_binary"),
                 UnsupportedEncoding);
}
```

- [ ] **Step 5: tests CMakeLists update.**

- [ ] **Step 6: Build + test.**

```sh
cmake --preset default    # конфигурирует с новым find_package(libzip)
cmake --build --preset default --target coupecad_io_test
ctest --preset default -R "CcadArchive" --output-on-failure
```

Ожидается: 5 тестов passed.

- [ ] **Step 7: Commit.**

```sh
git commit -am "feat(core/io): CcadArchive save/load (ZIP + meta.json + migration pipeline)"
```

---

## Task 8: Golden JSON fixture + финальный прогон + push + CI

**Files:**
- Create: `tests/core/io/golden/v1_reference.json`
- Create: `tests/core/io/golden_test.cpp`
- Modify: `tests/core/io/CMakeLists.txt`

- [ ] **Step 1: Сгенерировать эталонный golden файл** — временным test binary'ом.

Сначала записать в репо «очевидно правильный» golden. Создать `tests/core/io/golden/v1_reference.json` вручную (запустить тест `JsonProject.EmptyProjectRoundTrip` с seed=42 для UUID'ов, скопировать serialize-выход в файл). Процедура:

```sh
# В корне проекта:
cat > /tmp/gen_golden.cpp <<'EOF'
#include "coupecad/core/io/json_project_serializer.h"
#include "coupecad/core/project.h"
#include <cstdio>
using namespace coupecad::core;
int main() {
    auto p = Project::create_empty("Golden", make_seeded_uuid_generator(42));
    JsonProjectSerializer s;
    auto bytes = s.serialize(p);
    std::fwrite(bytes.data(), 1, bytes.size(), stdout);
    return 0;
}
EOF
```

Если неохота городить временную программу — можно сохранить выход конкретного теста через одноразовый `std::ofstream` в самом тесте (Step 2 ниже генерирует golden из Project::create_empty("Golden", seed=42) и пишет в golden/ если GOLDEN_UPDATE=1 env выставлен).

**Вариант без временной программы:** создать golden файл через тест с update-режимом.

- [ ] **Step 2: Создать `tests/core/io/golden_test.cpp`.**

```cpp
#include "coupecad/core/io/json_project_serializer.h"
#include "coupecad/core/project.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace coupecad::core;

namespace {
std::filesystem::path golden_path() {
    // Путь относительный к source директории.
    return std::filesystem::path{COUPECAD_TESTS_DIR} / "core" / "io" / "golden" /
           "v1_reference.json";
}

std::string read_file(const std::filesystem::path& p) {
    std::ifstream f(p);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
}  // namespace

TEST(Golden, V1ReferenceMatchesEmptyProject) {
    auto p = Project::create_empty("Golden", make_seeded_uuid_generator(42));
    JsonProjectSerializer s;
    auto bytes = s.serialize(p);
    std::string actual(bytes.begin(), bytes.end());

    if (std::getenv("GOLDEN_UPDATE")) {
        std::filesystem::create_directories(golden_path().parent_path());
        std::ofstream out(golden_path());
        out << actual;
        SUCCEED() << "Updated golden at " << golden_path();
        return;
    }

    ASSERT_TRUE(std::filesystem::exists(golden_path()))
        << "Golden not found; run once with GOLDEN_UPDATE=1 to create: "
        << golden_path();
    std::string expected = read_file(golden_path());
    EXPECT_EQ(actual, expected)
        << "Serialized project doesn't match golden.\n"
        << "If the change is intentional, run this test with GOLDEN_UPDATE=1\n"
        << "and commit the updated " << golden_path();
}
```

- [ ] **Step 3: Обновить `tests/core/io/CMakeLists.txt`** — добавить golden_test.cpp и определить `COUPECAD_TESTS_DIR`:

```cmake
add_executable(coupecad_io_test
    serializer_interface_test.cpp
    json_primitives_test.cpp
    json_entities_test.cpp
    json_panel_test.cpp
    json_project_test.cpp
    schema_migration_test.cpp
    ccad_archive_test.cpp
    golden_test.cpp
)

target_compile_definitions(coupecad_io_test
    PRIVATE
        COUPECAD_TESTS_DIR="${CMAKE_SOURCE_DIR}/tests"
)

target_link_libraries(coupecad_io_test
    PRIVATE
        coupecad_core
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_io_test)
```

- [ ] **Step 4: Запустить тест в update-режиме для генерации golden.**

```sh
cmake --build --preset default --target coupecad_io_test
GOLDEN_UPDATE=1 ctest --preset default -R "Golden" --output-on-failure
```

Теперь `tests/core/io/golden/v1_reference.json` создан.

- [ ] **Step 5: Прогнать golden тест без UPDATE.**

```sh
ctest --preset default -R "Golden" --output-on-failure
```

Ожидается: 1 тест passed (bit-identical с тем что только что записали).

- [ ] **Step 6: Прогнать ВСЁ.**

```sh
ctest --preset default --output-on-failure
```

Ожидается: все тесты зелёные (162 из 1b + ~50 из 1c — итого ~210+).

- [ ] **Step 7: Commit + push.**

```sh
git add tests/core/io/
git commit -m "test(core/io): golden v1 reference + full regression coverage"
git push
```

- [ ] **Step 8: Дождаться CI зелёного.**

```sh
gh run list --limit 1 --workflow=CI
gh run watch
```

Ожидается: Linux + Windows green. macOS закомментирован как в Stage 0.

- [ ] **Step 9: Финальная проверка DoD.**

- [ ] Round-trip через `JsonProjectSerializer` для пустого и непустого проекта работает.
- [ ] `CcadArchive::save/load` пишет/читает ZIP с `meta.json` + `project.json`.
- [ ] Migration chain прогоняется, synthetic v0→v1 тест работает.
- [ ] Golden файл зафиксирован и проверяется в тесте.
- [ ] CI зелёный на Linux и Windows.

После этого **Stage 1 (Core domain) полностью завершён**. Следующий — Stage 2 (Geometry layer с OpenCASCADE), отдельный brainstorm + план.
