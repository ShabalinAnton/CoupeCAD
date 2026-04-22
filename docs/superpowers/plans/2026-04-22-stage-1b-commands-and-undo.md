# Stage 1b — Commands + UndoStack Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Реализовать командный слой доменной модели CoupeCAD: интерфейс `Command`, `PreviewableCommand`, `UndoStack` с дискретным и preview/commit режимами, все конкретные команды (Cabinet / Panel / Hardware / Material / HardwareSpec / MacroCommand), `IProjectObserver` callback invocation.

**Architecture:** Каждая команда хранит `before`/`after` дельту по UUID, не держит указателей на `Project`. `UndoStack` владеет `Project&`, стеками `undo_/redo_`, активным preview-сеансом (максимум один). Команды, поддерживающие интерактивную правку, наследуются от `PreviewableCommand` (добавляет `update()`).

**Tech Stack:** C++20. Новых Conan-зависимостей нет. Все тесты — GoogleTest. Qt по-прежнему не используется.

**Definition of Done.**
- Полный набор команд из §3.2 спеки реализован, каждая с exec/revert/ChangeSet.
- `UndoStack::execute/undo/redo` работает; observer получает ChangeSet после каждой операции.
- `begin_preview/update_preview/commit_preview/cancel_preview` работают; активный preview — только один в моменте.
- `begin_macro/end_macro` группирует дискретные команды в один `MacroCommand` на стеке.
- Валидация внутри `apply()` атомарна: при ошибке ни `Project`, ни стек не остаются в полу-состоянии.
- Покрытие `coupecad_core` ≥ 70%.
- CI зелёный (Linux + Windows).

---

## File Structure

После Stage 1b новые/изменённые файлы:

```
src/coupecad/core/
├── commands/                            +  (новый подкаталог)
│   ├── change_set.h                     +  (вынос ChangeSet из project.h)
│   ├── command.h                        +  (Command + PreviewableCommand + CommandKind)
│   ├── cabinet_commands.h/.cpp          +
│   ├── panel_commands.h/.cpp            +
│   ├── hardware_commands.h/.cpp         +
│   ├── material_commands.h/.cpp         +
│   ├── hardware_spec_commands.h/.cpp    +
│   └── macro_command.h/.cpp             +
├── undo_stack.h                         +
├── undo_stack.cpp                       +
├── project.h                            M  (ChangeSet извлекается в commands/change_set.h; reexport через project.h)
└── CMakeLists.txt                       M  (новые .cpp в списке)

tests/core/
├── commands/                            +
│   ├── CMakeLists.txt                   +
│   ├── cabinet_commands_test.cpp        +
│   ├── panel_commands_test.cpp          +
│   ├── hardware_commands_test.cpp       +
│   ├── material_commands_test.cpp       +
│   ├── hardware_spec_commands_test.cpp  +
│   └── macro_command_test.cpp           +
├── undo_stack_test.cpp                  +
├── undo_stack_preview_test.cpp          +
└── CMakeLists.txt                       M
```

**Ответственности:**
- `commands/command.h` — базовые интерфейсы. Все конкретные команды наследуются от `Command` (или `PreviewableCommand` для интерактивных).
- `commands/<entity>_commands.h/.cpp` — команды одной entity-группы в отдельных файлах (каждый файл 100-250 строк, удобно читать).
- `undo_stack.h/.cpp` — стек и управление preview/macro.
- `tests/core/commands/` — отдельные suite-файлы на entity-группу.

---

## Task 1: `change_set.h` вынос + `command.h` (интерфейс)

**Files:**
- Create: `src/coupecad/core/commands/change_set.h`
- Create: `src/coupecad/core/commands/command.h`
- Modify: `src/coupecad/core/project.h` (вынести ChangeSet; оставить re-export)
- Modify: `src/coupecad/core/CMakeLists.txt`
- Create: `tests/core/commands/CMakeLists.txt`
- Create: `tests/core/commands/command_interface_test.cpp`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1: Создать `src/coupecad/core/commands/change_set.h`** (перенос содержимого ChangeSet из project.h).

```cpp
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
```

- [ ] **Step 2: Создать `src/coupecad/core/commands/change_set.cpp`** — реализация `merge()`.

```cpp
#include "coupecad/core/commands/change_set.h"

namespace coupecad::core {

namespace {
template <class V>
void append(V& dst, const V& src) {
    dst.insert(dst.end(), src.begin(), src.end());
}
}  // namespace

void ChangeSet::merge(const ChangeSet& o) {
    append(added_panels, o.added_panels);
    append(removed_panels, o.removed_panels);
    append(updated_panels, o.updated_panels);
    append(added_hardware, o.added_hardware);
    append(removed_hardware, o.removed_hardware);
    append(updated_hardware, o.updated_hardware);
    append(added_materials, o.added_materials);
    append(removed_materials, o.removed_materials);
    append(updated_materials, o.updated_materials);
    if (o.cabinet_changed) cabinet_changed = true;
}

}  // namespace coupecad::core
```

- [ ] **Step 3: Создать `src/coupecad/core/commands/command.h`** — интерфейсы Command и PreviewableCommand.

```cpp
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
```

- [ ] **Step 4: Модифицировать `src/coupecad/core/project.h`** — убрать определение ChangeSet, сохранить re-include.

Заменить блок `struct ChangeSet { ... };` на единственную строку-инклюд (сохраняя публичное использование `ChangeSet` из project.h для обратной совместимости):

```cpp
#include "coupecad/core/commands/change_set.h"
```

(остальное содержимое project.h не трогать — IProjectObserver и класс Project остаются).

- [ ] **Step 5: Обновить `src/coupecad/core/CMakeLists.txt`** — добавить новый source.

```cmake
find_package(stduuid REQUIRED)
find_package(fmt REQUIRED)

add_library(coupecad_core STATIC
    id.cpp
    material.cpp
    hardware.cpp
    panel.cpp
    cabinet.cpp
    project.cpp
    geometry.cpp
    commands/change_set.cpp
)

target_include_directories(coupecad_core
    PUBLIC ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(coupecad_core
    PUBLIC
        coupecad_logging
        fmt::fmt
        stduuid::stduuid
)

target_compile_features(coupecad_core PUBLIC cxx_std_20)
```

- [ ] **Step 6: Создать `tests/core/commands/CMakeLists.txt`.**

```cmake
add_executable(coupecad_commands_test
    command_interface_test.cpp
)

target_link_libraries(coupecad_commands_test
    PRIVATE
        coupecad_core
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_commands_test)
```

- [ ] **Step 7: Создать `tests/core/commands/command_interface_test.cpp`** — проверка ChangeSet и наличия интерфейса.

```cpp
#include "coupecad/core/commands/change_set.h"
#include "coupecad/core/commands/command.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

TEST(ChangeSet, DefaultIsEmpty) {
    ChangeSet cs;
    EXPECT_TRUE(cs.empty());
}

TEST(ChangeSet, AnyFieldMakesNonEmpty) {
    {
        ChangeSet cs;
        cs.added_panels.push_back(PanelId{});
        EXPECT_FALSE(cs.empty());
    }
    {
        ChangeSet cs;
        cs.cabinet_changed = true;
        EXPECT_FALSE(cs.empty());
    }
    {
        ChangeSet cs;
        cs.updated_materials.push_back(MaterialId{});
        EXPECT_FALSE(cs.empty());
    }
}

TEST(ChangeSet, MergeConcatenates) {
    ChangeSet a;
    a.added_panels.push_back(PanelId{});
    ChangeSet b;
    b.added_panels.push_back(PanelId{});
    b.removed_hardware.push_back(HardwareItemId{});
    a.merge(b);
    EXPECT_EQ(a.added_panels.size(), 2u);
    EXPECT_EQ(a.removed_hardware.size(), 1u);
}

TEST(ChangeSet, MergePropagatesCabinetChanged) {
    ChangeSet a;
    ChangeSet b;
    b.cabinet_changed = true;
    a.merge(b);
    EXPECT_TRUE(a.cabinet_changed);
}

TEST(CommandKind, KindEnumCompiles) {
    // Просто фиксируем, что enum виден и имеет ожидаемые значения.
    EXPECT_NE(static_cast<int>(CommandKind::AddPanel),
              static_cast<int>(CommandKind::RemovePanel));
    EXPECT_NE(static_cast<int>(CommandKind::AddMaterial),
              static_cast<int>(CommandKind::Macro));
}
```

- [ ] **Step 8: Обновить `tests/core/CMakeLists.txt`** — добавить `add_subdirectory(commands)`.

```cmake
add_executable(coupecad_core_test
    units_test.cpp
    id_test.cpp
    material_test.cpp
    hardware_test.cpp
    panel_test.cpp
    cabinet_test.cpp
    project_test.cpp
    geometry_test.cpp
)

target_link_libraries(coupecad_core_test
    PRIVATE
        coupecad_core
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_core_test)

add_subdirectory(commands)
```

- [ ] **Step 9: Собрать и прогнать тесты.**

```sh
export Qt6_DIR=$HOME/Qt/6.7.3/macos/lib/cmake/Qt6
cmake --preset default
cmake --build --preset default
ctest --preset default -R "ChangeSet|CommandKind" --output-on-failure
```

Ожидается: 5 тестов passed. Plus все предыдущие 97 должны работать (project.h re-include сохраняет совместимость).

- [ ] **Step 10: Закоммитить.**

```sh
git add src/coupecad/core/ tests/core/
git commit -m "feat(core): Command/PreviewableCommand interfaces + ChangeSet in commands/"
```

---

## Task 2: Cabinet commands (SetCabinetDimensions, SetCabinetDefaults)

**Files:**
- Create: `src/coupecad/core/commands/cabinet_commands.h`
- Create: `src/coupecad/core/commands/cabinet_commands.cpp`
- Modify: `src/coupecad/core/CMakeLists.txt`
- Create: `tests/core/commands/cabinet_commands_test.cpp`
- Modify: `tests/core/commands/CMakeLists.txt`

- [ ] **Step 1: Создать `src/coupecad/core/commands/cabinet_commands.h`.**

```cpp
#pragma once

#include "coupecad/core/commands/command.h"
#include "coupecad/core/id.h"
#include "coupecad/core/units.h"

namespace coupecad::core {

// Изменить габариты шкафа. Preview-совместимая.
class SetCabinetDimensions : public PreviewableCommand {
public:
    SetCabinetDimensions(CabinetId target, Dimensions new_value);

    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    ChangeSet update(Project& project, const std::any& new_value) override;
    std::string_view label() const noexcept override { return "Set cabinet dimensions"; }
    CommandKind kind() const noexcept override { return CommandKind::CabinetDimensions; }

private:
    CabinetId target_;
    Dimensions new_value_;
    Dimensions old_value_{};
    bool applied_ = false;
};

// Изменить дефолты корпуса: материал, толщину панелей/спинки.
class SetCabinetDefaults : public Command {
public:
    SetCabinetDefaults(CabinetId target,
                       MaterialId panel_material,
                       Millimeters panel_thickness,
                       Millimeters back_thickness);

    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return "Set cabinet defaults"; }
    CommandKind kind() const noexcept override { return CommandKind::CabinetDefaults; }

private:
    CabinetId target_;
    MaterialId new_material_;
    Millimeters new_panel_thickness_;
    Millimeters new_back_thickness_;
    MaterialId old_material_{};
    Millimeters old_panel_thickness_{};
    Millimeters old_back_thickness_{};
    bool applied_ = false;
};

}  // namespace coupecad::core
```

- [ ] **Step 2: Создать `src/coupecad/core/commands/cabinet_commands.cpp`.**

```cpp
#include "coupecad/core/commands/cabinet_commands.h"

#include "coupecad/core/errors.h"
#include "coupecad/core/project.h"

namespace coupecad::core {

namespace {

void validate_dimensions(const Dimensions& d) {
    if (d.width.value() <= 0 || d.depth.value() <= 0 ||
        d.height.value() <= 0) {
        throw DomainError{"cabinet.nonpositive_dimensions",
                          "Cabinet dimensions must all be > 0"};
    }
}

}  // namespace

SetCabinetDimensions::SetCabinetDimensions(CabinetId target, Dimensions new_value)
    : target_(target), new_value_(new_value) {
    validate_dimensions(new_value);
}

ChangeSet SetCabinetDimensions::apply(Project& project) {
    auto& c = project.mutable_cabinet();
    if (c.id != target_) {
        throw DomainError{"cabinet.wrong_id", "SetCabinetDimensions target mismatch"};
    }
    if (!applied_) old_value_ = c.dimensions;
    c.dimensions = new_value_;
    applied_ = true;
    ChangeSet cs;
    cs.cabinet_changed = true;
    return cs;
}

ChangeSet SetCabinetDimensions::revert(Project& project) {
    auto& c = project.mutable_cabinet();
    if (c.id != target_) {
        throw DomainError{"cabinet.wrong_id", "SetCabinetDimensions revert target mismatch"};
    }
    c.dimensions = old_value_;
    ChangeSet cs;
    cs.cabinet_changed = true;
    return cs;
}

ChangeSet SetCabinetDimensions::update(Project& project, const std::any& new_value) {
    auto* casted = std::any_cast<Dimensions>(&new_value);
    if (!casted) {
        throw LogicError{"command.preview.type_mismatch",
                         "SetCabinetDimensions::update expects Dimensions"};
    }
    validate_dimensions(*casted);
    auto& c = project.mutable_cabinet();
    if (c.id != target_) {
        throw DomainError{"cabinet.wrong_id", "SetCabinetDimensions update target mismatch"};
    }
    new_value_ = *casted;
    c.dimensions = *casted;
    ChangeSet cs;
    cs.cabinet_changed = true;
    return cs;
}

SetCabinetDefaults::SetCabinetDefaults(CabinetId target,
                                       MaterialId panel_material,
                                       Millimeters panel_thickness,
                                       Millimeters back_thickness)
    : target_(target),
      new_material_(panel_material),
      new_panel_thickness_(panel_thickness),
      new_back_thickness_(back_thickness) {
    if (!panel_material.is_valid()) {
        throw DomainError{"cabinet.invalid_default_material",
                          "panel_material is invalid"};
    }
    if (panel_thickness.value() <= 0 || back_thickness.value() <= 0) {
        throw DomainError{"cabinet.nonpositive_thickness",
                          "Thicknesses must be > 0"};
    }
}

ChangeSet SetCabinetDefaults::apply(Project& project) {
    auto& c = project.mutable_cabinet();
    if (c.id != target_) {
        throw DomainError{"cabinet.wrong_id", "SetCabinetDefaults target mismatch"};
    }
    // Проверка, что материал существует в проекте.
    if (project.materials().find(new_material_) == project.materials().end()) {
        throw DomainError{"project.default_material_missing",
                          "panel_material not in project materials"};
    }
    if (!applied_) {
        old_material_ = c.default_panel_material;
        old_panel_thickness_ = c.default_panel_thickness;
        old_back_thickness_ = c.default_back_thickness;
    }
    c.default_panel_material = new_material_;
    c.default_panel_thickness = new_panel_thickness_;
    c.default_back_thickness = new_back_thickness_;
    applied_ = true;
    ChangeSet cs;
    cs.cabinet_changed = true;
    return cs;
}

ChangeSet SetCabinetDefaults::revert(Project& project) {
    auto& c = project.mutable_cabinet();
    c.default_panel_material = old_material_;
    c.default_panel_thickness = old_panel_thickness_;
    c.default_back_thickness = old_back_thickness_;
    ChangeSet cs;
    cs.cabinet_changed = true;
    return cs;
}

}  // namespace coupecad::core
```

- [ ] **Step 3: Обновить `src/coupecad/core/CMakeLists.txt`** — добавить source.

Вставить `commands/cabinet_commands.cpp` в список:

```cmake
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
)
```

- [ ] **Step 4: Создать `tests/core/commands/cabinet_commands_test.cpp`.**

```cpp
#include "coupecad/core/commands/cabinet_commands.h"
#include "coupecad/core/errors.h"
#include "coupecad/core/project.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

namespace {
Project make_project() {
    return Project::create_empty("T", make_seeded_uuid_generator(1));
}
}  // namespace

TEST(SetCabinetDimensions, ApplyAndRevert) {
    auto p = make_project();
    Dimensions original = p.cabinet().dimensions;

    SetCabinetDimensions cmd{p.cabinet().id,
                              Dimensions{Millimeters{3000}, Millimeters{700},
                                         Millimeters{2500}}};
    auto cs = cmd.apply(p);
    EXPECT_TRUE(cs.cabinet_changed);
    EXPECT_EQ(p.cabinet().dimensions.width, Millimeters{3000});

    cmd.revert(p);
    EXPECT_EQ(p.cabinet().dimensions, original);
}

TEST(SetCabinetDimensions, RejectsNonPositive) {
    EXPECT_THROW(SetCabinetDimensions(CabinetId{},
                                       Dimensions{Millimeters{0},
                                                  Millimeters{600},
                                                  Millimeters{2400}}),
                 DomainError);
}

TEST(SetCabinetDimensions, WrongTargetThrowsOnApply) {
    auto p = make_project();
    SetCabinetDimensions cmd{CabinetId{p.uuid_gen().next()},
                              Dimensions{Millimeters{1000}, Millimeters{500},
                                         Millimeters{1000}}};
    EXPECT_THROW(cmd.apply(p), DomainError);
}

TEST(SetCabinetDimensions, PreviewUpdateChangesLiveProject) {
    auto p = make_project();
    SetCabinetDimensions cmd{p.cabinet().id,
                              Dimensions{Millimeters{2500}, Millimeters{650},
                                         Millimeters{2400}}};
    cmd.apply(p);   // устанавливает initial снимок
    cmd.update(p,
               std::any{Dimensions{Millimeters{2800}, Millimeters{700},
                                    Millimeters{2500}}});
    EXPECT_EQ(p.cabinet().dimensions.width, Millimeters{2800});
    cmd.revert(p);
    EXPECT_EQ(p.cabinet().dimensions.width, Millimeters{2400});   // исходные 2400
}

TEST(SetCabinetDimensions, PreviewUpdateRejectsWrongType) {
    auto p = make_project();
    SetCabinetDimensions cmd{p.cabinet().id,
                              Dimensions{Millimeters{2500}, Millimeters{650},
                                         Millimeters{2400}}};
    cmd.apply(p);
    EXPECT_THROW(cmd.update(p, std::any{42}), LogicError);
}

TEST(SetCabinetDefaults, ApplyAndRevert) {
    auto p = make_project();
    auto old_material = p.cabinet().default_panel_material;
    auto new_mat_id = p.uuid_gen().next_id<MaterialIdTag>();

    // Добавим материал вручную (Stage 1a-style), затем через команду переключим
    // default.
    Material mat{.id = new_mat_id,
                  .name = "MDF 18mm",
                  .kind = MaterialKind::Mdf,
                  .default_thickness = Millimeters{18}};
    p.mutable_materials()[new_mat_id] = mat;

    SetCabinetDefaults cmd{p.cabinet().id, new_mat_id, Millimeters{18},
                            Millimeters{6}};
    cmd.apply(p);
    EXPECT_EQ(p.cabinet().default_panel_material, new_mat_id);
    EXPECT_EQ(p.cabinet().default_panel_thickness, Millimeters{18});
    EXPECT_EQ(p.cabinet().default_back_thickness, Millimeters{6});

    cmd.revert(p);
    EXPECT_EQ(p.cabinet().default_panel_material, old_material);
    EXPECT_EQ(p.cabinet().default_panel_thickness, Millimeters{16});
    EXPECT_EQ(p.cabinet().default_back_thickness, Millimeters{4});
}

TEST(SetCabinetDefaults, UnknownMaterialThrowsOnApply) {
    auto p = make_project();
    auto unknown = p.uuid_gen().next_id<MaterialIdTag>();
    SetCabinetDefaults cmd{p.cabinet().id, unknown, Millimeters{16},
                            Millimeters{4}};
    EXPECT_THROW(cmd.apply(p), DomainError);
}
```

- [ ] **Step 5: Обновить `tests/core/commands/CMakeLists.txt`.**

```cmake
add_executable(coupecad_commands_test
    command_interface_test.cpp
    cabinet_commands_test.cpp
)

target_link_libraries(coupecad_commands_test
    PRIVATE
        coupecad_core
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_commands_test)
```

- [ ] **Step 6: Собрать и прогнать.**

```sh
cmake --build --preset default --target coupecad_commands_test
ctest --preset default -R "SetCabinet" --output-on-failure
```

Ожидается: 7 тестов passed.

- [ ] **Step 7: Закоммитить.**

```sh
git add src/coupecad/core/commands/cabinet_commands.* \
        src/coupecad/core/CMakeLists.txt \
        tests/core/commands/cabinet_commands_test.cpp \
        tests/core/commands/CMakeLists.txt
git commit -m "feat(core/commands): SetCabinetDimensions + SetCabinetDefaults"
```

---

## Task 3: Panel Add/Remove commands

**Files:**
- Create: `src/coupecad/core/commands/panel_commands.h`
- Create: `src/coupecad/core/commands/panel_commands.cpp`
- Modify: `src/coupecad/core/CMakeLists.txt`
- Create: `tests/core/commands/panel_commands_test.cpp`
- Modify: `tests/core/commands/CMakeLists.txt`

Task 3 и Task 4 делят один файл `panel_commands.h/.cpp` — начинаем с Add/Remove, в Task 4 дополняем Update-командами. В этом Task — минимальный скелет файла + две команды.

- [ ] **Step 1: Создать `src/coupecad/core/commands/panel_commands.h`** (первая версия, содержит только Add/Remove; расширится в Task 4).

```cpp
#pragma once

#include "coupecad/core/commands/command.h"
#include "coupecad/core/id.h"
#include "coupecad/core/panel.h"

#include <optional>

namespace coupecad::core {

// Добавить новую панель. Конструктор НЕ назначает id — вместо этого
// id выдаётся в apply() (или пере-используется из предыдущего apply
// при redo после undo, чтобы внешние ссылки не ломались).
class AddPanel : public Command {
public:
    AddPanel(PanelRole role, RoleParams role_params);

    // Доступно только после первого apply().
    PanelId assigned_id() const noexcept { return assigned_id_; }

    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return "Add panel"; }
    CommandKind kind() const noexcept override { return CommandKind::AddPanel; }

private:
    PanelRole role_;
    RoleParams role_params_;
    PanelId assigned_id_{};   // назначается в apply
    bool applied_ = false;
};

class RemovePanel : public Command {
public:
    explicit RemovePanel(PanelId target);

    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return "Remove panel"; }
    CommandKind kind() const noexcept override { return CommandKind::RemovePanel; }

private:
    PanelId target_;
    std::optional<Panel> snapshot_;   // для revert
};

}  // namespace coupecad::core
```

- [ ] **Step 2: Создать `src/coupecad/core/commands/panel_commands.cpp`** (реализация Add/Remove).

```cpp
#include "coupecad/core/commands/panel_commands.h"

#include "coupecad/core/errors.h"
#include "coupecad/core/project.h"

namespace coupecad::core {

AddPanel::AddPanel(PanelRole role, RoleParams role_params)
    : role_(role), role_params_(std::move(role_params)) {
    if (!is_role_params_valid_for(role, role_params_)) {
        throw DomainError{"panel.role_params_mismatch",
                          "role/role_params variant don't match"};
    }
}

ChangeSet AddPanel::apply(Project& project) {
    if (!applied_) {
        assigned_id_ = project.uuid_gen().next_id<PanelIdTag>();
    }
    Panel p;
    p.id = assigned_id_;
    p.role = role_;
    p.role_params = role_params_;
    p.validate();
    auto& cab = project.mutable_cabinet();
    if (cab.panels.find(assigned_id_) != cab.panels.end()) {
        throw LogicError{"panel.duplicate_id",
                         "AddPanel apply for already-present id"};
    }
    cab.panels.emplace(assigned_id_, std::move(p));
    applied_ = true;
    ChangeSet cs;
    cs.added_panels.push_back(assigned_id_);
    return cs;
}

ChangeSet AddPanel::revert(Project& project) {
    auto& cab = project.mutable_cabinet();
    auto it = cab.panels.find(assigned_id_);
    if (it == cab.panels.end()) {
        throw LogicError{"panel.missing_on_revert",
                         "AddPanel revert: panel not present"};
    }
    cab.panels.erase(it);
    ChangeSet cs;
    cs.removed_panels.push_back(assigned_id_);
    return cs;
}

RemovePanel::RemovePanel(PanelId target) : target_(target) {}

ChangeSet RemovePanel::apply(Project& project) {
    auto& cab = project.mutable_cabinet();
    auto it = cab.panels.find(target_);
    if (it == cab.panels.end()) {
        throw DomainError{"panel.unknown_id",
                          "RemovePanel: panel not in cabinet"};
    }
    // Проверка: ни один HardwareItem не должен ссылаться на этот PanelId.
    for (const auto& [_, hw] : cab.hardware) {
        for (const auto& a : hw.attachments) {
            if (a.panel_id == target_) {
                throw DomainError{"panel.still_referenced_by_hardware",
                                  "Cannot remove panel while hardware references it"};
            }
        }
    }
    snapshot_ = it->second;
    cab.panels.erase(it);
    ChangeSet cs;
    cs.removed_panels.push_back(target_);
    return cs;
}

ChangeSet RemovePanel::revert(Project& project) {
    if (!snapshot_) {
        throw LogicError{"panel.no_snapshot_on_revert",
                         "RemovePanel revert without prior apply"};
    }
    auto& cab = project.mutable_cabinet();
    cab.panels.emplace(target_, *snapshot_);
    ChangeSet cs;
    cs.added_panels.push_back(target_);
    return cs;
}

}  // namespace coupecad::core
```

- [ ] **Step 3: Обновить `src/coupecad/core/CMakeLists.txt`** — добавить `commands/panel_commands.cpp`.

- [ ] **Step 4: Создать `tests/core/commands/panel_commands_test.cpp`** (только Add/Remove-тесты; расширится в Task 4).

```cpp
#include "coupecad/core/commands/panel_commands.h"
#include "coupecad/core/errors.h"
#include "coupecad/core/project.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

namespace {
Project make_project() {
    return Project::create_empty("T", make_seeded_uuid_generator(2));
}
}  // namespace

TEST(AddPanel, AppliesAndRevertsRoundTrip) {
    auto p = make_project();
    AddPanel cmd{PanelRole::Shelf,
                  ShelfParams{.height_from_bottom = Millimeters{800},
                               .extent = ShelfFullWidth{}}};

    auto cs = cmd.apply(p);
    ASSERT_EQ(cs.added_panels.size(), 1u);
    auto pid = cmd.assigned_id();
    ASSERT_TRUE(pid.is_valid());
    EXPECT_EQ(p.cabinet().panels.size(), 1u);

    cmd.revert(p);
    EXPECT_EQ(p.cabinet().panels.size(), 0u);
}

TEST(AddPanel, RejectsRoleParamsMismatch) {
    EXPECT_THROW(AddPanel(PanelRole::Top, ShelfParams{}), DomainError);
}

TEST(AddPanel, RedoKeepsSameId) {
    auto p = make_project();
    AddPanel cmd{PanelRole::Top, NoRoleParams{}};
    cmd.apply(p);
    auto first_id = cmd.assigned_id();
    cmd.revert(p);
    cmd.apply(p);   // re-apply (redo)
    EXPECT_EQ(cmd.assigned_id(), first_id);
    EXPECT_EQ(p.cabinet().panels.count(first_id), 1u);
}

TEST(RemovePanel, RemovesAndRestores) {
    auto p = make_project();
    AddPanel add{PanelRole::Top, NoRoleParams{}};
    add.apply(p);
    auto pid = add.assigned_id();

    RemovePanel rm{pid};
    auto cs = rm.apply(p);
    EXPECT_EQ(cs.removed_panels.size(), 1u);
    EXPECT_EQ(p.cabinet().panels.count(pid), 0u);

    rm.revert(p);
    EXPECT_EQ(p.cabinet().panels.count(pid), 1u);
}

TEST(RemovePanel, UnknownIdThrows) {
    auto p = make_project();
    RemovePanel rm{PanelId{}};
    EXPECT_THROW(rm.apply(p), DomainError);
}

TEST(RemovePanel, BlockedByHardwareReference) {
    auto p = make_project();
    AddPanel add{PanelRole::Top, NoRoleParams{}};
    add.apply(p);
    auto pid = add.assigned_id();

    // Добавляем hardware spec + item прямым доступом (команды для них — Task 6/8).
    HardwareSpec spec{.ref = HardwareRef{"test.ref"},
                      .kind = HardwareKind::Hinge,
                      .name = "Test",
                      .bbox = Vec3{Millimeters{10}, Millimeters{10}, Millimeters{10}}};
    p.mutable_hardware_catalog()[spec.ref] = spec;
    HardwareItem hw;
    hw.id = p.uuid_gen().next_id<HardwareItemIdTag>();
    hw.ref = spec.ref;
    hw.attachments.push_back(PanelAttachment{.panel_id = pid,
                                              .local_position = Vec3{},
                                              .orientation = Quat::identity()});
    p.mutable_cabinet().hardware[hw.id] = std::move(hw);

    RemovePanel rm{pid};
    EXPECT_THROW(rm.apply(p), DomainError);
}
```

- [ ] **Step 5: Обновить `tests/core/commands/CMakeLists.txt`** — добавить `panel_commands_test.cpp`.

- [ ] **Step 6: Собрать и прогнать.**

```sh
cmake --build --preset default --target coupecad_commands_test
ctest --preset default -R "AddPanel|RemovePanel" --output-on-failure
```

Ожидается: 6 тестов passed.

- [ ] **Step 7: Закоммитить.**

```sh
git add src/coupecad/core/commands/panel_commands.* \
        src/coupecad/core/CMakeLists.txt \
        tests/core/commands/panel_commands_test.cpp \
        tests/core/commands/CMakeLists.txt
git commit -m "feat(core/commands): AddPanel + RemovePanel"
```

---

## Task 4: Panel Update commands

**Files:**
- Modify: `src/coupecad/core/commands/panel_commands.h`
- Modify: `src/coupecad/core/commands/panel_commands.cpp`
- Modify: `tests/core/commands/panel_commands_test.cpp`

Добавляем 6 update-команд: `UpdatePanelRoleParams` (preview), `SetPanelMaterial` (preview), `SetPanelThickness` (preview), `SetPanelEdgeBanding`, `SetPanelLabel`, `SetPanelGrain`.

- [ ] **Step 1: Дополнить `src/coupecad/core/commands/panel_commands.h`** — добавить в конец namespace (перед `}  // namespace coupecad::core`):

```cpp
// UpdatePanelRoleParams: меняет role_params панели. Preview-совместимая.
// role остаётся прежней.
class UpdatePanelRoleParams : public PreviewableCommand {
public:
    UpdatePanelRoleParams(PanelId target, RoleParams new_value);
    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    ChangeSet update(Project& project, const std::any& new_value) override;
    std::string_view label() const noexcept override { return "Update panel parameters"; }
    CommandKind kind() const noexcept override { return CommandKind::UpdatePanelRoleParams; }
private:
    PanelId target_;
    RoleParams new_value_;
    RoleParams old_value_;
    bool applied_ = false;
};

class SetPanelMaterial : public PreviewableCommand {
public:
    SetPanelMaterial(PanelId target, std::optional<MaterialId> new_value);
    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    ChangeSet update(Project& project, const std::any& new_value) override;
    std::string_view label() const noexcept override { return "Set panel material"; }
    CommandKind kind() const noexcept override { return CommandKind::SetPanelMaterial; }
private:
    PanelId target_;
    std::optional<MaterialId> new_value_;
    std::optional<MaterialId> old_value_;
    bool applied_ = false;
};

class SetPanelThickness : public PreviewableCommand {
public:
    SetPanelThickness(PanelId target, std::optional<Millimeters> new_value);
    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    ChangeSet update(Project& project, const std::any& new_value) override;
    std::string_view label() const noexcept override { return "Set panel thickness"; }
    CommandKind kind() const noexcept override { return CommandKind::SetPanelThickness; }
private:
    PanelId target_;
    std::optional<Millimeters> new_value_;
    std::optional<Millimeters> old_value_;
    bool applied_ = false;
};

// Side — какая из четырёх сторон панели.
enum class PanelSide { Front, Back, Left, Right };

class SetPanelEdgeBanding : public Command {
public:
    SetPanelEdgeBanding(PanelId target, PanelSide side,
                        std::optional<EdgeBanding> new_value);
    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return "Set panel edge banding"; }
    CommandKind kind() const noexcept override { return CommandKind::SetPanelEdgeBanding; }
private:
    PanelId target_;
    PanelSide side_;
    std::optional<EdgeBanding> new_value_;
    std::optional<EdgeBanding> old_value_;
    bool applied_ = false;
};

class SetPanelLabel : public Command {
public:
    SetPanelLabel(PanelId target, std::optional<std::string> new_value);
    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return "Set panel label"; }
    CommandKind kind() const noexcept override { return CommandKind::SetPanelLabel; }
private:
    PanelId target_;
    std::optional<std::string> new_value_;
    std::optional<std::string> old_value_;
    bool applied_ = false;
};

class SetPanelGrain : public Command {
public:
    SetPanelGrain(PanelId target, GrainDirection new_value);
    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return "Set panel grain"; }
    CommandKind kind() const noexcept override { return CommandKind::SetPanelGrain; }
private:
    PanelId target_;
    GrainDirection new_value_;
    GrainDirection old_value_{};
    bool applied_ = false;
};
```

Добавить в include-блок:

```cpp
#include <string>
```

- [ ] **Step 2: Дополнить `src/coupecad/core/commands/panel_commands.cpp`** — добавить в конец namespace:

```cpp
namespace {

Panel& get_panel_or_throw(Project& p, PanelId id, const char* code) {
    auto& cab = p.mutable_cabinet();
    auto it = cab.panels.find(id);
    if (it == cab.panels.end()) {
        throw DomainError{code, "Panel not found in cabinet"};
    }
    return it->second;
}

ChangeSet panel_updated(PanelId id) {
    ChangeSet cs;
    cs.updated_panels.push_back(id);
    return cs;
}

}  // namespace

// --- UpdatePanelRoleParams ---

UpdatePanelRoleParams::UpdatePanelRoleParams(PanelId target, RoleParams new_value)
    : target_(target), new_value_(std::move(new_value)) {}

ChangeSet UpdatePanelRoleParams::apply(Project& project) {
    auto& pan = get_panel_or_throw(project, target_,
                                    "panel.unknown_id");
    if (!is_role_params_valid_for(pan.role, new_value_)) {
        throw DomainError{"panel.role_params_mismatch",
                          "Incompatible role_params variant"};
    }
    if (!applied_) old_value_ = pan.role_params;
    pan.role_params = new_value_;
    applied_ = true;
    return panel_updated(target_);
}

ChangeSet UpdatePanelRoleParams::revert(Project& project) {
    auto& pan = get_panel_or_throw(project, target_, "panel.unknown_id");
    pan.role_params = old_value_;
    return panel_updated(target_);
}

ChangeSet UpdatePanelRoleParams::update(Project& project, const std::any& v) {
    auto* casted = std::any_cast<RoleParams>(&v);
    if (!casted) {
        throw LogicError{"command.preview.type_mismatch",
                         "UpdatePanelRoleParams::update expects RoleParams"};
    }
    auto& pan = get_panel_or_throw(project, target_, "panel.unknown_id");
    if (!is_role_params_valid_for(pan.role, *casted)) {
        throw DomainError{"panel.role_params_mismatch",
                          "Incompatible role_params variant"};
    }
    new_value_ = *casted;
    pan.role_params = *casted;
    return panel_updated(target_);
}

// --- SetPanelMaterial ---

SetPanelMaterial::SetPanelMaterial(PanelId target,
                                    std::optional<MaterialId> new_value)
    : target_(target), new_value_(new_value) {}

ChangeSet SetPanelMaterial::apply(Project& project) {
    if (new_value_ &&
        project.materials().find(*new_value_) == project.materials().end()) {
        throw DomainError{"project.panel_material_missing",
                          "material_override references unknown material"};
    }
    auto& pan = get_panel_or_throw(project, target_, "panel.unknown_id");
    if (!applied_) old_value_ = pan.material_override;
    pan.material_override = new_value_;
    applied_ = true;
    return panel_updated(target_);
}

ChangeSet SetPanelMaterial::revert(Project& project) {
    auto& pan = get_panel_or_throw(project, target_, "panel.unknown_id");
    pan.material_override = old_value_;
    return panel_updated(target_);
}

ChangeSet SetPanelMaterial::update(Project& project, const std::any& v) {
    auto* casted = std::any_cast<std::optional<MaterialId>>(&v);
    if (!casted) {
        throw LogicError{"command.preview.type_mismatch",
                         "SetPanelMaterial::update expects std::optional<MaterialId>"};
    }
    if (*casted &&
        project.materials().find(**casted) == project.materials().end()) {
        throw DomainError{"project.panel_material_missing",
                          "material_override references unknown material"};
    }
    auto& pan = get_panel_or_throw(project, target_, "panel.unknown_id");
    new_value_ = *casted;
    pan.material_override = *casted;
    return panel_updated(target_);
}

// --- SetPanelThickness ---

SetPanelThickness::SetPanelThickness(PanelId target,
                                      std::optional<Millimeters> new_value)
    : target_(target), new_value_(new_value) {
    if (new_value && new_value->value() <= 0) {
        throw DomainError{"panel.nonpositive_thickness",
                          "thickness must be > 0"};
    }
}

ChangeSet SetPanelThickness::apply(Project& project) {
    auto& pan = get_panel_or_throw(project, target_, "panel.unknown_id");
    if (!applied_) old_value_ = pan.thickness_override;
    pan.thickness_override = new_value_;
    applied_ = true;
    return panel_updated(target_);
}

ChangeSet SetPanelThickness::revert(Project& project) {
    auto& pan = get_panel_or_throw(project, target_, "panel.unknown_id");
    pan.thickness_override = old_value_;
    return panel_updated(target_);
}

ChangeSet SetPanelThickness::update(Project& project, const std::any& v) {
    auto* casted = std::any_cast<std::optional<Millimeters>>(&v);
    if (!casted) {
        throw LogicError{"command.preview.type_mismatch",
                         "SetPanelThickness::update expects std::optional<Millimeters>"};
    }
    if (*casted && (*casted)->value() <= 0) {
        throw DomainError{"panel.nonpositive_thickness",
                          "thickness must be > 0"};
    }
    auto& pan = get_panel_or_throw(project, target_, "panel.unknown_id");
    new_value_ = *casted;
    pan.thickness_override = *casted;
    return panel_updated(target_);
}

// --- SetPanelEdgeBanding ---

namespace {
std::optional<EdgeBanding>& edge_side(PanelEdgeBanding& eb, PanelSide side) {
    switch (side) {
        case PanelSide::Front: return eb.front;
        case PanelSide::Back:  return eb.back;
        case PanelSide::Left:  return eb.left;
        case PanelSide::Right: return eb.right;
    }
    // unreachable
    return eb.front;
}
}  // namespace

SetPanelEdgeBanding::SetPanelEdgeBanding(PanelId target, PanelSide side,
                                          std::optional<EdgeBanding> new_value)
    : target_(target), side_(side), new_value_(std::move(new_value)) {
    if (new_value_ &&
        (new_value_->thickness.value() <= 0 ||
         !new_value_->material_id.is_valid())) {
        throw DomainError{"panel.edge_banding_invalid",
                          "edge banding thickness or material invalid"};
    }
}

ChangeSet SetPanelEdgeBanding::apply(Project& project) {
    if (new_value_ &&
        project.materials().find(new_value_->material_id) ==
            project.materials().end()) {
        throw DomainError{"project.edge_banding_material_missing",
                          "edge banding material not in project"};
    }
    auto& pan = get_panel_or_throw(project, target_, "panel.unknown_id");
    auto& slot = edge_side(pan.edge_banding, side_);
    if (!applied_) old_value_ = slot;
    slot = new_value_;
    applied_ = true;
    return panel_updated(target_);
}

ChangeSet SetPanelEdgeBanding::revert(Project& project) {
    auto& pan = get_panel_or_throw(project, target_, "panel.unknown_id");
    edge_side(pan.edge_banding, side_) = old_value_;
    return panel_updated(target_);
}

// --- SetPanelLabel ---

SetPanelLabel::SetPanelLabel(PanelId target, std::optional<std::string> new_value)
    : target_(target), new_value_(std::move(new_value)) {}

ChangeSet SetPanelLabel::apply(Project& project) {
    auto& pan = get_panel_or_throw(project, target_, "panel.unknown_id");
    if (!applied_) old_value_ = pan.label;
    pan.label = new_value_;
    applied_ = true;
    return panel_updated(target_);
}

ChangeSet SetPanelLabel::revert(Project& project) {
    auto& pan = get_panel_or_throw(project, target_, "panel.unknown_id");
    pan.label = old_value_;
    return panel_updated(target_);
}

// --- SetPanelGrain ---

SetPanelGrain::SetPanelGrain(PanelId target, GrainDirection new_value)
    : target_(target), new_value_(new_value) {}

ChangeSet SetPanelGrain::apply(Project& project) {
    auto& pan = get_panel_or_throw(project, target_, "panel.unknown_id");
    if (!applied_) old_value_ = pan.grain_direction;
    pan.grain_direction = new_value_;
    applied_ = true;
    return panel_updated(target_);
}

ChangeSet SetPanelGrain::revert(Project& project) {
    auto& pan = get_panel_or_throw(project, target_, "panel.unknown_id");
    pan.grain_direction = old_value_;
    return panel_updated(target_);
}
```

- [ ] **Step 3: Дополнить `tests/core/commands/panel_commands_test.cpp`** — добавить тесты для всех 6 команд:

```cpp
// ... (в конец файла перед закрытием пространства using)

TEST(UpdatePanelRoleParams, ApplyRevert) {
    auto p = make_project();
    AddPanel add{PanelRole::Shelf,
                  ShelfParams{.height_from_bottom = Millimeters{800},
                               .extent = ShelfFullWidth{}}};
    add.apply(p);
    auto pid = add.assigned_id();

    UpdatePanelRoleParams upd{pid,
                               ShelfParams{.height_from_bottom = Millimeters{1000},
                                            .extent = ShelfFullWidth{}}};
    upd.apply(p);
    auto& sp = std::get<ShelfParams>(p.cabinet().panels.at(pid).role_params);
    EXPECT_EQ(sp.height_from_bottom, Millimeters{1000});

    upd.revert(p);
    auto& sp2 = std::get<ShelfParams>(p.cabinet().panels.at(pid).role_params);
    EXPECT_EQ(sp2.height_from_bottom, Millimeters{800});
}

TEST(UpdatePanelRoleParams, WrongVariantThrows) {
    auto p = make_project();
    AddPanel add{PanelRole::Top, NoRoleParams{}};
    add.apply(p);
    UpdatePanelRoleParams upd{add.assigned_id(), ShelfParams{}};
    EXPECT_THROW(upd.apply(p), DomainError);
}

TEST(UpdatePanelRoleParams, PreviewUpdateLive) {
    auto p = make_project();
    AddPanel add{PanelRole::Shelf,
                  ShelfParams{.height_from_bottom = Millimeters{800},
                               .extent = ShelfFullWidth{}}};
    add.apply(p);
    auto pid = add.assigned_id();
    UpdatePanelRoleParams upd{pid,
                               ShelfParams{.height_from_bottom = Millimeters{900},
                                            .extent = ShelfFullWidth{}}};
    upd.apply(p);
    upd.update(p, std::any{ShelfParams{.height_from_bottom = Millimeters{950},
                                         .extent = ShelfFullWidth{}}});
    auto& sp = std::get<ShelfParams>(p.cabinet().panels.at(pid).role_params);
    EXPECT_EQ(sp.height_from_bottom, Millimeters{950});
    upd.revert(p);
    auto& sp2 = std::get<ShelfParams>(p.cabinet().panels.at(pid).role_params);
    EXPECT_EQ(sp2.height_from_bottom, Millimeters{800});
}

TEST(SetPanelMaterial, ApplyRevert) {
    auto p = make_project();
    AddPanel add{PanelRole::Top, NoRoleParams{}};
    add.apply(p);
    auto pid = add.assigned_id();
    auto mat_id = p.uuid_gen().next_id<MaterialIdTag>();
    p.mutable_materials()[mat_id] = Material{.id = mat_id, .name = "Test",
                                              .kind = MaterialKind::Mdf,
                                              .default_thickness = Millimeters{18}};
    SetPanelMaterial cmd{pid, mat_id};
    cmd.apply(p);
    EXPECT_EQ(p.cabinet().panels.at(pid).material_override, mat_id);
    cmd.revert(p);
    EXPECT_FALSE(p.cabinet().panels.at(pid).material_override.has_value());
}

TEST(SetPanelMaterial, UnknownMaterialThrows) {
    auto p = make_project();
    AddPanel add{PanelRole::Top, NoRoleParams{}};
    add.apply(p);
    SetPanelMaterial cmd{add.assigned_id(),
                          p.uuid_gen().next_id<MaterialIdTag>()};
    EXPECT_THROW(cmd.apply(p), DomainError);
}

TEST(SetPanelThickness, ApplyRevert) {
    auto p = make_project();
    AddPanel add{PanelRole::Top, NoRoleParams{}};
    add.apply(p);
    SetPanelThickness cmd{add.assigned_id(), Millimeters{22}};
    cmd.apply(p);
    EXPECT_EQ(*p.cabinet().panels.at(add.assigned_id()).thickness_override,
              Millimeters{22});
    cmd.revert(p);
    EXPECT_FALSE(p.cabinet().panels.at(add.assigned_id())
                     .thickness_override.has_value());
}

TEST(SetPanelThickness, RejectsNonPositive) {
    EXPECT_THROW(SetPanelThickness(PanelId{}, Millimeters{0}), DomainError);
}

TEST(SetPanelEdgeBanding, ApplyRevert) {
    auto p = make_project();
    AddPanel add{PanelRole::Top, NoRoleParams{}};
    add.apply(p);
    auto pid = add.assigned_id();
    auto mat_id = p.cabinet().default_panel_material;
    SetPanelEdgeBanding cmd{pid, PanelSide::Front,
                              EdgeBanding{.material_id = mat_id,
                                           .thickness = Millimeters{2}}};
    cmd.apply(p);
    ASSERT_TRUE(p.cabinet().panels.at(pid).edge_banding.front.has_value());
    EXPECT_EQ(p.cabinet().panels.at(pid).edge_banding.front->thickness,
              Millimeters{2});
    cmd.revert(p);
    EXPECT_FALSE(p.cabinet().panels.at(pid).edge_banding.front.has_value());
}

TEST(SetPanelLabel, ApplyRevert) {
    auto p = make_project();
    AddPanel add{PanelRole::Top, NoRoleParams{}};
    add.apply(p);
    auto pid = add.assigned_id();
    SetPanelLabel cmd{pid, std::string{"Top panel"}};
    cmd.apply(p);
    EXPECT_EQ(*p.cabinet().panels.at(pid).label, "Top panel");
    cmd.revert(p);
    EXPECT_FALSE(p.cabinet().panels.at(pid).label.has_value());
}

TEST(SetPanelGrain, ApplyRevert) {
    auto p = make_project();
    AddPanel add{PanelRole::Top, NoRoleParams{}};
    add.apply(p);
    auto pid = add.assigned_id();
    SetPanelGrain cmd{pid, GrainDirection::Horizontal};
    cmd.apply(p);
    EXPECT_EQ(p.cabinet().panels.at(pid).grain_direction,
              GrainDirection::Horizontal);
    cmd.revert(p);
    EXPECT_EQ(p.cabinet().panels.at(pid).grain_direction, GrainDirection::None);
}
```

- [ ] **Step 4: Собрать и прогнать.**

```sh
cmake --build --preset default --target coupecad_commands_test
ctest --preset default -R "Panel" --output-on-failure
```

Ожидается: 16 тестов passed (6 из Task 3 + 10 новых).

- [ ] **Step 5: Закоммитить.**

```sh
git add src/coupecad/core/commands/panel_commands.* \
        tests/core/commands/panel_commands_test.cpp
git commit -m "feat(core/commands): Update/SetPanel* commands (RoleParams/Material/Thickness/EdgeBanding/Label/Grain)"
```

---

## Task 5: Material commands (Add/Update/Remove)

**Files:**
- Create: `src/coupecad/core/commands/material_commands.h`
- Create: `src/coupecad/core/commands/material_commands.cpp`
- Modify: `src/coupecad/core/CMakeLists.txt`
- Create: `tests/core/commands/material_commands_test.cpp`
- Modify: `tests/core/commands/CMakeLists.txt`

- [ ] **Step 1: `material_commands.h`.**

```cpp
#pragma once

#include "coupecad/core/commands/command.h"
#include "coupecad/core/id.h"
#include "coupecad/core/material.h"

#include <optional>

namespace coupecad::core {

class AddMaterial : public Command {
public:
    // Если у material.id уже valid — сохраняется (для redo); иначе назначается
    // новый из UuidGenerator при первом apply.
    explicit AddMaterial(Material material);
    MaterialId assigned_id() const noexcept { return assigned_id_; }
    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return "Add material"; }
    CommandKind kind() const noexcept override { return CommandKind::AddMaterial; }
private:
    Material material_;
    MaterialId assigned_id_{};
    bool applied_ = false;
};

class UpdateMaterial : public Command {
public:
    UpdateMaterial(MaterialId target, Material new_value);
    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return "Update material"; }
    CommandKind kind() const noexcept override { return CommandKind::UpdateMaterial; }
private:
    MaterialId target_;
    Material new_value_;
    Material old_value_;
    bool applied_ = false;
};

class RemoveMaterial : public Command {
public:
    explicit RemoveMaterial(MaterialId target);
    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return "Remove material"; }
    CommandKind kind() const noexcept override { return CommandKind::RemoveMaterial; }
private:
    MaterialId target_;
    std::optional<Material> snapshot_;
};

}  // namespace coupecad::core
```

- [ ] **Step 2: `material_commands.cpp`.**

```cpp
#include "coupecad/core/commands/material_commands.h"

#include "coupecad/core/errors.h"
#include "coupecad/core/project.h"

namespace coupecad::core {

AddMaterial::AddMaterial(Material m) : material_(std::move(m)) {}

ChangeSet AddMaterial::apply(Project& project) {
    if (!applied_) {
        if (!material_.id.is_valid()) {
            material_.id = project.uuid_gen().next_id<MaterialIdTag>();
        }
        assigned_id_ = material_.id;
    }
    material_.validate();
    auto& mats = project.mutable_materials();
    if (mats.find(assigned_id_) != mats.end()) {
        throw LogicError{"material.duplicate_id",
                         "AddMaterial apply for already-present id"};
    }
    mats.emplace(assigned_id_, material_);
    applied_ = true;
    ChangeSet cs;
    cs.added_materials.push_back(assigned_id_);
    return cs;
}

ChangeSet AddMaterial::revert(Project& project) {
    auto& mats = project.mutable_materials();
    auto it = mats.find(assigned_id_);
    if (it == mats.end()) {
        throw LogicError{"material.missing_on_revert",
                         "AddMaterial revert: not present"};
    }
    mats.erase(it);
    ChangeSet cs;
    cs.removed_materials.push_back(assigned_id_);
    return cs;
}

UpdateMaterial::UpdateMaterial(MaterialId target, Material new_value)
    : target_(target), new_value_(std::move(new_value)) {
    new_value_.id = target_;   // нельзя менять id через update
}

ChangeSet UpdateMaterial::apply(Project& project) {
    auto& mats = project.mutable_materials();
    auto it = mats.find(target_);
    if (it == mats.end()) {
        throw DomainError{"material.unknown_id",
                          "UpdateMaterial: target not in project"};
    }
    new_value_.validate();
    if (!applied_) old_value_ = it->second;
    it->second = new_value_;
    applied_ = true;
    ChangeSet cs;
    cs.updated_materials.push_back(target_);
    return cs;
}

ChangeSet UpdateMaterial::revert(Project& project) {
    auto& mats = project.mutable_materials();
    mats[target_] = old_value_;
    ChangeSet cs;
    cs.updated_materials.push_back(target_);
    return cs;
}

RemoveMaterial::RemoveMaterial(MaterialId target) : target_(target) {}

ChangeSet RemoveMaterial::apply(Project& project) {
    auto& mats = project.mutable_materials();
    auto it = mats.find(target_);
    if (it == mats.end()) {
        throw DomainError{"material.unknown_id",
                          "RemoveMaterial: target not in project"};
    }
    // Проверка: никакая панель и не default_panel_material не ссылаются.
    if (project.cabinet().default_panel_material == target_) {
        throw DomainError{"material.still_used_as_cabinet_default",
                          "Material in use as cabinet.default_panel_material"};
    }
    for (const auto& [_, pan] : project.cabinet().panels) {
        if (pan.material_override == target_) {
            throw DomainError{"material.still_used_by_panel_override",
                              "Material in use as panel.material_override"};
        }
        auto eb_uses = [&](const std::optional<EdgeBanding>& eb) {
            return eb && eb->material_id == target_;
        };
        if (eb_uses(pan.edge_banding.front) || eb_uses(pan.edge_banding.back) ||
            eb_uses(pan.edge_banding.left)  || eb_uses(pan.edge_banding.right)) {
            throw DomainError{"material.still_used_by_edge_banding",
                              "Material in use by edge banding"};
        }
    }
    snapshot_ = it->second;
    mats.erase(it);
    ChangeSet cs;
    cs.removed_materials.push_back(target_);
    return cs;
}

ChangeSet RemoveMaterial::revert(Project& project) {
    if (!snapshot_) {
        throw LogicError{"material.no_snapshot_on_revert",
                         "RemoveMaterial revert without prior apply"};
    }
    project.mutable_materials()[target_] = *snapshot_;
    ChangeSet cs;
    cs.added_materials.push_back(target_);
    return cs;
}

}  // namespace coupecad::core
```

- [ ] **Step 3: Обновить `src/coupecad/core/CMakeLists.txt`** — добавить `commands/material_commands.cpp`.

- [ ] **Step 4: `tests/core/commands/material_commands_test.cpp`.**

```cpp
#include "coupecad/core/commands/material_commands.h"
#include "coupecad/core/commands/panel_commands.h"
#include "coupecad/core/errors.h"
#include "coupecad/core/project.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

namespace {
Project make_project() {
    return Project::create_empty("T", make_seeded_uuid_generator(3));
}

Material make_mat(std::string name = "NewMaterial") {
    return Material{.name = std::move(name),
                     .kind = MaterialKind::Mdf,
                     .default_thickness = Millimeters{18}};
}
}  // namespace

TEST(AddMaterial, ApplyRevert) {
    auto p = make_project();
    auto size_before = p.materials().size();
    AddMaterial cmd{make_mat()};
    cmd.apply(p);
    EXPECT_TRUE(cmd.assigned_id().is_valid());
    EXPECT_EQ(p.materials().size(), size_before + 1);
    cmd.revert(p);
    EXPECT_EQ(p.materials().size(), size_before);
}

TEST(AddMaterial, InvalidMaterialThrows) {
    auto p = make_project();
    Material bad = make_mat();
    bad.default_thickness = Millimeters{0};
    AddMaterial cmd{bad};
    EXPECT_THROW(cmd.apply(p), DomainError);
}

TEST(UpdateMaterial, ApplyRevert) {
    auto p = make_project();
    AddMaterial add{make_mat("Orig")};
    add.apply(p);
    auto mid = add.assigned_id();
    Material patched = make_mat("Updated");
    UpdateMaterial upd{mid, patched};
    upd.apply(p);
    EXPECT_EQ(p.materials().at(mid).name, "Updated");
    upd.revert(p);
    EXPECT_EQ(p.materials().at(mid).name, "Orig");
}

TEST(RemoveMaterial, ApplyRevert) {
    auto p = make_project();
    AddMaterial add{make_mat()};
    add.apply(p);
    auto mid = add.assigned_id();
    RemoveMaterial rm{mid};
    rm.apply(p);
    EXPECT_EQ(p.materials().count(mid), 0u);
    rm.revert(p);
    EXPECT_EQ(p.materials().count(mid), 1u);
}

TEST(RemoveMaterial, CabinetDefaultBlocksRemoval) {
    auto p = make_project();
    RemoveMaterial rm{p.cabinet().default_panel_material};
    EXPECT_THROW(rm.apply(p), DomainError);
}

TEST(RemoveMaterial, PanelOverrideBlocksRemoval) {
    auto p = make_project();
    AddMaterial add_mat{make_mat()};
    add_mat.apply(p);
    auto mid = add_mat.assigned_id();

    AddPanel add_pan{PanelRole::Top, NoRoleParams{}};
    add_pan.apply(p);
    SetPanelMaterial set_mat{add_pan.assigned_id(), mid};
    set_mat.apply(p);

    RemoveMaterial rm{mid};
    EXPECT_THROW(rm.apply(p), DomainError);
}
```

- [ ] **Step 5: Обновить `tests/core/commands/CMakeLists.txt`** — добавить `material_commands_test.cpp`.

- [ ] **Step 6: Собрать и прогнать.**

```sh
cmake --build --preset default --target coupecad_commands_test
ctest --preset default -R "AddMaterial|UpdateMaterial|RemoveMaterial" --output-on-failure
```

Ожидается: 6 тестов passed.

- [ ] **Step 7: Закоммитить.**

```sh
git add src/coupecad/core/commands/material_commands.* \
        src/coupecad/core/CMakeLists.txt \
        tests/core/commands/material_commands_test.cpp \
        tests/core/commands/CMakeLists.txt
git commit -m "feat(core/commands): Add/Update/RemoveMaterial with reference checks"
```

---

## Task 6: Hardware commands (Add/Remove/UpdateAttachments)

**Files:**
- Create: `src/coupecad/core/commands/hardware_commands.h`
- Create: `src/coupecad/core/commands/hardware_commands.cpp`
- Modify: `src/coupecad/core/CMakeLists.txt`
- Create: `tests/core/commands/hardware_commands_test.cpp`
- Modify: `tests/core/commands/CMakeLists.txt`

- [ ] **Step 1: `hardware_commands.h`.**

```cpp
#pragma once

#include "coupecad/core/commands/command.h"
#include "coupecad/core/hardware.h"
#include "coupecad/core/id.h"

#include <optional>
#include <vector>

namespace coupecad::core {

class AddHardware : public Command {
public:
    AddHardware(HardwareRef ref, std::vector<PanelAttachment> attachments);
    HardwareItemId assigned_id() const noexcept { return assigned_id_; }
    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return "Add hardware"; }
    CommandKind kind() const noexcept override { return CommandKind::AddHardware; }
private:
    HardwareRef ref_;
    std::vector<PanelAttachment> attachments_;
    HardwareItemId assigned_id_{};
    bool applied_ = false;
};

class RemoveHardware : public Command {
public:
    explicit RemoveHardware(HardwareItemId target);
    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return "Remove hardware"; }
    CommandKind kind() const noexcept override { return CommandKind::RemoveHardware; }
private:
    HardwareItemId target_;
    std::optional<HardwareItem> snapshot_;
};

class UpdateHardwareAttachments : public Command {
public:
    UpdateHardwareAttachments(HardwareItemId target,
                               std::vector<PanelAttachment> new_attachments);
    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return "Update hardware attachments"; }
    CommandKind kind() const noexcept override { return CommandKind::UpdateHardwareAttachments; }
private:
    HardwareItemId target_;
    std::vector<PanelAttachment> new_value_;
    std::vector<PanelAttachment> old_value_;
    bool applied_ = false;
};

}  // namespace coupecad::core
```

- [ ] **Step 2: `hardware_commands.cpp`.**

```cpp
#include "coupecad/core/commands/hardware_commands.h"

#include "coupecad/core/errors.h"
#include "coupecad/core/project.h"

namespace coupecad::core {

namespace {
void check_attachments(const Project& p,
                       const std::vector<PanelAttachment>& atts) {
    if (atts.empty()) {
        throw DomainError{"hardware_item.no_attachments",
                          "At least one attachment required"};
    }
    for (const auto& a : atts) {
        if (!a.panel_id.is_valid()) {
            throw DomainError{"hardware_item.invalid_attachment_panel",
                              "attachment has invalid panel_id"};
        }
        if (p.cabinet().panels.find(a.panel_id) == p.cabinet().panels.end()) {
            throw DomainError{"cabinet.hardware_unknown_panel",
                              "attachment references unknown panel"};
        }
    }
}
}  // namespace

AddHardware::AddHardware(HardwareRef ref,
                         std::vector<PanelAttachment> attachments)
    : ref_(std::move(ref)), attachments_(std::move(attachments)) {
    if (ref_.value().empty()) {
        throw DomainError{"hardware_item.empty_ref",
                          "HardwareRef is empty"};
    }
}

ChangeSet AddHardware::apply(Project& project) {
    if (project.hardware_catalog().find(ref_) ==
        project.hardware_catalog().end()) {
        throw DomainError{"project.hardware_ref_missing",
                          "HardwareRef not in hardware_catalog"};
    }
    check_attachments(project, attachments_);
    if (!applied_) {
        assigned_id_ = project.uuid_gen().next_id<HardwareItemIdTag>();
    }
    HardwareItem hw;
    hw.id = assigned_id_;
    hw.ref = ref_;
    hw.attachments = attachments_;
    auto& cab = project.mutable_cabinet();
    cab.hardware.emplace(assigned_id_, std::move(hw));
    applied_ = true;
    ChangeSet cs;
    cs.added_hardware.push_back(assigned_id_);
    return cs;
}

ChangeSet AddHardware::revert(Project& project) {
    auto& cab = project.mutable_cabinet();
    cab.hardware.erase(assigned_id_);
    ChangeSet cs;
    cs.removed_hardware.push_back(assigned_id_);
    return cs;
}

RemoveHardware::RemoveHardware(HardwareItemId target) : target_(target) {}

ChangeSet RemoveHardware::apply(Project& project) {
    auto& cab = project.mutable_cabinet();
    auto it = cab.hardware.find(target_);
    if (it == cab.hardware.end()) {
        throw DomainError{"hardware_item.unknown_id",
                          "RemoveHardware: target not in cabinet"};
    }
    snapshot_ = it->second;
    cab.hardware.erase(it);
    ChangeSet cs;
    cs.removed_hardware.push_back(target_);
    return cs;
}

ChangeSet RemoveHardware::revert(Project& project) {
    if (!snapshot_) {
        throw LogicError{"hardware_item.no_snapshot_on_revert",
                         "RemoveHardware revert without apply"};
    }
    project.mutable_cabinet().hardware[target_] = *snapshot_;
    ChangeSet cs;
    cs.added_hardware.push_back(target_);
    return cs;
}

UpdateHardwareAttachments::UpdateHardwareAttachments(
    HardwareItemId target, std::vector<PanelAttachment> new_attachments)
    : target_(target), new_value_(std::move(new_attachments)) {}

ChangeSet UpdateHardwareAttachments::apply(Project& project) {
    auto& cab = project.mutable_cabinet();
    auto it = cab.hardware.find(target_);
    if (it == cab.hardware.end()) {
        throw DomainError{"hardware_item.unknown_id",
                          "UpdateHardwareAttachments: target not in cabinet"};
    }
    check_attachments(project, new_value_);
    if (!applied_) old_value_ = it->second.attachments;
    it->second.attachments = new_value_;
    applied_ = true;
    ChangeSet cs;
    cs.updated_hardware.push_back(target_);
    return cs;
}

ChangeSet UpdateHardwareAttachments::revert(Project& project) {
    project.mutable_cabinet().hardware.at(target_).attachments = old_value_;
    ChangeSet cs;
    cs.updated_hardware.push_back(target_);
    return cs;
}

}  // namespace coupecad::core
```

- [ ] **Step 3: Обновить CMakeLists.txt.**

- [ ] **Step 4: `hardware_commands_test.cpp`.**

```cpp
#include "coupecad/core/commands/hardware_commands.h"
#include "coupecad/core/commands/panel_commands.h"
#include "coupecad/core/errors.h"
#include "coupecad/core/project.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

namespace {
struct Fixture {
    Project p = Project::create_empty("T", make_seeded_uuid_generator(4));
    PanelId panel_id;
    HardwareRef ref{"hinge.test"};

    Fixture() {
        AddPanel add{PanelRole::Top, NoRoleParams{}};
        add.apply(p);
        panel_id = add.assigned_id();
        HardwareSpec spec{.ref = ref, .kind = HardwareKind::Hinge,
                          .name = "Test hinge",
                          .bbox = Vec3{Millimeters{10}, Millimeters{10},
                                       Millimeters{10}}};
        p.mutable_hardware_catalog()[ref] = spec;
    }

    PanelAttachment att() const {
        return PanelAttachment{.panel_id = panel_id,
                                .local_position = Vec3{},
                                .orientation = Quat::identity()};
    }
};
}  // namespace

TEST(AddHardware, ApplyRevert) {
    Fixture f;
    AddHardware cmd{f.ref, {f.att()}};
    cmd.apply(f.p);
    EXPECT_EQ(f.p.cabinet().hardware.size(), 1u);
    cmd.revert(f.p);
    EXPECT_EQ(f.p.cabinet().hardware.size(), 0u);
}

TEST(AddHardware, UnknownRefThrows) {
    Fixture f;
    AddHardware cmd{HardwareRef{"unknown"}, {f.att()}};
    EXPECT_THROW(cmd.apply(f.p), DomainError);
}

TEST(AddHardware, NoAttachmentsThrows) {
    Fixture f;
    AddHardware cmd{f.ref, {}};
    EXPECT_THROW(cmd.apply(f.p), DomainError);
}

TEST(AddHardware, UnknownPanelThrows) {
    Fixture f;
    PanelAttachment bad{.panel_id = f.p.uuid_gen().next_id<PanelIdTag>(),
                         .local_position = Vec3{},
                         .orientation = Quat::identity()};
    AddHardware cmd{f.ref, {bad}};
    EXPECT_THROW(cmd.apply(f.p), DomainError);
}

TEST(RemoveHardware, ApplyRevert) {
    Fixture f;
    AddHardware add{f.ref, {f.att()}};
    add.apply(f.p);
    auto hid = add.assigned_id();
    RemoveHardware rm{hid};
    rm.apply(f.p);
    EXPECT_EQ(f.p.cabinet().hardware.size(), 0u);
    rm.revert(f.p);
    EXPECT_EQ(f.p.cabinet().hardware.size(), 1u);
}

TEST(UpdateHardwareAttachments, ApplyRevert) {
    Fixture f;
    AddHardware add{f.ref, {f.att()}};
    add.apply(f.p);
    auto hid = add.assigned_id();
    auto doubled = f.att();
    doubled.local_position.x = Millimeters{50};
    UpdateHardwareAttachments upd{hid, {f.att(), doubled}};
    upd.apply(f.p);
    EXPECT_EQ(f.p.cabinet().hardware.at(hid).attachments.size(), 2u);
    upd.revert(f.p);
    EXPECT_EQ(f.p.cabinet().hardware.at(hid).attachments.size(), 1u);
}
```

- [ ] **Step 5: Обновить tests CMakeLists.**

- [ ] **Step 6: Build + test.**

```sh
cmake --build --preset default --target coupecad_commands_test
ctest --preset default -R "Hardware" --output-on-failure
```

Ожидается: 9 тестов из Task 1 Hardware* (уже были) + 6 новых = 15.

- [ ] **Step 7: Commit.**

```sh
git commit -am "feat(core/commands): Add/Remove/UpdateHardwareAttachments"
```

---

## Task 7: Hardware catalog commands (Add/Update/RemoveHardwareSpec)

**Files:**
- Create: `src/coupecad/core/commands/hardware_spec_commands.h`
- Create: `src/coupecad/core/commands/hardware_spec_commands.cpp`
- Modify: `src/coupecad/core/CMakeLists.txt`
- Create: `tests/core/commands/hardware_spec_commands_test.cpp`
- Modify: `tests/core/commands/CMakeLists.txt`

Паттерн полностью симметричен Material commands (Task 5), но ключ — `HardwareRef` (строка), а не UUID. RemoveHardwareSpec проверяет, что ни один HardwareItem не использует ref.

- [ ] **Step 1: `hardware_spec_commands.h`.**

```cpp
#pragma once

#include "coupecad/core/commands/command.h"
#include "coupecad/core/hardware.h"
#include "coupecad/core/id.h"

#include <optional>

namespace coupecad::core {

class AddHardwareSpec : public Command {
public:
    explicit AddHardwareSpec(HardwareSpec spec);
    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return "Add hardware spec"; }
    CommandKind kind() const noexcept override { return CommandKind::AddHardwareSpec; }
private:
    HardwareSpec spec_;
    bool applied_ = false;
};

class UpdateHardwareSpec : public Command {
public:
    UpdateHardwareSpec(HardwareRef target, HardwareSpec new_value);
    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return "Update hardware spec"; }
    CommandKind kind() const noexcept override { return CommandKind::UpdateHardwareSpec; }
private:
    HardwareRef target_;
    HardwareSpec new_value_;
    HardwareSpec old_value_;
    bool applied_ = false;
};

class RemoveHardwareSpec : public Command {
public:
    explicit RemoveHardwareSpec(HardwareRef target);
    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return "Remove hardware spec"; }
    CommandKind kind() const noexcept override { return CommandKind::RemoveHardwareSpec; }
private:
    HardwareRef target_;
    std::optional<HardwareSpec> snapshot_;
};

}  // namespace coupecad::core
```

- [ ] **Step 2: `hardware_spec_commands.cpp`.**

```cpp
#include "coupecad/core/commands/hardware_spec_commands.h"

#include "coupecad/core/errors.h"
#include "coupecad/core/project.h"

namespace coupecad::core {

AddHardwareSpec::AddHardwareSpec(HardwareSpec spec) : spec_(std::move(spec)) {
    spec_.validate();
}

ChangeSet AddHardwareSpec::apply(Project& project) {
    auto& cat = project.mutable_hardware_catalog();
    if (cat.find(spec_.ref) != cat.end()) {
        throw DomainError{"hardware_spec.duplicate_ref",
                          "HardwareSpec with this ref already exists"};
    }
    cat[spec_.ref] = spec_;
    applied_ = true;
    return ChangeSet{};
}

ChangeSet AddHardwareSpec::revert(Project& project) {
    project.mutable_hardware_catalog().erase(spec_.ref);
    return ChangeSet{};
}

UpdateHardwareSpec::UpdateHardwareSpec(HardwareRef target, HardwareSpec new_value)
    : target_(std::move(target)), new_value_(std::move(new_value)) {
    new_value_.ref = target_;   // ref неизменен
    new_value_.validate();
}

ChangeSet UpdateHardwareSpec::apply(Project& project) {
    auto& cat = project.mutable_hardware_catalog();
    auto it = cat.find(target_);
    if (it == cat.end()) {
        throw DomainError{"hardware_spec.unknown_ref",
                          "UpdateHardwareSpec: target not in catalog"};
    }
    if (!applied_) old_value_ = it->second;
    it->second = new_value_;
    applied_ = true;
    return ChangeSet{};
}

ChangeSet UpdateHardwareSpec::revert(Project& project) {
    project.mutable_hardware_catalog()[target_] = old_value_;
    return ChangeSet{};
}

RemoveHardwareSpec::RemoveHardwareSpec(HardwareRef target) : target_(std::move(target)) {}

ChangeSet RemoveHardwareSpec::apply(Project& project) {
    auto& cat = project.mutable_hardware_catalog();
    auto it = cat.find(target_);
    if (it == cat.end()) {
        throw DomainError{"hardware_spec.unknown_ref",
                          "RemoveHardwareSpec: target not in catalog"};
    }
    for (const auto& [_, hw] : project.cabinet().hardware) {
        if (hw.ref == target_) {
            throw DomainError{"hardware_spec.still_in_use",
                              "HardwareSpec in use by a HardwareItem"};
        }
    }
    snapshot_ = it->second;
    cat.erase(it);
    return ChangeSet{};
}

ChangeSet RemoveHardwareSpec::revert(Project& project) {
    if (!snapshot_) {
        throw LogicError{"hardware_spec.no_snapshot_on_revert",
                         "RemoveHardwareSpec revert without apply"};
    }
    project.mutable_hardware_catalog()[target_] = *snapshot_;
    return ChangeSet{};
}

}  // namespace coupecad::core
```

- [ ] **Step 3: Обновить `src/coupecad/core/CMakeLists.txt`.**

- [ ] **Step 4: Тесты — 6 кейсов по аналогии с material_commands_test.**

```cpp
#include "coupecad/core/commands/hardware_spec_commands.h"
#include "coupecad/core/commands/hardware_commands.h"
#include "coupecad/core/commands/panel_commands.h"
#include "coupecad/core/errors.h"
#include "coupecad/core/project.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

namespace {
HardwareSpec make_spec(std::string_view ref = "test.spec") {
    return HardwareSpec{.ref = HardwareRef{std::string{ref}},
                         .kind = HardwareKind::Hinge,
                         .name = "Test",
                         .bbox = Vec3{Millimeters{10}, Millimeters{10},
                                       Millimeters{10}}};
}
}  // namespace

TEST(AddHardwareSpec, ApplyRevert) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(5));
    AddHardwareSpec cmd{make_spec()};
    cmd.apply(p);
    EXPECT_EQ(p.hardware_catalog().size(), 1u);
    cmd.revert(p);
    EXPECT_EQ(p.hardware_catalog().size(), 0u);
}

TEST(AddHardwareSpec, DuplicateRefThrows) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(5));
    AddHardwareSpec a{make_spec("dup")};
    a.apply(p);
    AddHardwareSpec b{make_spec("dup")};
    EXPECT_THROW(b.apply(p), DomainError);
}

TEST(UpdateHardwareSpec, ApplyRevert) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(5));
    AddHardwareSpec add{make_spec("u1")};
    add.apply(p);
    HardwareSpec patched = make_spec("u1");
    patched.name = "Updated name";
    UpdateHardwareSpec upd{HardwareRef{"u1"}, patched};
    upd.apply(p);
    EXPECT_EQ(p.hardware_catalog().at(HardwareRef{"u1"}).name, "Updated name");
    upd.revert(p);
    EXPECT_EQ(p.hardware_catalog().at(HardwareRef{"u1"}).name, "Test");
}

TEST(RemoveHardwareSpec, ApplyRevert) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(5));
    AddHardwareSpec add{make_spec("r1")};
    add.apply(p);
    RemoveHardwareSpec rm{HardwareRef{"r1"}};
    rm.apply(p);
    EXPECT_EQ(p.hardware_catalog().count(HardwareRef{"r1"}), 0u);
    rm.revert(p);
    EXPECT_EQ(p.hardware_catalog().count(HardwareRef{"r1"}), 1u);
}

TEST(RemoveHardwareSpec, BlockedWhenInUse) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(5));
    AddPanel add_pan{PanelRole::Top, NoRoleParams{}};
    add_pan.apply(p);
    AddHardwareSpec add_spec{make_spec("in_use")};
    add_spec.apply(p);
    AddHardware add_hw{HardwareRef{"in_use"},
                        {PanelAttachment{.panel_id = add_pan.assigned_id(),
                                          .local_position = Vec3{},
                                          .orientation = Quat::identity()}}};
    add_hw.apply(p);

    RemoveHardwareSpec rm{HardwareRef{"in_use"}};
    EXPECT_THROW(rm.apply(p), DomainError);
}
```

- [ ] **Step 5: Обновить tests CMakeLists.**

- [ ] **Step 6: Build + test — 5 новых HardwareSpec-тестов.**

- [ ] **Step 7: Commit.**

```sh
git commit -am "feat(core/commands): Add/Update/RemoveHardwareSpec with reference checks"
```

---

## Task 8: MacroCommand

**Files:**
- Create: `src/coupecad/core/commands/macro_command.h`
- Create: `src/coupecad/core/commands/macro_command.cpp`
- Modify: `src/coupecad/core/CMakeLists.txt`
- Create: `tests/core/commands/macro_command_test.cpp`
- Modify: `tests/core/commands/CMakeLists.txt`

- [ ] **Step 1: `macro_command.h`.**

```cpp
#pragma once

#include "coupecad/core/commands/command.h"

#include <memory>
#include <string>
#include <vector>

namespace coupecad::core {

// Composite-команда: список других команд, применяется и откатывается
// атомарно. Label задаётся при конструировании.
class MacroCommand : public Command {
public:
    explicit MacroCommand(std::string label);

    // Добавить под-команду. Выполняется только через apply() макро-команды.
    void push(std::unique_ptr<Command> cmd);

    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return label_; }
    CommandKind kind() const noexcept override { return CommandKind::Macro; }

    bool empty() const noexcept { return children_.empty(); }
    std::size_t size() const noexcept { return children_.size(); }

private:
    std::string label_;
    std::vector<std::unique_ptr<Command>> children_;
    bool applied_ = false;
};

}  // namespace coupecad::core
```

- [ ] **Step 2: `macro_command.cpp`.**

```cpp
#include "coupecad/core/commands/macro_command.h"

#include "coupecad/core/errors.h"
#include "coupecad/core/project.h"

namespace coupecad::core {

MacroCommand::MacroCommand(std::string label) : label_(std::move(label)) {}

void MacroCommand::push(std::unique_ptr<Command> cmd) {
    if (applied_) {
        throw LogicError{"macro.push_after_apply",
                         "Cannot push into an already-applied MacroCommand"};
    }
    children_.push_back(std::move(cmd));
}

ChangeSet MacroCommand::apply(Project& project) {
    ChangeSet acc;
    std::size_t done = 0;
    try {
        for (; done < children_.size(); ++done) {
            acc.merge(children_[done]->apply(project));
        }
    } catch (...) {
        // Откатить уже применённые под-команды в обратном порядке.
        while (done > 0) {
            --done;
            try { children_[done]->revert(project); } catch (...) {}
        }
        throw;
    }
    applied_ = true;
    return acc;
}

ChangeSet MacroCommand::revert(Project& project) {
    ChangeSet acc;
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        acc.merge((*it)->revert(project));
    }
    return acc;
}

}  // namespace coupecad::core
```

- [ ] **Step 3: CMakeLists update.**

- [ ] **Step 4: Тесты.**

```cpp
#include "coupecad/core/commands/macro_command.h"
#include "coupecad/core/commands/panel_commands.h"
#include "coupecad/core/errors.h"
#include "coupecad/core/project.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

TEST(MacroCommand, AppliesChildrenInOrder) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(6));
    MacroCommand mc{"Add three panels"};
    mc.push(std::make_unique<AddPanel>(PanelRole::Top, NoRoleParams{}));
    mc.push(std::make_unique<AddPanel>(PanelRole::Bottom, NoRoleParams{}));
    mc.push(std::make_unique<AddPanel>(PanelRole::Back, NoRoleParams{}));
    auto cs = mc.apply(p);
    EXPECT_EQ(p.cabinet().panels.size(), 3u);
    EXPECT_EQ(cs.added_panels.size(), 3u);
}

TEST(MacroCommand, RevertsChildrenInReverseOrder) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(6));
    MacroCommand mc{"x"};
    mc.push(std::make_unique<AddPanel>(PanelRole::Top, NoRoleParams{}));
    mc.push(std::make_unique<AddPanel>(PanelRole::Bottom, NoRoleParams{}));
    mc.apply(p);
    mc.revert(p);
    EXPECT_EQ(p.cabinet().panels.size(), 0u);
}

TEST(MacroCommand, FailureInMiddleRollsBack) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(6));
    MacroCommand mc{"x"};
    mc.push(std::make_unique<AddPanel>(PanelRole::Top, NoRoleParams{}));
    // Вторая команда заведомо упадёт (role/role_params mismatch внутри apply):
    // нельзя её сконструировать — поэтому используем RemovePanel с invalid id,
    // чтобы apply бросил.
    mc.push(std::make_unique<RemovePanel>(PanelId{}));
    EXPECT_THROW(mc.apply(p), DomainError);
    EXPECT_EQ(p.cabinet().panels.size(), 0u);   // откатили первую
}

TEST(MacroCommand, PushAfterApplyThrows) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(6));
    MacroCommand mc{"x"};
    mc.push(std::make_unique<AddPanel>(PanelRole::Top, NoRoleParams{}));
    mc.apply(p);
    EXPECT_THROW(mc.push(std::make_unique<AddPanel>(PanelRole::Back, NoRoleParams{})),
                 LogicError);
}
```

- [ ] **Step 5: tests CMakeLists update.**

- [ ] **Step 6: Build + test.**

```sh
ctest --preset default -R MacroCommand --output-on-failure
```

Ожидается: 4 теста passed.

- [ ] **Step 7: Commit.**

```sh
git commit -am "feat(core/commands): MacroCommand with all-or-nothing semantics"
```

---

## Task 9: UndoStack — discrete (execute/undo/redo/observers)

**Files:**
- Create: `src/coupecad/core/undo_stack.h`
- Create: `src/coupecad/core/undo_stack.cpp`
- Modify: `src/coupecad/core/CMakeLists.txt`
- Create: `tests/core/undo_stack_test.cpp`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1: `undo_stack.h`** (только discrete-режим; preview добавится в Task 12).

```cpp
#pragma once

#include "coupecad/core/commands/change_set.h"
#include "coupecad/core/commands/command.h"
#include "coupecad/core/project.h"

#include <memory>
#include <string_view>
#include <vector>

namespace coupecad::core {

class MacroCommand;

class UndoStack {
public:
    explicit UndoStack(Project& project);
    ~UndoStack();

    UndoStack(const UndoStack&) = delete;
    UndoStack& operator=(const UndoStack&) = delete;

    // Discrete execute.
    void execute(std::unique_ptr<Command> cmd);

    void undo();
    void redo();
    bool can_undo() const noexcept { return !undo_.empty(); }
    bool can_redo() const noexcept { return !redo_.empty(); }
    std::size_t undo_depth() const noexcept { return undo_.size(); }
    std::size_t redo_depth() const noexcept { return redo_.size(); }

    // Labels текущих команд (для UI).
    std::vector<std::string_view> undo_labels() const;

    void clear() noexcept;

    // Макросы — начаты в Task 10.
    void begin_macro(std::string label);
    void end_macro();

    // Наблюдатели.
    void add_observer(IProjectObserver* obs);
    void remove_observer(IProjectObserver* obs);

private:
    Project& project_;
    std::vector<std::unique_ptr<Command>> undo_;
    std::vector<std::unique_ptr<Command>> redo_;
    std::unique_ptr<MacroCommand> pending_macro_;
    std::vector<IProjectObserver*> observers_;

    void notify(const ChangeSet& cs);
};

}  // namespace coupecad::core
```

- [ ] **Step 2: `undo_stack.cpp`** — пока без макросов и preview.

```cpp
#include "coupecad/core/undo_stack.h"

#include "coupecad/core/commands/macro_command.h"
#include "coupecad/core/errors.h"

namespace coupecad::core {

UndoStack::UndoStack(Project& project) : project_(project) {}
UndoStack::~UndoStack() = default;

void UndoStack::execute(std::unique_ptr<Command> cmd) {
    if (pending_macro_) {
        auto cs = cmd->apply(project_);
        pending_macro_->push(std::move(cmd));
        notify(cs);
        return;
    }
    auto cs = cmd->apply(project_);
    undo_.push_back(std::move(cmd));
    redo_.clear();
    notify(cs);
}

void UndoStack::undo() {
    if (undo_.empty()) return;
    auto cmd = std::move(undo_.back());
    undo_.pop_back();
    auto cs = cmd->revert(project_);
    redo_.push_back(std::move(cmd));
    notify(cs);
}

void UndoStack::redo() {
    if (redo_.empty()) return;
    auto cmd = std::move(redo_.back());
    redo_.pop_back();
    auto cs = cmd->apply(project_);
    undo_.push_back(std::move(cmd));
    notify(cs);
}

std::vector<std::string_view> UndoStack::undo_labels() const {
    std::vector<std::string_view> out;
    out.reserve(undo_.size());
    for (const auto& c : undo_) out.push_back(c->label());
    return out;
}

void UndoStack::clear() noexcept {
    undo_.clear();
    redo_.clear();
    pending_macro_.reset();
}

void UndoStack::begin_macro(std::string label) {
    if (pending_macro_) {
        throw LogicError{"undo.macro_already_active",
                         "begin_macro called while macro already active"};
    }
    pending_macro_ = std::make_unique<MacroCommand>(std::move(label));
}

void UndoStack::end_macro() {
    if (!pending_macro_) {
        throw LogicError{"undo.no_macro_to_end",
                         "end_macro without begin_macro"};
    }
    if (!pending_macro_->empty()) {
        undo_.push_back(std::move(pending_macro_));
        redo_.clear();
    } else {
        pending_macro_.reset();
    }
}

void UndoStack::add_observer(IProjectObserver* o) {
    observers_.push_back(o);
}

void UndoStack::remove_observer(IProjectObserver* o) {
    observers_.erase(std::remove(observers_.begin(), observers_.end(), o),
                     observers_.end());
}

void UndoStack::notify(const ChangeSet& cs) {
    for (auto* o : observers_) {
        o->on_changed(project_, cs);
    }
}

}  // namespace coupecad::core
```

Добавить include `<algorithm>` для `std::remove` в верх файла.

- [ ] **Step 3: CMakeLists update** — добавить `undo_stack.cpp`.

- [ ] **Step 4: `tests/core/undo_stack_test.cpp`** (discrete tests; preview — в Task 12).

```cpp
#include "coupecad/core/commands/panel_commands.h"
#include "coupecad/core/project.h"
#include "coupecad/core/undo_stack.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

namespace {
class RecordingObserver : public IProjectObserver {
public:
    std::vector<ChangeSet> changes;
    void on_changed(const Project&, const ChangeSet& cs) override {
        changes.push_back(cs);
    }
};
}  // namespace

TEST(UndoStack, ExecutePushesAndNotifies) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(7));
    UndoStack us{p};
    RecordingObserver obs;
    us.add_observer(&obs);
    us.execute(std::make_unique<AddPanel>(PanelRole::Top, NoRoleParams{}));
    EXPECT_TRUE(us.can_undo());
    EXPECT_FALSE(us.can_redo());
    ASSERT_EQ(obs.changes.size(), 1u);
    EXPECT_EQ(obs.changes[0].added_panels.size(), 1u);
}

TEST(UndoStack, UndoRedoRoundTrip) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(7));
    UndoStack us{p};
    us.execute(std::make_unique<AddPanel>(PanelRole::Top, NoRoleParams{}));
    us.execute(std::make_unique<AddPanel>(PanelRole::Bottom, NoRoleParams{}));
    EXPECT_EQ(p.cabinet().panels.size(), 2u);
    us.undo();
    EXPECT_EQ(p.cabinet().panels.size(), 1u);
    us.undo();
    EXPECT_EQ(p.cabinet().panels.size(), 0u);
    us.redo();
    EXPECT_EQ(p.cabinet().panels.size(), 1u);
    us.redo();
    EXPECT_EQ(p.cabinet().panels.size(), 2u);
}

TEST(UndoStack, NewExecuteClearsRedoStack) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(7));
    UndoStack us{p};
    us.execute(std::make_unique<AddPanel>(PanelRole::Top, NoRoleParams{}));
    us.undo();
    EXPECT_TRUE(us.can_redo());
    us.execute(std::make_unique<AddPanel>(PanelRole::Bottom, NoRoleParams{}));
    EXPECT_FALSE(us.can_redo());
}

TEST(UndoStack, UndoAtEmptyIsNoOp) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(7));
    UndoStack us{p};
    us.undo();   // не бросает
    EXPECT_FALSE(us.can_undo());
}

TEST(UndoStack, ClearResetsStacks) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(7));
    UndoStack us{p};
    us.execute(std::make_unique<AddPanel>(PanelRole::Top, NoRoleParams{}));
    us.clear();
    EXPECT_FALSE(us.can_undo());
    EXPECT_FALSE(us.can_redo());
}

TEST(UndoStack, RemoveObserver) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(7));
    UndoStack us{p};
    RecordingObserver obs;
    us.add_observer(&obs);
    us.remove_observer(&obs);
    us.execute(std::make_unique<AddPanel>(PanelRole::Top, NoRoleParams{}));
    EXPECT_TRUE(obs.changes.empty());
}
```

- [ ] **Step 5: tests CMakeLists update.**

Добавить `undo_stack_test.cpp` в `coupecad_core_test` в `tests/core/CMakeLists.txt`.

- [ ] **Step 6: Build + test.**

```sh
cmake --build --preset default --target coupecad_core_test
ctest --preset default -R "UndoStack" --output-on-failure
```

Ожидается: 6 тестов passed.

- [ ] **Step 7: Commit.**

```sh
git commit -am "feat(core/undo): UndoStack discrete mode + observer hookup"
```

---

## Task 10: UndoStack — macros

Поскольку `begin_macro/end_macro` и взаимодействие `execute` с active macro уже реализованы в Task 9 (см. код `undo_stack.cpp`), здесь — **только добавление тестов**.

**Files:**
- Create: `tests/core/undo_stack_macro_test.cpp`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1: Создать `tests/core/undo_stack_macro_test.cpp`.**

```cpp
#include "coupecad/core/commands/panel_commands.h"
#include "coupecad/core/errors.h"
#include "coupecad/core/project.h"
#include "coupecad/core/undo_stack.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

TEST(UndoStackMacro, GroupsExecutesIntoOneEntry) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(8));
    UndoStack us{p};
    us.begin_macro("Add pair");
    us.execute(std::make_unique<AddPanel>(PanelRole::Top, NoRoleParams{}));
    us.execute(std::make_unique<AddPanel>(PanelRole::Bottom, NoRoleParams{}));
    us.end_macro();
    EXPECT_EQ(us.undo_depth(), 1u);
    EXPECT_EQ(p.cabinet().panels.size(), 2u);
    us.undo();
    EXPECT_EQ(p.cabinet().panels.size(), 0u);
    us.redo();
    EXPECT_EQ(p.cabinet().panels.size(), 2u);
}

TEST(UndoStackMacro, EmptyMacroAddsNothing) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(8));
    UndoStack us{p};
    us.begin_macro("Nop");
    us.end_macro();
    EXPECT_EQ(us.undo_depth(), 0u);
}

TEST(UndoStackMacro, NestedBeginThrows) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(8));
    UndoStack us{p};
    us.begin_macro("A");
    EXPECT_THROW(us.begin_macro("B"), LogicError);
    us.end_macro();
}

TEST(UndoStackMacro, EndWithoutBeginThrows) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(8));
    UndoStack us{p};
    EXPECT_THROW(us.end_macro(), LogicError);
}
```

- [ ] **Step 2: Обновить `tests/core/CMakeLists.txt`** — добавить `undo_stack_macro_test.cpp`.

- [ ] **Step 3: Build + test.**

```sh
ctest --preset default -R "UndoStackMacro" --output-on-failure
```

Ожидается: 4 теста passed.

- [ ] **Step 4: Commit.**

```sh
git commit -am "test(core/undo): macro begin/end behavior"
```

---

## Task 11: UndoStack — preview/commit

**Files:**
- Modify: `src/coupecad/core/undo_stack.h` (добавить preview API)
- Modify: `src/coupecad/core/undo_stack.cpp` (реализовать)
- Create: `tests/core/undo_stack_preview_test.cpp`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1: Дополнить `undo_stack.h`** — добавить preview API перед `private:`:

```cpp
    // --- Live preview (см. spec §3.3.2) ---
    class PreviewHandle {
    public:
        PreviewHandle() = default;
        bool is_active() const noexcept { return active_; }
    private:
        friend class UndoStack;
        bool active_ = false;
    };

    PreviewHandle begin_preview(std::unique_ptr<PreviewableCommand> cmd);
    void update_preview(PreviewHandle& handle, const std::any& new_value);
    void commit_preview(PreviewHandle& handle);
    void cancel_preview(PreviewHandle& handle);

    bool preview_active() const noexcept { return active_preview_ != nullptr; }
```

И добавить в private-секцию:

```cpp
    std::unique_ptr<PreviewableCommand> active_preview_;
```

Добавить `#include <any>` в верх заголовка.

- [ ] **Step 2: Дополнить `undo_stack.cpp`** — реализации preview-методов и блокировки execute во время preview:

```cpp
UndoStack::PreviewHandle UndoStack::begin_preview(
    std::unique_ptr<PreviewableCommand> cmd) {
    if (pending_macro_) {
        throw LogicError{"undo.preview_inside_macro",
                         "Live preview not allowed inside a macro"};
    }
    if (active_preview_) {
        throw LogicError{"undo.preview_already_active",
                         "Another preview session is already active"};
    }
    auto cs = cmd->apply(project_);
    active_preview_ = std::move(cmd);
    PreviewHandle h;
    h.active_ = true;
    notify(cs);
    return h;
}

void UndoStack::update_preview(PreviewHandle& h, const std::any& v) {
    if (!h.active_ || !active_preview_) {
        throw LogicError{"undo.preview_not_active",
                         "update_preview without active session"};
    }
    auto cs = active_preview_->update(project_, v);
    notify(cs);
}

void UndoStack::commit_preview(PreviewHandle& h) {
    if (!h.active_ || !active_preview_) {
        throw LogicError{"undo.preview_not_active",
                         "commit_preview without active session"};
    }
    // Команда уже применена в initial→current. Кладём её как Discrete-запись.
    undo_.push_back(std::move(active_preview_));
    redo_.clear();
    h.active_ = false;
}

void UndoStack::cancel_preview(PreviewHandle& h) {
    if (!h.active_ || !active_preview_) {
        throw LogicError{"undo.preview_not_active",
                         "cancel_preview without active session"};
    }
    auto cs = active_preview_->revert(project_);
    active_preview_.reset();
    h.active_ = false;
    notify(cs);
}
```

И обновить `execute` — запрещать вызов во время активного preview, добавить в самое начало метода:

```cpp
void UndoStack::execute(std::unique_ptr<Command> cmd) {
    if (active_preview_) {
        throw LogicError{"undo.execute_during_preview",
                         "execute() not allowed while preview is active"};
    }
    // ... (остальное без изменений)
}
```

Также запретить `begin_macro` во время preview, в начало метода:

```cpp
    if (active_preview_) {
        throw LogicError{"undo.macro_during_preview",
                         "begin_macro not allowed while preview is active"};
    }
```

И `clear()` обнуляет active_preview_:

```cpp
void UndoStack::clear() noexcept {
    undo_.clear();
    redo_.clear();
    pending_macro_.reset();
    active_preview_.reset();
}
```

- [ ] **Step 3: `tests/core/undo_stack_preview_test.cpp`.**

```cpp
#include "coupecad/core/commands/cabinet_commands.h"
#include "coupecad/core/commands/panel_commands.h"
#include "coupecad/core/errors.h"
#include "coupecad/core/project.h"
#include "coupecad/core/undo_stack.h"

#include <gtest/gtest.h>

using namespace coupecad::core;

TEST(UndoStackPreview, CommitLandsAsSingleEntry) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(9));
    UndoStack us{p};
    auto cmd = std::make_unique<SetCabinetDimensions>(
        p.cabinet().id,
        Dimensions{Millimeters{2500}, Millimeters{700}, Millimeters{2500}});
    auto h = us.begin_preview(std::move(cmd));
    us.update_preview(h, std::any{Dimensions{Millimeters{2600},
                                               Millimeters{700},
                                               Millimeters{2500}}});
    us.update_preview(h, std::any{Dimensions{Millimeters{2700},
                                               Millimeters{700},
                                               Millimeters{2500}}});
    us.commit_preview(h);
    EXPECT_EQ(us.undo_depth(), 1u);
    EXPECT_EQ(p.cabinet().dimensions.width, Millimeters{2700});
    us.undo();
    EXPECT_EQ(p.cabinet().dimensions.width, Millimeters{2400});   // исходные
}

TEST(UndoStackPreview, CancelLeavesStackEmpty) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(9));
    UndoStack us{p};
    auto cmd = std::make_unique<SetCabinetDimensions>(
        p.cabinet().id,
        Dimensions{Millimeters{3000}, Millimeters{600}, Millimeters{2400}});
    auto h = us.begin_preview(std::move(cmd));
    us.update_preview(h, std::any{Dimensions{Millimeters{3500},
                                               Millimeters{600},
                                               Millimeters{2400}}});
    us.cancel_preview(h);
    EXPECT_EQ(us.undo_depth(), 0u);
    EXPECT_EQ(p.cabinet().dimensions.width, Millimeters{2400});
    EXPECT_FALSE(h.is_active());
}

TEST(UndoStackPreview, NestedBeginThrows) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(9));
    UndoStack us{p};
    auto h = us.begin_preview(std::make_unique<SetCabinetDimensions>(
        p.cabinet().id, p.cabinet().dimensions));
    EXPECT_THROW(us.begin_preview(std::make_unique<SetCabinetDimensions>(
                      p.cabinet().id, p.cabinet().dimensions)),
                 LogicError);
    us.cancel_preview(h);
}

TEST(UndoStackPreview, ExecuteBlockedDuringPreview) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(9));
    UndoStack us{p};
    auto h = us.begin_preview(std::make_unique<SetCabinetDimensions>(
        p.cabinet().id, p.cabinet().dimensions));
    EXPECT_THROW(us.execute(std::make_unique<AddPanel>(PanelRole::Top,
                                                         NoRoleParams{})),
                 LogicError);
    us.cancel_preview(h);
}

TEST(UndoStackPreview, MacroBlockedDuringPreview) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(9));
    UndoStack us{p};
    auto h = us.begin_preview(std::make_unique<SetCabinetDimensions>(
        p.cabinet().id, p.cabinet().dimensions));
    EXPECT_THROW(us.begin_macro("x"), LogicError);
    us.cancel_preview(h);
}

TEST(UndoStackPreview, PreviewInsideMacroThrows) {
    auto p = Project::create_empty("T", make_seeded_uuid_generator(9));
    UndoStack us{p};
    us.begin_macro("m");
    EXPECT_THROW(us.begin_preview(std::make_unique<SetCabinetDimensions>(
                      p.cabinet().id, p.cabinet().dimensions)),
                 LogicError);
    us.end_macro();
}
```

- [ ] **Step 4: tests CMakeLists update.**

- [ ] **Step 5: Build + test.**

```sh
ctest --preset default -R "UndoStackPreview" --output-on-failure
```

Ожидается: 6 тестов passed.

- [ ] **Step 6: Commit.**

```sh
git commit -am "feat(core/undo): preview/commit live-edit mode"
```

---

## Task 12: Финальный прогон + push + CI

**Files:** никаких новых.

- [ ] **Step 1: Полный прогон локально.**

```sh
cmake --build --preset default
ctest --preset default --output-on-failure
```

Ожидается: все тесты passed (Logger + Core units/id/material/hardware/panel/cabinet/project/geometry + Commands/Undo ≈ 100+ кейсов).

- [ ] **Step 2: Push.**

```sh
git push
```

- [ ] **Step 3: Дождаться CI зелёного.**

```sh
gh run list --limit 1 --workflow=CI
gh run watch
```

Ожидается: Linux + Windows зелёные. macOS закомментирован (из Stage 0).

- [ ] **Step 4: Финальная проверка DoD.**

- [ ] Все команды из §3.2 спеки реализованы и покрыты тестами.
- [ ] UndoStack discrete + preview + macro работают.
- [ ] Observer получает ChangeSet после каждой операции.
- [ ] Активный preview — только один в моменте, execute/macro блокируются.
- [ ] Валидация атомарна (macro-rollback при middle-failure проверен).
- [ ] CI зелёный.

После этого Stage 1 почти закончен — остаётся Stage 1c (`.ccad` I/O): ZIP + JSON + миграции. Отдельный план.
