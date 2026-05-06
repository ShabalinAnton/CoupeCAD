# Stage 3 — Renderer (OCCT) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Реализовать `IRenderer` интерфейс и его OCCT-бэкенд (`coupecad_renderer_occt`), который потребляет геометрию из Stage 2 `GeometryBuilder`, поддерживает ChangeSet-инкрементальный sync, камеру, picking, selection, цвет панелей из `Material.color_hint`, и работает в headless-режиме на CI без GL.

**Architecture:** Две CMake-цели: `coupecad_renderer` (статическая, экспортирует `IRenderer` + `EntityId`, без OCCT в публичных хедерах) и `coupecad_renderer_occt` (статическая, использует OCCT Visualization). Внутренние компоненты бэкенда — `view_driver` (V3d/driver/headless-fallback), `ais_scene` (AIS_InteractiveContext + словари id↔handle), `material_resolver` (Material→Quantity_Color), `OcctRenderer` (фасад, реализующий `IRenderer`).

**Tech Stack:** C++20, OpenCASCADE 7.9.1 (Visualization: TKV3d, TKOpenGl, TKService, TKHLR, TKMeshVS), `coupecad_core` (Project, ChangeSet, ids, errors), `coupecad_geometry` (GeometryBuilder), `coupecad_logging`, GoogleTest. CMake селектор бэкенда `COUPECAD_RENDERER`.

**Связанные документы:**
- Дизайн: [`../specs/2026-05-04-stage-3-renderer-design.md`](../specs/2026-05-04-stage-3-renderer-design.md)
- Stage 2 геометрия: [`../specs/2026-04-22-stage-2-geometry-design.md`](../specs/2026-04-22-stage-2-geometry-design.md)

**Definition of Done (см. spec §11):** все бинарники собраны на Linux + Windows CI; все методы `IRenderer` покрыты тестами; sync корректно отслеживает каждое поле ChangeSet; picking возвращает корректный `EntityId`; цвет берётся из effective `Material.color_hint`; логирование по категории `"renderer"`; coverage `coupecad_renderer_occt` ≥ 60% lines; не сломаны 249 тестов Stage 2; нет линкажа Qt; `COUPECAD_RENDERER=qq3d` даёт чистую CMake-ошибку; headless-фолбэк работает.

---

## File Structure

После Stage 3:

```
src/coupecad/renderer/
    CMakeLists.txt              # селектор COUPECAD_RENDERER + interface lib
    i_renderer.h                # PUBLIC: интерфейс
    entity_id.h                 # variant<PanelId, HardwareItemId> + std::hash
    entity_id.cpp               # std::hash<EntityId> impl
    occt/
        CMakeLists.txt
        occt_renderer.h         # PUBLIC: класс OcctRenderer + make_occt_renderer
        occt_renderer.cpp
        ais_scene.h             # internal
        ais_scene.cpp
        material_resolver.h     # internal
        material_resolver.cpp
        view_driver.h           # internal
        view_driver.cpp

tests/renderer/
    CMakeLists.txt
    entity_id_test.cpp
    view_driver_test.cpp
    material_resolver_test.cpp
    ais_scene_test.cpp
    occt_renderer_sync_test.cpp
    occt_renderer_camera_test.cpp
    occt_renderer_selection_test.cpp
    occt_renderer_picking_test.cpp
    occt_renderer_render_smoke_test.cpp
```

Модифицируется:
- `src/CMakeLists.txt` — добавить `add_subdirectory(coupecad/renderer)`
- `tests/CMakeLists.txt` — добавить `add_subdirectory(renderer)`

---

## Task 1: CMake skeleton + COUPECAD_RENDERER selector

**Files:**
- Create: `src/coupecad/renderer/CMakeLists.txt`
- Create: `src/coupecad/renderer/occt/CMakeLists.txt` (заглушка, просто `message(STATUS ...)`)
- Modify: `src/CMakeLists.txt`

- [ ] **Step 1: Создать `src/coupecad/renderer/CMakeLists.txt` — селектор + interface skeleton**

```cmake
# coupecad_renderer — общая часть, без OCCT.
# В Task 1 — INTERFACE (нет ещё source-файлов). В Task 2 переделаем
# в STATIC, когда добавится entity_id.cpp.
add_library(coupecad_renderer INTERFACE)
target_include_directories(coupecad_renderer INTERFACE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(coupecad_renderer INTERFACE coupecad_core)
target_compile_features(coupecad_renderer INTERFACE cxx_std_20)

# Селектор бэкенда. По умолчанию "occt"; объявлен в корневом CMakeLists.
if(COUPECAD_RENDERER STREQUAL "occt")
    add_subdirectory(occt)
elseif(COUPECAD_RENDERER STREQUAL "qq3d")
    message(FATAL_ERROR
        "COUPECAD_RENDERER=qq3d backend lands in Stage 11; "
        "use 'occt' for now.")
else()
    message(FATAL_ERROR "Unknown COUPECAD_RENDERER='${COUPECAD_RENDERER}' "
        "(expected 'occt' or 'qq3d').")
endif()
```

- [ ] **Step 2: Создать заглушку `src/coupecad/renderer/occt/CMakeLists.txt`**

```cmake
# Заполнится в Task 4. Пока — заглушка, чтобы add_subdirectory(occt) не падал.
message(STATUS "coupecad_renderer_occt: skeleton (Task 4 fills it in)")
```

- [ ] **Step 3: Подключить renderer в корневой `src/CMakeLists.txt`**

Открыть `src/CMakeLists.txt`, заменить целиком:

```cmake
add_subdirectory(coupecad/logging)
add_subdirectory(coupecad/core)
add_subdirectory(coupecad/geometry)
add_subdirectory(coupecad/renderer)
```

- [ ] **Step 4: Сконфигурировать и проверить**

Run:
```bash
conan install . --build=missing -s build_type=Debug
cmake --preset default
```

Expected: `Configuring done`, без ошибок. В логе видно `STATUS coupecad_renderer_occt: skeleton`.

- [ ] **Step 5: Проверить, что `qq3d` даёт чистую ошибку**

Run:
```bash
cmake --preset default -DCOUPECAD_RENDERER=qq3d
```

Expected: `FATAL_ERROR: COUPECAD_RENDERER=qq3d backend lands in Stage 11`. Откатить:
```bash
cmake --preset default -DCOUPECAD_RENDERER=occt
```

- [ ] **Step 6: Закоммитить**

```bash
git add src/coupecad/renderer/CMakeLists.txt src/coupecad/renderer/occt/CMakeLists.txt src/CMakeLists.txt
git commit -m "chore(stage-3): подключить coupecad/renderer + COUPECAD_RENDERER селектор"
```

---

## Task 2: EntityId + std::hash

**Files:**
- Create: `src/coupecad/renderer/entity_id.h`
- Create: `src/coupecad/renderer/entity_id.cpp`
- Create: `tests/renderer/CMakeLists.txt`
- Create: `tests/renderer/entity_id_test.cpp`
- Modify: `src/coupecad/renderer/CMakeLists.txt` (добавить `entity_id.cpp` в источники)
- Modify: `tests/CMakeLists.txt` (добавить `add_subdirectory(renderer)`)

- [ ] **Step 1: Написать падающий тест `tests/renderer/entity_id_test.cpp`**

```cpp
#include "coupecad/renderer/entity_id.h"

#include "coupecad/core/id.h"

#include <gtest/gtest.h>

#include <unordered_map>
#include <unordered_set>

using coupecad::core::HardwareItemId;
using coupecad::core::PanelId;
using coupecad::core::Id;
using coupecad::renderer::EntityId;

namespace {

PanelId make_panel_id(std::string_view s) {
    return PanelId::from_string(s);
}

HardwareItemId make_hw_id(std::string_view s) {
    return HardwareItemId::from_string(s);
}

}  // namespace

TEST(EntityIdTest, EqualityWithinSameAlternative) {
    const auto p1 = make_panel_id("11111111-1111-1111-1111-111111111111");
    const auto p2 = make_panel_id("11111111-1111-1111-1111-111111111111");
    EXPECT_EQ(EntityId{p1}, EntityId{p2});
}

TEST(EntityIdTest, InequalityAcrossAlternatives) {
    const auto p = make_panel_id("11111111-1111-1111-1111-111111111111");
    const auto h = make_hw_id("11111111-1111-1111-1111-111111111111");
    EXPECT_NE(EntityId{p}, EntityId{h});
}

TEST(EntityIdTest, UsableAsUnorderedKey) {
    std::unordered_set<EntityId> set;
    set.insert(EntityId{make_panel_id("11111111-1111-1111-1111-111111111111")});
    set.insert(EntityId{make_hw_id("22222222-2222-2222-2222-222222222222")});
    EXPECT_EQ(set.size(), 2u);
    EXPECT_TRUE(set.contains(
        EntityId{make_panel_id("11111111-1111-1111-1111-111111111111")}));
}
```

- [ ] **Step 2: Создать `src/coupecad/renderer/entity_id.h`**

```cpp
#pragma once

#include "coupecad/core/id.h"

#include <cstddef>
#include <variant>

namespace coupecad::renderer {

using EntityId = std::variant<core::PanelId, core::HardwareItemId>;

}  // namespace coupecad::renderer

namespace std {

template <>
struct hash<coupecad::renderer::EntityId> {
    std::size_t operator()(
        const coupecad::renderer::EntityId& id) const noexcept;
};

}  // namespace std
```

- [ ] **Step 3: Создать `src/coupecad/renderer/entity_id.cpp`**

```cpp
#include "coupecad/renderer/entity_id.h"

#include <variant>

namespace std {

std::size_t hash<coupecad::renderer::EntityId>::operator()(
    const coupecad::renderer::EntityId& id) const noexcept {
    // Включаем index() в хеш, чтобы PanelId{X} и HardwareItemId{X}
    // c одинаковыми UUID давали разные значения hash.
    const std::size_t base = std::visit(
        [](const auto& v) -> std::size_t {
            using T = std::decay_t<decltype(v)>;
            return std::hash<T>{}(v);
        },
        id);
    const std::size_t mix = id.index() * 0x9E3779B97F4A7C15ULL;
    return base ^ (mix + 0x9E3779B9 + (base << 6) + (base >> 2));
}

}  // namespace std
```

- [ ] **Step 4: Конвертировать `coupecad_renderer` в STATIC в `src/coupecad/renderer/CMakeLists.txt`**

Заменить блок `add_library(coupecad_renderer INTERFACE) ... target_compile_features(... INTERFACE ...)` на:

```cmake
add_library(coupecad_renderer STATIC
    entity_id.cpp
)
target_include_directories(coupecad_renderer PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(coupecad_renderer PUBLIC coupecad_core)
target_compile_features(coupecad_renderer PUBLIC cxx_std_20)
```

Селектор `if(COUPECAD_RENDERER ...) add_subdirectory(occt) ... endif()` остаётся ниже, без изменений.

- [ ] **Step 5: Создать `tests/renderer/CMakeLists.txt`**

```cmake
add_executable(coupecad_renderer_test
    entity_id_test.cpp
)

target_link_libraries(coupecad_renderer_test
    PRIVATE
        coupecad_renderer
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_renderer_test)
```

- [ ] **Step 6: Подключить в `tests/CMakeLists.txt`**

Заменить весь файл на:

```cmake
find_package(GTest REQUIRED)

add_subdirectory(smoke)
add_subdirectory(app)
add_subdirectory(logging)
add_subdirectory(core)
add_subdirectory(geometry)
add_subdirectory(renderer)
```

- [ ] **Step 7: Собрать и запустить тесты**

Run:
```bash
cmake --build --preset default --target coupecad_renderer_test
ctest --preset default -R EntityIdTest --output-on-failure
```

Expected: 3 tests, 3 passed.

- [ ] **Step 8: Запустить весь набор — Stage 2 не сломан**

Run:
```bash
ctest --preset default --output-on-failure
```

Expected: 252 tests passed (249 Stage 2 + 3 новых). Если что-то красное — исправить.

- [ ] **Step 9: Закоммитить**

```bash
git add src/coupecad/renderer/entity_id.h src/coupecad/renderer/entity_id.cpp \
        src/coupecad/renderer/CMakeLists.txt \
        tests/renderer/ tests/CMakeLists.txt
git commit -m "feat(renderer): EntityId variant + std::hash"
```

---

## Task 3: IRenderer интерфейс

**Files:**
- Create: `src/coupecad/renderer/i_renderer.h`

Чисто декларативный шаг: интерфейс без реализации, без тестов (тестируем через концретный бэкенд в следующих тасках).

- [ ] **Step 1: Создать `src/coupecad/renderer/i_renderer.h`**

```cpp
#pragma once

#include "coupecad/core/commands/change_set.h"
#include "coupecad/core/units.h"
#include "coupecad/renderer/entity_id.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace coupecad::renderer {

struct CameraState {
    core::Vec3  eye{};
    core::Vec3  target{};
    core::Vec3  up{core::Millimeters{0}, core::Millimeters{0}, core::Millimeters{1}};
    double      fov_deg = 45.0;   // 0 = orthographic, >0 = perspective
};

struct ViewportSize {
    int width  = 1;
    int height = 1;
};

// Stateful, single-threaded, не thread-safe. Хранит non-owning ссылки
// на Project и GeometryBuilder; оба должны пережить рендерер.
//
// Дизайн — см. docs/superpowers/specs/2026-05-04-stage-3-renderer-design.md
class IRenderer {
public:
    virtual ~IRenderer() = default;

    // §4 spec: ChangeSet-driven incremental sync. Тянет шейпы из
    // GeometryBuilder, переданного фабрикой.
    virtual void sync(const core::ChangeSet& cs) = 0;

    // Полная пересборка AIS-сцены с нуля. Вызывается после
    // GeometryBuilder::rebuild_all() (например, после deserialize).
    virtual void rebuild_all() = 0;

    // Камера.
    virtual void           set_camera(const CameraState& state) = 0;
    virtual CameraState    camera() const = 0;
    virtual void           fit_all() = 0;

    // Picking. Координаты — пиксели, origin = top-left.
    virtual std::optional<EntityId> pick(int x, int y) = 0;

    // Selection.
    virtual void                          select(const EntityId& id) = 0;
    virtual void                          deselect(const EntityId& id) = 0;
    virtual void                          clear_selection() = 0;
    virtual std::vector<EntityId>         selection() const = 0;

    // Viewport size.
    virtual void           set_viewport_size(ViewportSize size) = 0;
    virtual ViewportSize   viewport_size() const = 0;

    // Offscreen render → RGBA8, row-major, top-left origin.
    // Размер буфера = viewport_size().width * viewport_size().height * 4.
    // В headless-режиме без GL возвращает пустой vector + warning в лог.
    virtual std::vector<std::uint8_t> render_to_image() = 0;
};

}  // namespace coupecad::renderer
```

- [ ] **Step 2: Проверить, что заголовок компилируется**

Run:
```bash
cmake --build --preset default --target coupecad_renderer
```

Expected: builds without errors. Никаких изменений в исполняемых файлах не ожидается, поскольку header не подтягивается ничем.

- [ ] **Step 3: Закоммитить**

```bash
git add src/coupecad/renderer/i_renderer.h
git commit -m "feat(renderer): IRenderer interface + CameraState/ViewportSize"
```

---

## Task 4: view_driver — V3d/OpenGl + headless-fallback

**Files:**
- Create: `src/coupecad/renderer/occt/view_driver.h`
- Create: `src/coupecad/renderer/occt/view_driver.cpp`
- Create: `src/coupecad/renderer/occt/CMakeLists.txt` (заменяет заглушку из Task 1)
- Create: `tests/renderer/view_driver_test.cpp`
- Modify: `tests/renderer/CMakeLists.txt`

- [ ] **Step 1: Заменить `src/coupecad/renderer/occt/CMakeLists.txt`**

```cmake
add_library(coupecad_renderer_occt STATIC
    view_driver.cpp
)

target_link_libraries(coupecad_renderer_occt
    PUBLIC
        coupecad_renderer
        coupecad_geometry
        coupecad_logging
    PRIVATE
        # Visualization stack (поверх Stage 2 OCCT-таргетов).
        TKV3d
        TKOpenGl
        TKService
        TKHLR
        TKMeshVS
)

target_compile_features(coupecad_renderer_occt PUBLIC cxx_std_20)
```

- [ ] **Step 2: Написать падающий тест `tests/renderer/view_driver_test.cpp`**

```cpp
#include "coupecad/renderer/occt/view_driver.h"

#include <gtest/gtest.h>

namespace {

using coupecad::renderer::occt::ViewDriver;

}  // namespace

TEST(ViewDriverTest, ConstructDoesNotThrow) {
    EXPECT_NO_THROW({
        ViewDriver driver;
    });
}

TEST(ViewDriverTest, ViewerAndViewAccessible) {
    ViewDriver driver;
    EXPECT_FALSE(driver.viewer().IsNull());
    EXPECT_FALSE(driver.view().IsNull());
}

TEST(ViewDriverTest, GlAvailableFlagIsConsistent) {
    ViewDriver driver;
    // Не зависим от наличия GL: важно лишь, что флаг детерминирован
    // и не бросает.
    const bool gl1 = driver.gl_available();
    const bool gl2 = driver.gl_available();
    EXPECT_EQ(gl1, gl2);
}

TEST(ViewDriverTest, SetViewportSizeUpdatesNeutralWindow) {
    ViewDriver driver;
    driver.set_viewport_size(800, 600);
    EXPECT_EQ(driver.viewport_width(), 800);
    EXPECT_EQ(driver.viewport_height(), 600);
}
```

- [ ] **Step 3: Создать `src/coupecad/renderer/occt/view_driver.h`**

```cpp
#pragma once

#include <Aspect_NeutralWindow.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>

namespace coupecad::renderer::occt {

// Владеет OpenGl_GraphicDriver (lazy process-wide), V3d_Viewer, V3d_View
// и Aspect_NeutralWindow для headless-операций.
//
// Headless-fallback: если инициализация GL-драйвера падает (нет
// X-сервера / GL-context'а), V3d-объекты всё равно создаются (они не
// требуют GL для in-memory структур), а gl_available() возвращает
// false. См. spec §5.1.
//
// Не thread-safe. Один экземпляр на OcctRenderer.
class ViewDriver {
public:
    ViewDriver();
    ~ViewDriver();

    ViewDriver(const ViewDriver&) = delete;
    ViewDriver& operator=(const ViewDriver&) = delete;

    const Handle(V3d_Viewer)&           viewer() const noexcept { return viewer_; }
    const Handle(V3d_View)&             view() const noexcept { return view_; }
    const Handle(Aspect_NeutralWindow)& window() const noexcept { return window_; }

    bool gl_available() const noexcept { return gl_available_; }

    void set_viewport_size(int width, int height);
    int  viewport_width() const noexcept { return width_; }
    int  viewport_height() const noexcept { return height_; }

private:
    Handle(OpenGl_GraphicDriver) driver_;
    Handle(V3d_Viewer)           viewer_;
    Handle(V3d_View)             view_;
    Handle(Aspect_NeutralWindow) window_;
    bool                         gl_available_ = false;
    int                          width_  = 1;
    int                          height_ = 1;
};

}  // namespace coupecad::renderer::occt
```

- [ ] **Step 4: Создать `src/coupecad/renderer/occt/view_driver.cpp`**

```cpp
#include "coupecad/renderer/occt/view_driver.h"

#include "coupecad/logging/logger.h"

#include <Aspect_DisplayConnection.hxx>

#include <exception>

namespace coupecad::renderer::occt {

namespace {

Handle(OpenGl_GraphicDriver) try_make_gl_driver(bool& ok_out) {
    try {
        Handle(Aspect_DisplayConnection) display = new Aspect_DisplayConnection();
        Handle(OpenGl_GraphicDriver) drv = new OpenGl_GraphicDriver(display, false);
        // InitContext() требует windowing system; в headless-CI бросит.
        // Сохраняем исключение и переходим в logical mode.
        ok_out = true;
        return drv;
    } catch (const std::exception& e) {
        coupecad::logging::Logger::instance().warn(
            "renderer", "renderer.gl_unavailable: OpenGl_GraphicDriver init failed: {}",
            e.what());
        ok_out = false;
        return Handle(OpenGl_GraphicDriver){};
    } catch (...) {
        coupecad::logging::Logger::instance().warn(
            "renderer", "renderer.gl_unavailable: OpenGl_GraphicDriver init failed: <unknown>");
        ok_out = false;
        return Handle(OpenGl_GraphicDriver){};
    }
}

}  // namespace

ViewDriver::ViewDriver() {
    driver_ = try_make_gl_driver(gl_available_);

    // V3d_Viewer и V3d_View строятся даже если driver_ — нулевой handle:
    // OCCT поддерживает их как logical structures; рисование откажет
    // graceful'но в render_to_image, всё остальное (BVH, селекция,
    // позиции, AIS-граф) работает.
    viewer_ = new V3d_Viewer(driver_);
    viewer_->SetDefaultLights();
    viewer_->SetLightOn();

    view_ = viewer_->CreateView();

    // NeutralWindow без нативного хэндла — достаточно для projection-
    // математики picking'а. Размер 1×1 как стартовый.
    window_ = new Aspect_NeutralWindow();
    window_->SetSize(static_cast<Standard_Integer>(width_),
                     static_cast<Standard_Integer>(height_));
    view_->SetWindow(window_);

    // Z-up по умолчанию (см. spec §5.5).
    view_->SetUp(0.0, 0.0, 1.0);

    coupecad::logging::Logger::instance().info(
        "renderer", "ViewDriver constructed (gl_available={})", gl_available_);
}

ViewDriver::~ViewDriver() = default;

void ViewDriver::set_viewport_size(int width, int height) {
    width_  = width;
    height_ = height;
    if (!window_.IsNull()) {
        window_->SetSize(static_cast<Standard_Integer>(width),
                         static_cast<Standard_Integer>(height));
    }
    if (!view_.IsNull()) {
        view_->MustBeResized();
    }
}

}  // namespace coupecad::renderer::occt
```

- [ ] **Step 5: Подключить тест в `tests/renderer/CMakeLists.txt`**

Заменить файл целиком:

```cmake
add_executable(coupecad_renderer_test
    entity_id_test.cpp
    view_driver_test.cpp
)

target_link_libraries(coupecad_renderer_test
    PRIVATE
        coupecad_renderer
        coupecad_renderer_occt
        coupecad_logging
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_renderer_test)
```

- [ ] **Step 6: Сконфигурировать, собрать, запустить — ожидаем зелёный**

Run:
```bash
conan install . --build=missing -s build_type=Debug
cmake --preset default
cmake --build --preset default --target coupecad_renderer_test
ctest --preset default -R ViewDriverTest --output-on-failure
```

Expected: 4 теста, 4 passed. На CI без GL будет одна `warn`-строка `renderer.gl_unavailable` — ожидаемо, тест проходит.

- [ ] **Step 7: Полный прогон тестов**

Run:
```bash
ctest --preset default --output-on-failure
```

Expected: 256 tests passed (249 + 3 entity_id + 4 view_driver).

- [ ] **Step 8: Закоммитить**

```bash
git add src/coupecad/renderer/occt/CMakeLists.txt \
        src/coupecad/renderer/occt/view_driver.h \
        src/coupecad/renderer/occt/view_driver.cpp \
        tests/renderer/CMakeLists.txt tests/renderer/view_driver_test.cpp
git commit -m "feat(renderer): ViewDriver — V3d/OpenGl + headless-fallback"
```

---

## Task 5: material_resolver

**Files:**
- Create: `src/coupecad/renderer/occt/material_resolver.h`
- Create: `src/coupecad/renderer/occt/material_resolver.cpp`
- Create: `tests/renderer/material_resolver_test.cpp`
- Modify: `src/coupecad/renderer/occt/CMakeLists.txt`
- Modify: `tests/renderer/CMakeLists.txt`

- [ ] **Step 1: Написать падающий тест `tests/renderer/material_resolver_test.cpp`**

```cpp
#include "coupecad/renderer/occt/material_resolver.h"

#include "coupecad/core/cabinet.h"
#include "coupecad/core/material.h"
#include "coupecad/core/panel.h"
#include "coupecad/core/project.h"
#include "coupecad/core/units.h"

#include <Quantity_Color.hxx>
#include <gtest/gtest.h>

using coupecad::core::Cabinet;
using coupecad::core::Material;
using coupecad::core::MaterialId;
using coupecad::core::Project;
using coupecad::core::RGBA;
using coupecad::renderer::occt::resolve_panel_color;
using coupecad::renderer::occt::resolve_hardware_color;

namespace {

Project make_project_with_default_material(RGBA default_color) {
    auto project = Project::create_empty("test");
    auto& materials = project.mutable_materials();
    auto& cabinet = project.mutable_cabinet();
    // Project::create_empty уже создаёт материал; перекрашиваем его.
    materials.at(cabinet.default_panel_material).color_hint = default_color;
    return project;
}

Material make_material(MaterialId id, RGBA color) {
    Material m;
    m.id = id;
    m.name = "extra";
    m.color_hint = color;
    return m;
}

}  // namespace

TEST(MaterialResolverTest, PanelUsesCabinetDefaultWhenNoOverride) {
    auto project = make_project_with_default_material(RGBA{200, 100, 50, 255});

    coupecad::core::Panel panel{};
    panel.role = coupecad::core::PanelRole::Bottom;

    const Quantity_Color color = resolve_panel_color(project, panel);
    EXPECT_NEAR(color.Red(),   200.0 / 255.0, 1e-6);
    EXPECT_NEAR(color.Green(), 100.0 / 255.0, 1e-6);
    EXPECT_NEAR(color.Blue(),   50.0 / 255.0, 1e-6);
}

TEST(MaterialResolverTest, PanelUsesOverrideWhenSet) {
    auto project = make_project_with_default_material(RGBA{0, 0, 0, 255});
    auto extra_id = project.uuid_gen().next_id<coupecad::core::MaterialIdTag>();
    auto extra = make_material(extra_id, RGBA{10, 20, 30, 255});
    project.mutable_materials().emplace(extra_id, extra);

    coupecad::core::Panel panel{};
    panel.role = coupecad::core::PanelRole::Bottom;
    panel.material_override = extra_id;

    const Quantity_Color color = resolve_panel_color(project, panel);
    EXPECT_NEAR(color.Red(),   10.0 / 255.0, 1e-6);
    EXPECT_NEAR(color.Green(), 20.0 / 255.0, 1e-6);
    EXPECT_NEAR(color.Blue(),  30.0 / 255.0, 1e-6);
}

TEST(MaterialResolverTest, MissingMaterialThrowsDomainError) {
    auto project = make_project_with_default_material(RGBA{0, 0, 0, 255});
    coupecad::core::Panel panel{};
    panel.role = coupecad::core::PanelRole::Bottom;
    panel.material_override =
        coupecad::core::MaterialId::from_string("ffffffff-ffff-ffff-ffff-ffffffffff00");

    try {
        (void)resolve_panel_color(project, panel);
        FAIL() << "expected DomainError";
    } catch (const coupecad::core::DomainError& e) {
        EXPECT_EQ(e.code(), "renderer.material_not_found");
    }
}

TEST(MaterialResolverTest, HardwareUsesFixedMetallicGray) {
    const Quantity_Color color = resolve_hardware_color();
    EXPECT_NEAR(color.Red(),   0.55, 1e-6);
    EXPECT_NEAR(color.Green(), 0.57, 1e-6);
    EXPECT_NEAR(color.Blue(),  0.60, 1e-6);
}
```

- [ ] **Step 2: Создать `src/coupecad/renderer/occt/material_resolver.h`**

```cpp
#pragma once

#include "coupecad/core/panel.h"
#include "coupecad/core/project.h"

#include <Quantity_Color.hxx>

namespace coupecad::renderer::occt {

// Возвращает Quantity_Color для панели по её эффективному материалу:
// material_override → cabinet.default_panel_material.
// Бросает core::DomainError{"renderer.material_not_found"}, если
// material id не найден в project.materials().
Quantity_Color resolve_panel_color(const core::Project& project,
                                   const core::Panel& panel);

// Фиксированный металлический серый (см. spec §4.4).
Quantity_Color resolve_hardware_color();

}  // namespace coupecad::renderer::occt
```

- [ ] **Step 3: Создать `src/coupecad/renderer/occt/material_resolver.cpp`**

```cpp
#include "coupecad/renderer/occt/material_resolver.h"

#include "coupecad/core/errors.h"

#include <Quantity_TypeOfColor.hxx>

namespace coupecad::renderer::occt {

namespace {

Quantity_Color rgba_to_color(const core::RGBA& rgba) {
    return Quantity_Color(static_cast<Standard_Real>(rgba.r) / 255.0,
                          static_cast<Standard_Real>(rgba.g) / 255.0,
                          static_cast<Standard_Real>(rgba.b) / 255.0,
                          Quantity_TOC_RGB);
}

}  // namespace

Quantity_Color resolve_panel_color(const core::Project& project,
                                   const core::Panel& panel) {
    const core::MaterialId effective_id =
        panel.material_override.value_or(project.cabinet().default_panel_material);

    const auto it = project.materials().find(effective_id);
    if (it == project.materials().end()) {
        throw core::DomainError{
            "renderer.material_not_found",
            "Panel " + panel.id.to_string() +
                " references missing material " + effective_id.to_string()};
    }
    return rgba_to_color(it->second.color_hint);
}

Quantity_Color resolve_hardware_color() {
    return Quantity_Color(0.55, 0.57, 0.60, Quantity_TOC_RGB);
}

}  // namespace coupecad::renderer::occt
```

- [ ] **Step 4: Добавить файл в `src/coupecad/renderer/occt/CMakeLists.txt`**

Заменить `add_library(coupecad_renderer_occt STATIC view_driver.cpp)` на:

```cmake
add_library(coupecad_renderer_occt STATIC
    view_driver.cpp
    material_resolver.cpp
)
```

- [ ] **Step 5: Подключить тест в `tests/renderer/CMakeLists.txt`**

Добавить `material_resolver_test.cpp` в список источников исполняемого `coupecad_renderer_test`.

- [ ] **Step 6: Собрать, запустить тесты**

Run:
```bash
cmake --build --preset default --target coupecad_renderer_test
ctest --preset default -R MaterialResolverTest --output-on-failure
```

Expected: 4 теста, 4 passed.

- [ ] **Step 7: Закоммитить**

```bash
git add src/coupecad/renderer/occt/material_resolver.h \
        src/coupecad/renderer/occt/material_resolver.cpp \
        src/coupecad/renderer/occt/CMakeLists.txt \
        tests/renderer/material_resolver_test.cpp \
        tests/renderer/CMakeLists.txt
git commit -m "feat(renderer): material_resolver — Material.color_hint → Quantity_Color"
```

---

## Task 6: ais_scene — добавление/удаление/замена панелей

**Files:**
- Create: `src/coupecad/renderer/occt/ais_scene.h`
- Create: `src/coupecad/renderer/occt/ais_scene.cpp`
- Create: `tests/renderer/ais_scene_test.cpp`
- Modify: `src/coupecad/renderer/occt/CMakeLists.txt`
- Modify: `tests/renderer/CMakeLists.txt`

> Этот таск ставит панельную часть `AisScene`. Hardware и material-refresh — Task 7 и Task 8.

- [ ] **Step 1: Написать падающий тест `tests/renderer/ais_scene_test.cpp`**

```cpp
#include "coupecad/renderer/occt/ais_scene.h"
#include "coupecad/renderer/occt/view_driver.h"

#include "coupecad/core/cabinet.h"
#include "coupecad/core/panel.h"
#include "coupecad/core/project.h"
#include "coupecad/geometry/geometry_builder.h"

#include <BRepPrimAPI_MakeBox.hxx>
#include <gtest/gtest.h>

using coupecad::core::Project;
using coupecad::geometry::GeometryBuilder;
using coupecad::renderer::occt::AisScene;
using coupecad::renderer::occt::ViewDriver;

namespace {

// Минимальный Project с одной панелью Bottom; геометрия подсчитывается
// через compute_panel_geometry (Stage 1a).
struct Fixture {
    Project   project = Project::create_empty("test");
    GeometryBuilder builder{project};
    ViewDriver      driver;
    AisScene        scene{driver, project};

    coupecad::core::PanelId add_bottom_panel() {
        auto& cabinet = project.mutable_cabinet();
        cabinet.dimensions = {coupecad::core::Millimeters{1000},
                              coupecad::core::Millimeters{500},
                              coupecad::core::Millimeters{1500}};
        coupecad::core::Panel panel;
        panel.id = project.uuid_gen().next_id<coupecad::core::PanelIdTag>();
        panel.role = coupecad::core::PanelRole::Bottom;
        cabinet.panels.emplace(panel.id, panel);
        return panel.id;
    }
};

}  // namespace

TEST(AisSceneTest, EmptyAfterConstruction) {
    Fixture f;
    EXPECT_EQ(f.scene.panel_count(), 0u);
}

TEST(AisSceneTest, AddPanelStoresAisHandle) {
    Fixture f;
    const auto pid = f.add_bottom_panel();
    f.scene.add_panel(pid, f.builder.panel_solid(pid));
    EXPECT_EQ(f.scene.panel_count(), 1u);
    EXPECT_TRUE(f.scene.has_panel(pid));
}

TEST(AisSceneTest, RemovePanelDropsHandle) {
    Fixture f;
    const auto pid = f.add_bottom_panel();
    f.scene.add_panel(pid, f.builder.panel_solid(pid));
    f.scene.remove_panel(pid);
    EXPECT_FALSE(f.scene.has_panel(pid));
    EXPECT_EQ(f.scene.panel_count(), 0u);
}

TEST(AisSceneTest, RemoveUnknownPanelIsNoOp) {
    Fixture f;
    auto missing = coupecad::core::PanelId::from_string(
        "00000000-0000-0000-0000-000000000001");
    EXPECT_NO_THROW(f.scene.remove_panel(missing));
}

TEST(AisSceneTest, ReplacePanelUpdatesHandlePointer) {
    Fixture f;
    const auto pid = f.add_bottom_panel();
    f.scene.add_panel(pid, f.builder.panel_solid(pid));
    const auto* before = f.scene.raw_ais_pointer_for_panel(pid);

    // Меняем геометрию: новый shape с другими размерами.
    auto& cabinet = f.project.mutable_cabinet();
    cabinet.dimensions.width = coupecad::core::Millimeters{2000};
    f.builder.rebuild_all();
    f.scene.replace_panel(pid, f.builder.panel_solid(pid));

    const auto* after = f.scene.raw_ais_pointer_for_panel(pid);
    EXPECT_NE(before, after);
    EXPECT_TRUE(f.scene.has_panel(pid));
}
```

- [ ] **Step 2: Создать `src/coupecad/renderer/occt/ais_scene.h`**

```cpp
#pragma once

#include "coupecad/core/id.h"
#include "coupecad/core/project.h"
#include "coupecad/renderer/entity_id.h"

#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Solid.hxx>

#include <unordered_map>

namespace coupecad::renderer::occt {

class ViewDriver;

// Владеет AIS_InteractiveContext, поддерживает словари
// PanelId/HardwareItemId → Handle(AIS_Shape) и обратный словарь
// raw-pointer → EntityId для picking/selection.
//
// Не thread-safe.
class AisScene {
public:
    AisScene(ViewDriver& driver, const core::Project& project);
    ~AisScene();

    AisScene(const AisScene&) = delete;
    AisScene& operator=(const AisScene&) = delete;

    const Handle(AIS_InteractiveContext)& context() const noexcept { return context_; }

    // Panel side.
    void add_panel(const core::PanelId& id, const TopoDS_Solid& solid);
    void replace_panel(const core::PanelId& id, const TopoDS_Solid& solid);
    void remove_panel(const core::PanelId& id);
    bool has_panel(const core::PanelId& id) const noexcept;
    std::size_t panel_count() const noexcept { return panel_objects_.size(); }
    std::vector<core::PanelId> panel_ids() const;

    // Только для тестов.
    const AIS_InteractiveObject* raw_ais_pointer_for_panel(
        const core::PanelId& id) const;

private:
    void erase_panel_internal(const core::PanelId& id);

    ViewDriver&                          driver_;
    const core::Project&                 project_;
    Handle(AIS_InteractiveContext)       context_;
    std::unordered_map<core::PanelId, Handle(AIS_Shape)> panel_objects_;
    std::unordered_map<const AIS_InteractiveObject*, EntityId> ais_to_entity_;
};

}  // namespace coupecad::renderer::occt
```

- [ ] **Step 3: Создать `src/coupecad/renderer/occt/ais_scene.cpp`**

```cpp
#include "coupecad/renderer/occt/ais_scene.h"

#include "coupecad/logging/logger.h"
#include "coupecad/renderer/occt/material_resolver.h"
#include "coupecad/renderer/occt/view_driver.h"

namespace coupecad::renderer::occt {

AisScene::AisScene(ViewDriver& driver, const core::Project& project)
    : driver_(driver), project_(project) {
    context_ = new AIS_InteractiveContext(driver_.viewer());
    context_->SetAutomaticHilight(Standard_False);
}

AisScene::~AisScene() = default;

void AisScene::add_panel(const core::PanelId& id, const TopoDS_Solid& solid) {
    const auto& panel = project_.cabinet().panels.at(id);
    Handle(AIS_Shape) ais = new AIS_Shape(solid);
    ais->SetColor(resolve_panel_color(project_, panel));

    context_->Display(ais, /*updateViewer=*/Standard_False);
    panel_objects_.emplace(id, ais);
    ais_to_entity_.emplace(ais.get(), EntityId{id});

    coupecad::logging::Logger::instance().trace(
        "renderer", "ais_scene.add_panel id={}", id.to_string());
}

void AisScene::replace_panel(const core::PanelId& id, const TopoDS_Solid& solid) {
    erase_panel_internal(id);
    add_panel(id, solid);
}

void AisScene::remove_panel(const core::PanelId& id) {
    erase_panel_internal(id);
    coupecad::logging::Logger::instance().trace(
        "renderer", "ais_scene.remove_panel id={}", id.to_string());
}

bool AisScene::has_panel(const core::PanelId& id) const noexcept {
    return panel_objects_.find(id) != panel_objects_.end();
}

std::vector<core::PanelId> AisScene::panel_ids() const {
    std::vector<core::PanelId> result;
    result.reserve(panel_objects_.size());
    for (const auto& [id, _] : panel_objects_) result.push_back(id);
    return result;
}

const AIS_InteractiveObject* AisScene::raw_ais_pointer_for_panel(
    const core::PanelId& id) const {
    auto it = panel_objects_.find(id);
    return it == panel_objects_.end() ? nullptr : it->second.get();
}

void AisScene::erase_panel_internal(const core::PanelId& id) {
    auto it = panel_objects_.find(id);
    if (it == panel_objects_.end()) return;

    ais_to_entity_.erase(it->second.get());
    context_->Remove(it->second, /*updateViewer=*/Standard_False);
    panel_objects_.erase(it);
}

}  // namespace coupecad::renderer::occt
```

- [ ] **Step 4: Добавить `ais_scene.cpp` в `src/coupecad/renderer/occt/CMakeLists.txt`**

```cmake
add_library(coupecad_renderer_occt STATIC
    view_driver.cpp
    material_resolver.cpp
    ais_scene.cpp
)
```

- [ ] **Step 5: Добавить `ais_scene_test.cpp` в `tests/renderer/CMakeLists.txt`**

- [ ] **Step 6: Собрать и запустить**

Run:
```bash
cmake --build --preset default --target coupecad_renderer_test
ctest --preset default -R AisSceneTest --output-on-failure
```

Expected: 5 тестов, 5 passed.

- [ ] **Step 7: Закоммитить**

```bash
git add src/coupecad/renderer/occt/ais_scene.h \
        src/coupecad/renderer/occt/ais_scene.cpp \
        src/coupecad/renderer/occt/CMakeLists.txt \
        tests/renderer/ais_scene_test.cpp \
        tests/renderer/CMakeLists.txt
git commit -m "feat(renderer): AisScene — add/replace/remove panel + reverse map"
```

---

## Task 7: ais_scene — hardware и material-refresh

**Files:**
- Modify: `src/coupecad/renderer/occt/ais_scene.h`
- Modify: `src/coupecad/renderer/occt/ais_scene.cpp`
- Modify: `tests/renderer/ais_scene_test.cpp`

- [ ] **Step 1: Дописать падающие тесты для hardware и refresh**

Добавить в `tests/renderer/ais_scene_test.cpp` после существующих:

```cpp
#include "coupecad/core/hardware.h"

namespace {

coupecad::core::HardwareItemId add_minimal_hardware(Fixture& f) {
    // HardwareSpec: 30×30×30 мм generic.
    coupecad::core::HardwareRef ref{"hw.test.box"};
    coupecad::core::HardwareSpec spec;
    spec.ref = ref;
    spec.kind = coupecad::core::HardwareKind::Other;
    spec.name = "test box";
    spec.bbox = {coupecad::core::Millimeters{30},
                 coupecad::core::Millimeters{30},
                 coupecad::core::Millimeters{30}};
    f.project.mutable_hardware_catalog().emplace(ref, spec);

    coupecad::core::HardwareItem item;
    item.id = f.project.uuid_gen().next_id<coupecad::core::HardwareItemIdTag>();
    item.ref = ref;
    coupecad::core::PanelAttachment att;
    att.panel_id = f.project.cabinet().panels.begin()->first;
    att.local_position = {coupecad::core::Millimeters{10},
                          coupecad::core::Millimeters{10},
                          coupecad::core::Millimeters{10}};
    item.attachments.push_back(att);
    f.project.mutable_cabinet().hardware.emplace(item.id, item);
    return item.id;
}

}  // namespace

TEST(AisSceneTest, AddHardwareStoresAisHandle) {
    Fixture f;
    f.add_bottom_panel();
    const auto hid = add_minimal_hardware(f);
    f.scene.add_hardware(hid, f.builder.hardware_compound(hid));
    EXPECT_TRUE(f.scene.has_hardware(hid));
    EXPECT_EQ(f.scene.hardware_count(), 1u);
}

TEST(AisSceneTest, RemoveHardwareDropsHandle) {
    Fixture f;
    f.add_bottom_panel();
    const auto hid = add_minimal_hardware(f);
    f.scene.add_hardware(hid, f.builder.hardware_compound(hid));
    f.scene.remove_hardware(hid);
    EXPECT_FALSE(f.scene.has_hardware(hid));
}

TEST(AisSceneTest, RefreshColorsForMaterialUpdatesAffectedPanelColor) {
    Fixture f;
    const auto pid = f.add_bottom_panel();
    f.scene.add_panel(pid, f.builder.panel_solid(pid));

    // Меняем цвет дефолтного материала шкафа.
    auto& cabinet = f.project.mutable_cabinet();
    auto& materials = f.project.mutable_materials();
    materials.at(cabinet.default_panel_material).color_hint =
        coupecad::core::RGBA{255, 0, 0, 255};

    f.scene.refresh_colors_for_material(cabinet.default_panel_material);

    // Sanity: после refresh AIS-объект всё ещё на месте.
    EXPECT_TRUE(f.scene.has_panel(pid));
    // Прямая проверка цвета AIS_Shape:
    Quantity_Color c;
    f.scene.context()->Color(
        f.scene.raw_ais_handle_for_panel(pid), c);
    EXPECT_NEAR(c.Red(),   1.0, 1e-3);
    EXPECT_NEAR(c.Green(), 0.0, 1e-3);
    EXPECT_NEAR(c.Blue(),  0.0, 1e-3);
}
```

> `raw_ais_handle_for_panel` — публичный test-helper, который мы добавим параллельно с `raw_ais_pointer_for_panel` (нужен `Handle(AIS_Shape)` для `Color()` API).

- [ ] **Step 2: Расширить `ais_scene.h`**

Добавить в public-секцию (после panel-методов):

```cpp
    // Hardware side.
    void add_hardware(const core::HardwareItemId& id, const TopoDS_Compound& compound);
    void replace_hardware(const core::HardwareItemId& id, const TopoDS_Compound& compound);
    void remove_hardware(const core::HardwareItemId& id);
    bool has_hardware(const core::HardwareItemId& id) const noexcept;
    std::size_t hardware_count() const noexcept { return hardware_objects_.size(); }
    std::vector<core::HardwareItemId> hardware_ids() const;

    // Перерисовать цвета всех панелей, чей effective_material совпадает с mat_id.
    // Если материал — default_panel_material шкафа, обновляются все панели
    // без material_override.
    void refresh_colors_for_material(const core::MaterialId& mat_id);

    // Очистить всю сцену (panels + hardware).
    void clear();

    // Test-helpers.
    Handle(AIS_Shape) raw_ais_handle_for_panel(const core::PanelId& id) const;
    const AIS_InteractiveObject* raw_ais_pointer_for_hardware(
        const core::HardwareItemId& id) const;
```

И добавить приватные поля:

```cpp
    void erase_hardware_internal(const core::HardwareItemId& id);

    std::unordered_map<core::HardwareItemId, Handle(AIS_Shape)> hardware_objects_;
```

- [ ] **Step 3: Расширить `ais_scene.cpp`**

Добавить методы:

```cpp
void AisScene::add_hardware(const core::HardwareItemId& id,
                            const TopoDS_Compound& compound) {
    Handle(AIS_Shape) ais = new AIS_Shape(compound);
    ais->SetColor(resolve_hardware_color());

    context_->Display(ais, Standard_False);
    hardware_objects_.emplace(id, ais);
    ais_to_entity_.emplace(ais.get(), EntityId{id});

    coupecad::logging::Logger::instance().trace(
        "renderer", "ais_scene.add_hardware id={}", id.to_string());
}

void AisScene::replace_hardware(const core::HardwareItemId& id,
                                const TopoDS_Compound& compound) {
    erase_hardware_internal(id);
    add_hardware(id, compound);
}

void AisScene::remove_hardware(const core::HardwareItemId& id) {
    erase_hardware_internal(id);
    coupecad::logging::Logger::instance().trace(
        "renderer", "ais_scene.remove_hardware id={}", id.to_string());
}

bool AisScene::has_hardware(const core::HardwareItemId& id) const noexcept {
    return hardware_objects_.find(id) != hardware_objects_.end();
}

std::vector<core::HardwareItemId> AisScene::hardware_ids() const {
    std::vector<core::HardwareItemId> result;
    result.reserve(hardware_objects_.size());
    for (const auto& [id, _] : hardware_objects_) result.push_back(id);
    return result;
}

void AisScene::refresh_colors_for_material(const core::MaterialId& mat_id) {
    const auto& cabinet = project_.cabinet();
    for (const auto& [panel_id, ais] : panel_objects_) {
        const auto& panel = cabinet.panels.at(panel_id);
        const core::MaterialId effective =
            panel.material_override.value_or(cabinet.default_panel_material);
        if (effective != mat_id) continue;
        ais->SetColor(resolve_panel_color(project_, panel));
        context_->Redisplay(ais, Standard_False, Standard_False);
    }
}

void AisScene::clear() {
    for (auto& [_, ais] : panel_objects_) {
        context_->Remove(ais, Standard_False);
    }
    for (auto& [_, ais] : hardware_objects_) {
        context_->Remove(ais, Standard_False);
    }
    panel_objects_.clear();
    hardware_objects_.clear();
    ais_to_entity_.clear();
}

void AisScene::erase_hardware_internal(const core::HardwareItemId& id) {
    auto it = hardware_objects_.find(id);
    if (it == hardware_objects_.end()) return;
    ais_to_entity_.erase(it->second.get());
    context_->Remove(it->second, Standard_False);
    hardware_objects_.erase(it);
}

Handle(AIS_Shape) AisScene::raw_ais_handle_for_panel(
    const core::PanelId& id) const {
    auto it = panel_objects_.find(id);
    return it == panel_objects_.end() ? Handle(AIS_Shape){} : it->second;
}

const AIS_InteractiveObject* AisScene::raw_ais_pointer_for_hardware(
    const core::HardwareItemId& id) const {
    auto it = hardware_objects_.find(id);
    return it == hardware_objects_.end() ? nullptr : it->second.get();
}
```

- [ ] **Step 4: Собрать и запустить**

```bash
cmake --build --preset default --target coupecad_renderer_test
ctest --preset default -R AisSceneTest --output-on-failure
```

Expected: 8 тестов (5 предыдущих + 3 новых), все passed.

- [ ] **Step 5: Закоммитить**

```bash
git add src/coupecad/renderer/occt/ais_scene.h \
        src/coupecad/renderer/occt/ais_scene.cpp \
        tests/renderer/ais_scene_test.cpp
git commit -m "feat(renderer): AisScene — hardware + refresh_colors_for_material + clear"
```

---

## Task 8: OcctRenderer skeleton + factory

**Files:**
- Create: `src/coupecad/renderer/occt/occt_renderer.h`
- Create: `src/coupecad/renderer/occt/occt_renderer.cpp`
- Modify: `src/coupecad/renderer/occt/CMakeLists.txt`

> Реализуем класс целиком, но методы `IRenderer` (sync, camera, picking, selection, render_to_image) — заглушки `throw "not implemented yet"`. Конкретные реализации добавляются в Tasks 9-13. На этом таске тестов нет — добавляем в следующих.

- [ ] **Step 1: Создать `src/coupecad/renderer/occt/occt_renderer.h`**

```cpp
#pragma once

#include "coupecad/renderer/i_renderer.h"
#include "coupecad/renderer/occt/ais_scene.h"
#include "coupecad/renderer/occt/view_driver.h"

#include <memory>

namespace coupecad::core { class Project; }
namespace coupecad::geometry { class GeometryBuilder; }

namespace coupecad::renderer::occt {

class OcctRenderer : public IRenderer {
public:
    OcctRenderer(const core::Project& project,
                 geometry::GeometryBuilder& builder);
    ~OcctRenderer() override;

    OcctRenderer(const OcctRenderer&) = delete;
    OcctRenderer& operator=(const OcctRenderer&) = delete;

    // IRenderer.
    void sync(const core::ChangeSet& cs) override;
    void rebuild_all() override;

    void           set_camera(const CameraState& state) override;
    CameraState    camera() const override;
    void           fit_all() override;

    std::optional<EntityId> pick(int x, int y) override;

    void                          select(const EntityId& id) override;
    void                          deselect(const EntityId& id) override;
    void                          clear_selection() override;
    std::vector<EntityId>         selection() const override;

    void           set_viewport_size(ViewportSize size) override;
    ViewportSize   viewport_size() const override;

    std::vector<std::uint8_t> render_to_image() override;

    // Test inspectors (не часть IRenderer).
    const AisScene&   scene() const noexcept { return scene_; }
    const ViewDriver& driver() const noexcept { return driver_; }

private:
    const core::Project&        project_;
    geometry::GeometryBuilder&  builder_;
    ViewDriver                  driver_;
    AisScene                    scene_;
};

// Factory.
std::unique_ptr<IRenderer> make_occt_renderer(const core::Project& project,
                                              geometry::GeometryBuilder& builder);

}  // namespace coupecad::renderer::occt
```

- [ ] **Step 2: Создать `src/coupecad/renderer/occt/occt_renderer.cpp`**

```cpp
#include "coupecad/renderer/occt/occt_renderer.h"

#include "coupecad/core/errors.h"
#include "coupecad/geometry/geometry_builder.h"
#include "coupecad/logging/logger.h"

namespace coupecad::renderer::occt {

namespace {

[[noreturn]] void not_implemented_yet(const char* where) {
    throw core::DomainError{"renderer.not_implemented_yet",
                            std::string{"OcctRenderer::"} + where +
                                " не реализовано (заполняется в плане Stage 3)"};
}

}  // namespace

OcctRenderer::OcctRenderer(const core::Project& project,
                           geometry::GeometryBuilder& builder)
    : project_(project), builder_(builder), scene_(driver_, project_) {
    coupecad::logging::Logger::instance().info(
        "renderer", "OcctRenderer constructed (gl_available={})",
        driver_.gl_available());
}

OcctRenderer::~OcctRenderer() = default;

void OcctRenderer::sync(const core::ChangeSet& /*cs*/)  { not_implemented_yet("sync"); }
void OcctRenderer::rebuild_all()                        { not_implemented_yet("rebuild_all"); }

void           OcctRenderer::set_camera(const CameraState&) { not_implemented_yet("set_camera"); }
CameraState    OcctRenderer::camera() const                 { not_implemented_yet("camera"); }
void           OcctRenderer::fit_all()                      { not_implemented_yet("fit_all"); }

std::optional<EntityId> OcctRenderer::pick(int /*x*/, int /*y*/) {
    not_implemented_yet("pick");
}

void                  OcctRenderer::select(const EntityId&)   { not_implemented_yet("select"); }
void                  OcctRenderer::deselect(const EntityId&) { not_implemented_yet("deselect"); }
void                  OcctRenderer::clear_selection()         { not_implemented_yet("clear_selection"); }
std::vector<EntityId> OcctRenderer::selection() const         { not_implemented_yet("selection"); }

void          OcctRenderer::set_viewport_size(ViewportSize) { not_implemented_yet("set_viewport_size"); }
ViewportSize  OcctRenderer::viewport_size() const           { not_implemented_yet("viewport_size"); }

std::vector<std::uint8_t> OcctRenderer::render_to_image() { not_implemented_yet("render_to_image"); }

std::unique_ptr<IRenderer> make_occt_renderer(const core::Project& project,
                                              geometry::GeometryBuilder& builder) {
    return std::make_unique<OcctRenderer>(project, builder);
}

}  // namespace coupecad::renderer::occt
```

- [ ] **Step 3: Добавить файл в `src/coupecad/renderer/occt/CMakeLists.txt`**

```cmake
add_library(coupecad_renderer_occt STATIC
    view_driver.cpp
    material_resolver.cpp
    ais_scene.cpp
    occt_renderer.cpp
)
```

- [ ] **Step 4: Собрать**

```bash
cmake --build --preset default --target coupecad_renderer_occt
```

Expected: builds clean. Тестов на этот этап нет; следующие тасски будут заполнять stub-методы один за другим.

- [ ] **Step 5: Закоммитить**

```bash
git add src/coupecad/renderer/occt/occt_renderer.h \
        src/coupecad/renderer/occt/occt_renderer.cpp \
        src/coupecad/renderer/occt/CMakeLists.txt
git commit -m "feat(renderer): OcctRenderer skeleton + make_occt_renderer factory"
```

---

## Task 9: sync(ChangeSet) + rebuild_all

**Files:**
- Modify: `src/coupecad/renderer/occt/occt_renderer.cpp`
- Create: `tests/renderer/occt_renderer_sync_test.cpp`
- Modify: `tests/renderer/CMakeLists.txt`

- [ ] **Step 1: Написать падающий тест `tests/renderer/occt_renderer_sync_test.cpp`**

```cpp
#include "coupecad/renderer/occt/occt_renderer.h"

#include "coupecad/core/cabinet.h"
#include "coupecad/core/commands/change_set.h"
#include "coupecad/core/hardware.h"
#include "coupecad/core/panel.h"
#include "coupecad/core/project.h"
#include "coupecad/geometry/geometry_builder.h"

#include <gtest/gtest.h>

using coupecad::core::ChangeSet;
using coupecad::core::Project;
using coupecad::geometry::GeometryBuilder;
using coupecad::renderer::occt::OcctRenderer;

namespace {

struct Fixture {
    Project project = Project::create_empty("sync");
    GeometryBuilder builder{project};
    OcctRenderer renderer{project, builder};

    Fixture() {
        auto& cabinet = project.mutable_cabinet();
        cabinet.dimensions = {coupecad::core::Millimeters{1000},
                              coupecad::core::Millimeters{500},
                              coupecad::core::Millimeters{1500}};
    }

    coupecad::core::PanelId add_panel_to_cabinet() {
        auto& cabinet = project.mutable_cabinet();
        coupecad::core::Panel p;
        p.id = project.uuid_gen().next_id<coupecad::core::PanelIdTag>();
        p.role = coupecad::core::PanelRole::Bottom;
        cabinet.panels.emplace(p.id, p);
        return p.id;
    }
};

}  // namespace

TEST(OcctRendererSyncTest, AddedPanelEntersScene) {
    Fixture f;
    const auto pid = f.add_panel_to_cabinet();
    ChangeSet cs;
    cs.added_panels.push_back(pid);

    f.renderer.sync(cs);

    EXPECT_TRUE(f.renderer.scene().has_panel(pid));
    EXPECT_EQ(f.renderer.scene().panel_count(), 1u);
}

TEST(OcctRendererSyncTest, RemovedPanelLeavesScene) {
    Fixture f;
    const auto pid = f.add_panel_to_cabinet();
    ChangeSet add; add.added_panels.push_back(pid);
    f.renderer.sync(add);

    // Удаляем из проекта; затем сообщаем рендереру.
    f.project.mutable_cabinet().panels.erase(pid);
    ChangeSet remove; remove.removed_panels.push_back(pid);
    f.renderer.sync(remove);

    EXPECT_FALSE(f.renderer.scene().has_panel(pid));
}

TEST(OcctRendererSyncTest, UpdatedPanelKeepsIdSwapsHandle) {
    Fixture f;
    const auto pid = f.add_panel_to_cabinet();
    ChangeSet add; add.added_panels.push_back(pid);
    f.renderer.sync(add);
    const auto* before = f.renderer.scene().raw_ais_pointer_for_panel(pid);

    // Изменяем размеры шкафа → role-based геометрия меняется.
    f.project.mutable_cabinet().dimensions.width = coupecad::core::Millimeters{2000};
    f.builder.rebuild_all();
    ChangeSet update; update.updated_panels.push_back(pid);
    f.renderer.sync(update);

    const auto* after = f.renderer.scene().raw_ais_pointer_for_panel(pid);
    EXPECT_TRUE(f.renderer.scene().has_panel(pid));
    EXPECT_NE(before, after);
}

TEST(OcctRendererSyncTest, CabinetChangedReplacesAllExistingPanels) {
    Fixture f;
    const auto p1 = f.add_panel_to_cabinet();
    const auto p2 = f.add_panel_to_cabinet();
    ChangeSet add; add.added_panels = {p1, p2};
    f.renderer.sync(add);

    const auto* before1 = f.renderer.scene().raw_ais_pointer_for_panel(p1);
    const auto* before2 = f.renderer.scene().raw_ais_pointer_for_panel(p2);

    f.project.mutable_cabinet().dimensions.width = coupecad::core::Millimeters{3000};
    f.builder.rebuild_all();
    ChangeSet cab; cab.cabinet_changed = true;
    f.renderer.sync(cab);

    EXPECT_NE(before1, f.renderer.scene().raw_ais_pointer_for_panel(p1));
    EXPECT_NE(before2, f.renderer.scene().raw_ais_pointer_for_panel(p2));
}

TEST(OcctRendererSyncTest, MaterialDeltaRefreshesColors) {
    Fixture f;
    const auto pid = f.add_panel_to_cabinet();
    ChangeSet add; add.added_panels.push_back(pid);
    f.renderer.sync(add);

    auto& cabinet = f.project.mutable_cabinet();
    auto& materials = f.project.mutable_materials();
    materials.at(cabinet.default_panel_material).color_hint =
        coupecad::core::RGBA{0, 255, 0, 255};

    ChangeSet upd; upd.updated_materials.push_back(cabinet.default_panel_material);
    f.renderer.sync(upd);

    Quantity_Color c;
    f.renderer.scene().context()->Color(
        f.renderer.scene().raw_ais_handle_for_panel(pid), c);
    EXPECT_NEAR(c.Green(), 1.0, 1e-3);
}

TEST(OcctRendererSyncTest, EmptyChangeSetIsNoOp) {
    Fixture f;
    const auto pid = f.add_panel_to_cabinet();
    ChangeSet add; add.added_panels.push_back(pid);
    f.renderer.sync(add);

    const auto* before = f.renderer.scene().raw_ais_pointer_for_panel(pid);
    f.renderer.sync(ChangeSet{});
    EXPECT_EQ(before, f.renderer.scene().raw_ais_pointer_for_panel(pid));
}

TEST(OcctRendererSyncTest, RebuildAllClearsAndRepopulates) {
    Fixture f;
    const auto pid = f.add_panel_to_cabinet();
    ChangeSet add; add.added_panels.push_back(pid);
    f.renderer.sync(add);

    f.builder.rebuild_all();
    f.renderer.rebuild_all();

    EXPECT_TRUE(f.renderer.scene().has_panel(pid));
    EXPECT_EQ(f.renderer.scene().panel_count(), 1u);
}
```

- [ ] **Step 2: Реализовать `sync` и `rebuild_all` в `occt_renderer.cpp`**

Заменить заглушки `not_implemented_yet("sync")` и `not_implemented_yet("rebuild_all")` на:

```cpp
void OcctRenderer::sync(const core::ChangeSet& cs) {
    if (cs.empty()) return;

    coupecad::logging::Logger::instance().debug(
        "renderer",
        "sync: panels(+/-/u)={}/{}/{} hardware(+/-/u)={}/{}/{} cabinet={} materials(+/-/u)={}/{}/{}",
        cs.added_panels.size(), cs.removed_panels.size(), cs.updated_panels.size(),
        cs.added_hardware.size(), cs.removed_hardware.size(), cs.updated_hardware.size(),
        cs.cabinet_changed ? 1 : 0,
        cs.added_materials.size(), cs.removed_materials.size(), cs.updated_materials.size());

    // Удаления.
    for (const auto& id : cs.removed_panels)   scene_.remove_panel(id);
    for (const auto& id : cs.removed_hardware) scene_.remove_hardware(id);

    // Updates = remove + add.
    for (const auto& id : cs.updated_panels)
        scene_.replace_panel(id, builder_.panel_solid(id));
    for (const auto& id : cs.updated_hardware)
        scene_.replace_hardware(id, builder_.hardware_compound(id));

    // Adds.
    for (const auto& id : cs.added_panels)
        scene_.add_panel(id, builder_.panel_solid(id));
    for (const auto& id : cs.added_hardware)
        scene_.add_hardware(id, builder_.hardware_compound(id));

    // cabinet_changed → replace всё, что сейчас в сцене.
    if (cs.cabinet_changed) {
        for (const auto& id : scene_.panel_ids())
            scene_.replace_panel(id, builder_.panel_solid(id));
        for (const auto& id : scene_.hardware_ids())
            scene_.replace_hardware(id, builder_.hardware_compound(id));
    }

    // Material-only дельты.
    for (const auto& mat_id : cs.added_materials)   scene_.refresh_colors_for_material(mat_id);
    for (const auto& mat_id : cs.removed_materials) scene_.refresh_colors_for_material(mat_id);
    for (const auto& mat_id : cs.updated_materials) scene_.refresh_colors_for_material(mat_id);
}

void OcctRenderer::rebuild_all() {
    coupecad::logging::Logger::instance().info("renderer", "rebuild_all");
    scene_.clear();
    for (const auto& [pid, _] : project_.cabinet().panels) {
        scene_.add_panel(pid, builder_.panel_solid(pid));
    }
    for (const auto& [hid, _] : project_.cabinet().hardware) {
        scene_.add_hardware(hid, builder_.hardware_compound(hid));
    }
}
```

- [ ] **Step 3: Подключить тест в `tests/renderer/CMakeLists.txt`**

Добавить `occt_renderer_sync_test.cpp` в источники.

- [ ] **Step 4: Собрать и запустить**

```bash
cmake --build --preset default --target coupecad_renderer_test
ctest --preset default -R OcctRendererSyncTest --output-on-failure
```

Expected: 7 тестов, 7 passed.

- [ ] **Step 5: Закоммитить**

```bash
git add src/coupecad/renderer/occt/occt_renderer.cpp \
        tests/renderer/occt_renderer_sync_test.cpp \
        tests/renderer/CMakeLists.txt
git commit -m "feat(renderer): OcctRenderer.sync(ChangeSet) + rebuild_all"
```

---

## Task 10: Камера (set/get/fit_all)

**Files:**
- Modify: `src/coupecad/renderer/occt/occt_renderer.cpp`
- Create: `tests/renderer/occt_renderer_camera_test.cpp`
- Modify: `tests/renderer/CMakeLists.txt`

- [ ] **Step 1: Написать падающий тест `tests/renderer/occt_renderer_camera_test.cpp`**

```cpp
#include "coupecad/renderer/occt/occt_renderer.h"

#include "coupecad/core/panel.h"
#include "coupecad/core/project.h"
#include "coupecad/geometry/geometry_builder.h"

#include <gtest/gtest.h>

using coupecad::core::Millimeters;
using coupecad::core::Project;
using coupecad::geometry::GeometryBuilder;
using coupecad::renderer::CameraState;
using coupecad::renderer::occt::OcctRenderer;

TEST(OcctRendererCameraTest, SetThenGetReturnsApprox) {
    Project project = Project::create_empty("cam");
    GeometryBuilder builder{project};
    OcctRenderer renderer{project, builder};

    CameraState in;
    in.eye    = {Millimeters{1000}, Millimeters{-1000}, Millimeters{500}};
    in.target = {Millimeters{0},    Millimeters{0},     Millimeters{0}};
    in.up     = {Millimeters{0},    Millimeters{0},     Millimeters{1}};
    in.fov_deg = 60.0;

    renderer.set_camera(in);
    const CameraState out = renderer.camera();

    EXPECT_EQ(out.eye, in.eye);
    EXPECT_EQ(out.target, in.target);
    EXPECT_NEAR(out.fov_deg, in.fov_deg, 1e-3);
}

TEST(OcctRendererCameraTest, OrthographicWhenFovZero) {
    Project project = Project::create_empty("cam2");
    GeometryBuilder builder{project};
    OcctRenderer renderer{project, builder};

    CameraState in;
    in.eye    = {Millimeters{0}, Millimeters{-1000}, Millimeters{0}};
    in.target = {Millimeters{0}, Millimeters{0},      Millimeters{0}};
    in.fov_deg = 0.0;

    renderer.set_camera(in);
    EXPECT_NEAR(renderer.camera().fov_deg, 0.0, 1e-9);
}

TEST(OcctRendererCameraTest, FitAllDoesNotThrowOnEmptyScene) {
    Project project = Project::create_empty("cam3");
    GeometryBuilder builder{project};
    OcctRenderer renderer{project, builder};
    EXPECT_NO_THROW(renderer.fit_all());
}
```

- [ ] **Step 2: Реализовать camera-методы в `occt_renderer.cpp`**

Заменить три заглушки:

```cpp
void OcctRenderer::set_camera(const CameraState& s) {
    auto& view = driver_.view();
    auto camera = view->Camera();
    camera->SetEye(gp_Pnt(static_cast<Standard_Real>(s.eye.x.value()),
                          static_cast<Standard_Real>(s.eye.y.value()),
                          static_cast<Standard_Real>(s.eye.z.value())));
    camera->SetCenter(gp_Pnt(static_cast<Standard_Real>(s.target.x.value()),
                             static_cast<Standard_Real>(s.target.y.value()),
                             static_cast<Standard_Real>(s.target.z.value())));
    camera->SetUp(gp_Dir(static_cast<Standard_Real>(s.up.x.value()),
                         static_cast<Standard_Real>(s.up.y.value()),
                         static_cast<Standard_Real>(s.up.z.value())));
    if (s.fov_deg <= 0.0) {
        camera->SetProjectionType(Graphic3d_Camera::Projection_Orthographic);
    } else {
        camera->SetProjectionType(Graphic3d_Camera::Projection_Perspective);
        camera->SetFOVy(s.fov_deg);
    }
    view->Update();
}

CameraState OcctRenderer::camera() const {
    const auto& view = driver_.view();
    const auto camera = view->Camera();
    const gp_Pnt eye    = camera->Eye();
    const gp_Pnt center = camera->Center();
    const gp_Dir up     = camera->Up();

    auto to_vec = [](const gp_Pnt& p) {
        return core::Vec3{
            core::Millimeters{static_cast<std::int32_t>(p.X())},
            core::Millimeters{static_cast<std::int32_t>(p.Y())},
            core::Millimeters{static_cast<std::int32_t>(p.Z())}};
    };
    auto to_vec_dir = [](const gp_Dir& d) {
        return core::Vec3{
            core::Millimeters{static_cast<std::int32_t>(d.X())},
            core::Millimeters{static_cast<std::int32_t>(d.Y())},
            core::Millimeters{static_cast<std::int32_t>(d.Z())}};
    };

    CameraState s;
    s.eye    = to_vec(eye);
    s.target = to_vec(center);
    s.up     = to_vec_dir(up);
    s.fov_deg =
        camera->ProjectionType() == Graphic3d_Camera::Projection_Orthographic
            ? 0.0
            : camera->FOVy();
    return s;
}

void OcctRenderer::fit_all() {
    if (scene_.panel_count() == 0 && scene_.hardware_count() == 0) {
        // Пустая сцена — нечего фиттить, но и не падаем.
        return;
    }
    driver_.view()->FitAll();
    driver_.view()->Update();
}
```

> Подключить недостающие OCCT-заголовки в `occt_renderer.cpp`:
> ```cpp
> #include <Graphic3d_Camera.hxx>
> #include <gp_Dir.hxx>
> #include <gp_Pnt.hxx>
> ```

- [ ] **Step 3: Добавить тест в CMakeLists, собрать, запустить**

```bash
cmake --build --preset default --target coupecad_renderer_test
ctest --preset default -R OcctRendererCameraTest --output-on-failure
```

Expected: 3 теста, 3 passed.

- [ ] **Step 4: Закоммитить**

```bash
git add src/coupecad/renderer/occt/occt_renderer.cpp \
        tests/renderer/occt_renderer_camera_test.cpp \
        tests/renderer/CMakeLists.txt
git commit -m "feat(renderer): camera set/get/fit_all"
```

---

## Task 11: Selection (select/deselect/clear/selection)

**Files:**
- Modify: `src/coupecad/renderer/occt/ais_scene.h`
- Modify: `src/coupecad/renderer/occt/ais_scene.cpp`
- Modify: `src/coupecad/renderer/occt/occt_renderer.cpp`
- Create: `tests/renderer/occt_renderer_selection_test.cpp`
- Modify: `tests/renderer/CMakeLists.txt`

> Selection state живёт в `AIS_InteractiveContext`; рендерер делегирует, но добавляем в `AisScene` хелпер `select_by_entity_id()`/`selection()`/`clear_selection()`/`deselect_by_entity_id()` для прямого доступа из теста.

- [ ] **Step 1: Падающий тест `tests/renderer/occt_renderer_selection_test.cpp`**

```cpp
#include "coupecad/renderer/occt/occt_renderer.h"

#include "coupecad/core/panel.h"
#include "coupecad/core/project.h"
#include "coupecad/geometry/geometry_builder.h"

#include <gtest/gtest.h>

#include <algorithm>

using coupecad::core::Millimeters;
using coupecad::core::Project;
using coupecad::geometry::GeometryBuilder;
using coupecad::renderer::EntityId;
using coupecad::renderer::occt::OcctRenderer;

namespace {

struct Fixture {
    Project project = Project::create_empty("sel");
    GeometryBuilder builder{project};
    OcctRenderer renderer{project, builder};

    Fixture() {
        auto& cab = project.mutable_cabinet();
        cab.dimensions = {Millimeters{1000}, Millimeters{500}, Millimeters{1500}};
    }

    coupecad::core::PanelId add_panel() {
        coupecad::core::Panel p;
        p.id = project.uuid_gen().next_id<coupecad::core::PanelIdTag>();
        p.role = coupecad::core::PanelRole::Bottom;
        project.mutable_cabinet().panels.emplace(p.id, p);
        coupecad::core::ChangeSet cs;
        cs.added_panels.push_back(p.id);
        renderer.sync(cs);
        return p.id;
    }
};

}  // namespace

TEST(OcctRendererSelectionTest, EmptySelectionInitially) {
    Fixture f;
    EXPECT_TRUE(f.renderer.selection().empty());
}

TEST(OcctRendererSelectionTest, SelectAddsToSelection) {
    Fixture f;
    const auto pid = f.add_panel();
    f.renderer.select(EntityId{pid});
    auto sel = f.renderer.selection();
    ASSERT_EQ(sel.size(), 1u);
    EXPECT_EQ(sel[0], EntityId{pid});
}

TEST(OcctRendererSelectionTest, DeselectRemovesFromSelection) {
    Fixture f;
    const auto pid = f.add_panel();
    f.renderer.select(EntityId{pid});
    f.renderer.deselect(EntityId{pid});
    EXPECT_TRUE(f.renderer.selection().empty());
}

TEST(OcctRendererSelectionTest, ClearSelectionEmptiesSet) {
    Fixture f;
    const auto p1 = f.add_panel();
    const auto p2 = f.add_panel();
    f.renderer.select(EntityId{p1});
    f.renderer.select(EntityId{p2});
    f.renderer.clear_selection();
    EXPECT_TRUE(f.renderer.selection().empty());
}

TEST(OcctRendererSelectionTest, SelectUnknownThrowsDomainError) {
    Fixture f;
    auto missing = coupecad::core::PanelId::from_string(
        "00000000-0000-0000-0000-000000000099");
    try {
        f.renderer.select(EntityId{missing});
        FAIL() << "expected DomainError";
    } catch (const coupecad::core::DomainError& e) {
        EXPECT_EQ(e.code(), "renderer.unknown_entity_in_selection");
    }
}

TEST(OcctRendererSelectionTest, RemovingSelectedPanelDropsItFromSelection) {
    Fixture f;
    const auto pid = f.add_panel();
    f.renderer.select(EntityId{pid});

    f.project.mutable_cabinet().panels.erase(pid);
    coupecad::core::ChangeSet rm;
    rm.removed_panels.push_back(pid);
    f.renderer.sync(rm);

    EXPECT_TRUE(f.renderer.selection().empty());
}
```

- [ ] **Step 2: Расширить `ais_scene.h` методами селекции**

Добавить в public:

```cpp
    // Selection.
    bool                    has_entity(const EntityId& id) const noexcept;
    void                    select(const EntityId& id);   // throws if unknown
    void                    deselect(const EntityId& id); // no-op if unknown
    void                    clear_selection();
    std::vector<EntityId>   selection() const;
```

И приватный helper:

```cpp
    Handle(AIS_InteractiveObject) ais_for_entity(const EntityId& id) const;
```

- [ ] **Step 3: Реализовать в `ais_scene.cpp`**

```cpp
bool AisScene::has_entity(const EntityId& id) const noexcept {
    return std::visit([this](const auto& v) -> bool {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, core::PanelId>) {
            return panel_objects_.find(v) != panel_objects_.end();
        } else {
            return hardware_objects_.find(v) != hardware_objects_.end();
        }
    }, id);
}

Handle(AIS_InteractiveObject) AisScene::ais_for_entity(const EntityId& id) const {
    return std::visit([this](const auto& v) -> Handle(AIS_InteractiveObject) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, core::PanelId>) {
            auto it = panel_objects_.find(v);
            return it == panel_objects_.end()
                ? Handle(AIS_InteractiveObject){}
                : Handle(AIS_InteractiveObject)(it->second);
        } else {
            auto it = hardware_objects_.find(v);
            return it == hardware_objects_.end()
                ? Handle(AIS_InteractiveObject){}
                : Handle(AIS_InteractiveObject)(it->second);
        }
    }, id);
}

void AisScene::select(const EntityId& id) {
    auto ais = ais_for_entity(id);
    if (ais.IsNull()) {
        throw core::DomainError{"renderer.unknown_entity_in_selection",
                                "Entity not in renderer scene"};
    }
    context_->AddOrRemoveSelected(ais, Standard_False);
}

void AisScene::deselect(const EntityId& id) {
    auto ais = ais_for_entity(id);
    if (ais.IsNull()) return;
    if (context_->IsSelected(ais)) {
        context_->AddOrRemoveSelected(ais, Standard_False);
    }
}

void AisScene::clear_selection() {
    context_->ClearSelected(Standard_False);
}

std::vector<EntityId> AisScene::selection() const {
    std::vector<EntityId> result;
    for (context_->InitSelected(); context_->MoreSelected(); context_->NextSelected()) {
        Handle(AIS_InteractiveObject) sel = context_->SelectedInteractive();
        auto it = ais_to_entity_.find(sel.get());
        if (it != ais_to_entity_.end()) result.push_back(it->second);
    }
    return result;
}
```

> В заголовке `ais_scene.cpp` подключить `<type_traits>` и `"coupecad/core/errors.h"`, если ещё нет.

- [ ] **Step 4: В `occt_renderer.cpp` делегировать в scene**

Заменить четыре заглушки:

```cpp
void OcctRenderer::select(const EntityId& id)   { scene_.select(id); }
void OcctRenderer::deselect(const EntityId& id) { scene_.deselect(id); }
void OcctRenderer::clear_selection()            { scene_.clear_selection(); }
std::vector<EntityId> OcctRenderer::selection() const { return scene_.selection(); }
```

- [ ] **Step 5: Собрать и запустить**

```bash
cmake --build --preset default --target coupecad_renderer_test
ctest --preset default -R OcctRendererSelectionTest --output-on-failure
```

Expected: 6 тестов, 6 passed.

- [ ] **Step 6: Закоммитить**

```bash
git add src/coupecad/renderer/occt/ais_scene.h \
        src/coupecad/renderer/occt/ais_scene.cpp \
        src/coupecad/renderer/occt/occt_renderer.cpp \
        tests/renderer/occt_renderer_selection_test.cpp \
        tests/renderer/CMakeLists.txt
git commit -m "feat(renderer): selection (select/deselect/clear/selection)"
```

---

## Task 12: Picking

**Files:**
- Modify: `src/coupecad/renderer/occt/ais_scene.h`
- Modify: `src/coupecad/renderer/occt/ais_scene.cpp`
- Modify: `src/coupecad/renderer/occt/occt_renderer.cpp`
- Create: `tests/renderer/occt_renderer_picking_test.cpp`
- Modify: `tests/renderer/CMakeLists.txt`

- [ ] **Step 1: Падающий тест `tests/renderer/occt_renderer_picking_test.cpp`**

```cpp
#include "coupecad/renderer/occt/occt_renderer.h"

#include "coupecad/core/panel.h"
#include "coupecad/core/project.h"
#include "coupecad/geometry/geometry_builder.h"

#include <gtest/gtest.h>

using coupecad::core::Millimeters;
using coupecad::core::Project;
using coupecad::geometry::GeometryBuilder;
using coupecad::renderer::CameraState;
using coupecad::renderer::EntityId;
using coupecad::renderer::ViewportSize;
using coupecad::renderer::occt::OcctRenderer;

namespace {

OcctRenderer make_renderer_with_one_bottom_panel(
    Project& project, GeometryBuilder& builder,
    coupecad::core::PanelId& panel_id_out) {
    auto& cab = project.mutable_cabinet();
    cab.dimensions = {Millimeters{1000}, Millimeters{500}, Millimeters{1500}};
    coupecad::core::Panel p;
    p.id = project.uuid_gen().next_id<coupecad::core::PanelIdTag>();
    p.role = coupecad::core::PanelRole::Bottom;
    cab.panels.emplace(p.id, p);
    panel_id_out = p.id;

    OcctRenderer r{project, builder};
    coupecad::core::ChangeSet cs;
    cs.added_panels.push_back(p.id);
    r.sync(cs);
    r.set_viewport_size(ViewportSize{800, 600});
    return r;
}

}  // namespace

TEST(OcctRendererPickingTest, PickOutsideViewportReturnsNullopt) {
    Project project = Project::create_empty("pick");
    GeometryBuilder builder{project};
    coupecad::core::PanelId pid;
    OcctRenderer r = make_renderer_with_one_bottom_panel(project, builder, pid);

    EXPECT_FALSE(r.pick(-1, 0).has_value());
    EXPECT_FALSE(r.pick(10000, 10000).has_value());
}

TEST(OcctRendererPickingTest, PickOnPanelReturnsItsId) {
    Project project = Project::create_empty("pick2");
    GeometryBuilder builder{project};
    coupecad::core::PanelId pid;
    OcctRenderer r = make_renderer_with_one_bottom_panel(project, builder, pid);

    // Камера сверху вниз: target в центре дна шкафа, eye высоко по Z.
    CameraState cam;
    cam.eye    = {Millimeters{500}, Millimeters{250}, Millimeters{5000}};
    cam.target = {Millimeters{500}, Millimeters{250}, Millimeters{0}};
    cam.up     = {Millimeters{0},   Millimeters{1},   Millimeters{0}};
    cam.fov_deg = 30.0;
    r.set_camera(cam);

    auto hit = r.pick(400, 300);   // центр viewport'а
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(*hit, EntityId{pid});
}
```

- [ ] **Step 2: Расширить `ais_scene` методом `pick_by_pixel`**

В `ais_scene.h` public:

```cpp
    // Picking. Координаты — пиксели в текущем viewport'е (top-left origin).
    // Возвращает std::nullopt, если ничего нет под курсором или координата
    // за пределами viewport'а.
    std::optional<EntityId> pick(int x, int y, const Handle(V3d_View)& view);
```

> В включения добавить `<optional>` и `<V3d_View.hxx>`.

В `ais_scene.cpp`:

```cpp
std::optional<EntityId> AisScene::pick(int x, int y, const Handle(V3d_View)& view) {
    // Bounds-check относительно текущего viewport'а.
    Standard_Integer w = 1, h = 1;
    view->Window()->Size(w, h);
    if (x < 0 || y < 0 || x >= w || y >= h) return std::nullopt;

    context_->MoveTo(static_cast<Standard_Integer>(x),
                     static_cast<Standard_Integer>(y),
                     view, Standard_False);
    if (!context_->HasDetected()) return std::nullopt;

    Handle(AIS_InteractiveObject) det = context_->DetectedInteractive();
    auto it = ais_to_entity_.find(det.get());
    if (it == ais_to_entity_.end()) return std::nullopt;
    return it->second;
}
```

- [ ] **Step 3: В `occt_renderer.cpp` заменить заглушку `pick`**

```cpp
std::optional<EntityId> OcctRenderer::pick(int x, int y) {
    return scene_.pick(x, y, driver_.view());
}
```

- [ ] **Step 4: Собрать и запустить**

```bash
cmake --build --preset default --target coupecad_renderer_test
ctest --preset default -R OcctRendererPickingTest --output-on-failure
```

Expected: 2 теста, 2 passed.

> Если на CI без GL `MoveTo` всё-таки бросает (selector требует frustum, а не GL — но реализация может зависеть от версии OCCT) — обернуть `pick()` в try/catch внутри `AisScene::pick`, логировать `warn renderer.pick_unavailable_headless` и возвращать `nullopt`. Это закроет окно нестабильности headless-CI.

- [ ] **Step 5: Закоммитить**

```bash
git add src/coupecad/renderer/occt/ais_scene.h \
        src/coupecad/renderer/occt/ais_scene.cpp \
        src/coupecad/renderer/occt/occt_renderer.cpp \
        tests/renderer/occt_renderer_picking_test.cpp \
        tests/renderer/CMakeLists.txt
git commit -m "feat(renderer): picking — pick(x, y) → optional<EntityId>"
```

---

## Task 13: Viewport size + render_to_image smoke

**Files:**
- Modify: `src/coupecad/renderer/occt/occt_renderer.cpp`
- Create: `tests/renderer/occt_renderer_render_smoke_test.cpp`
- Modify: `tests/renderer/CMakeLists.txt`

- [ ] **Step 1: Падающий тест `tests/renderer/occt_renderer_render_smoke_test.cpp`**

```cpp
#include "coupecad/renderer/occt/occt_renderer.h"

#include "coupecad/core/project.h"
#include "coupecad/geometry/geometry_builder.h"

#include <gtest/gtest.h>

using coupecad::core::Project;
using coupecad::geometry::GeometryBuilder;
using coupecad::renderer::ViewportSize;
using coupecad::renderer::occt::OcctRenderer;

TEST(OcctRendererViewportTest, SetThenGetRoundTrip) {
    Project project = Project::create_empty("vp");
    GeometryBuilder builder{project};
    OcctRenderer renderer{project, builder};

    renderer.set_viewport_size(ViewportSize{1280, 720});
    auto vp = renderer.viewport_size();
    EXPECT_EQ(vp.width, 1280);
    EXPECT_EQ(vp.height, 720);
}

TEST(OcctRendererViewportTest, ZeroOrNegativeSizeThrows) {
    Project project = Project::create_empty("vp2");
    GeometryBuilder builder{project};
    OcctRenderer renderer{project, builder};

    try {
        renderer.set_viewport_size(ViewportSize{0, 100});
        FAIL() << "expected DomainError";
    } catch (const coupecad::core::DomainError& e) {
        EXPECT_EQ(e.code(), "renderer.invalid_viewport_size");
    }
}

TEST(OcctRendererRenderSmokeTest, RenderToImageEitherEmptyOrFullBuffer) {
    Project project = Project::create_empty("render");
    GeometryBuilder builder{project};
    OcctRenderer renderer{project, builder};

    renderer.set_viewport_size(ViewportSize{64, 64});
    auto bytes = renderer.render_to_image();
    if (bytes.empty()) {
        // Headless: GL недоступен — рендерер вернул пусто (warning в логе).
        SUCCEED();
    } else {
        EXPECT_EQ(bytes.size(), static_cast<std::size_t>(64 * 64 * 4));
    }
}
```

- [ ] **Step 2: Реализовать viewport-методы и render_to_image в `occt_renderer.cpp`**

Заменить три заглушки:

```cpp
void OcctRenderer::set_viewport_size(ViewportSize size) {
    if (size.width <= 0 || size.height <= 0) {
        throw core::DomainError{
            "renderer.invalid_viewport_size",
            "Viewport size must be positive: w=" + std::to_string(size.width) +
                ", h=" + std::to_string(size.height)};
    }
    driver_.set_viewport_size(size.width, size.height);
}

ViewportSize OcctRenderer::viewport_size() const {
    return ViewportSize{driver_.viewport_width(), driver_.viewport_height()};
}

std::vector<std::uint8_t> OcctRenderer::render_to_image() {
    if (!driver_.gl_available()) {
        coupecad::logging::Logger::instance().warn(
            "renderer", "renderer.gl_unavailable: render_to_image returns empty");
        return {};
    }
    const int w = driver_.viewport_width();
    const int h = driver_.viewport_height();
    Image_PixMap image;
    image.InitZero(Image_Format_RGBA, w, h);
    if (!driver_.view()->ToPixMap(image, w, h, Graphic3d_BT_RGBA)) {
        coupecad::logging::Logger::instance().warn(
            "renderer", "renderer.render_to_image: ToPixMap failed");
        return {};
    }
    std::vector<std::uint8_t> out(static_cast<std::size_t>(w) *
                                   static_cast<std::size_t>(h) * 4u);
    std::memcpy(out.data(), image.Data(), out.size());
    return out;
}
```

> В `occt_renderer.cpp` подключить `<Image_PixMap.hxx>`, `<Graphic3d_BufferType.hxx>`, `<cstring>`, `<string>`.

- [ ] **Step 3: Собрать и запустить**

```bash
cmake --build --preset default --target coupecad_renderer_test
ctest --preset default -R "OcctRendererViewportTest|OcctRendererRenderSmokeTest" --output-on-failure
```

Expected: 3 теста, 3 passed (на CI без GL — `RenderToImageEitherEmptyOrFullBuffer` пройдёт по empty-ветке).

- [ ] **Step 4: Закоммитить**

```bash
git add src/coupecad/renderer/occt/occt_renderer.cpp \
        tests/renderer/occt_renderer_render_smoke_test.cpp \
        tests/renderer/CMakeLists.txt
git commit -m "feat(renderer): viewport size + render_to_image smoke"
```

---

## Task 14: Финальная сборка, прогон всех тестов, coverage

**Files:** none (только верификация и доработки, если что-то упало)

- [ ] **Step 1: Полный clean-rebuild**

```bash
rm -rf build/default
conan install . --build=missing -s build_type=Debug
cmake --preset default
cmake --build --preset default
```

Expected: всё компилируется, ноль warning'ов от наших файлов.

- [ ] **Step 2: Полный прогон тестов**

```bash
ctest --preset default --output-on-failure
```

Expected: всех тестов = 249 (Stage 2) + примерно 38 новых:
- entity_id: 3
- view_driver: 4
- material_resolver: 4
- ais_scene: 8
- occt_renderer_sync: 7
- occt_renderer_camera: 3
- occt_renderer_selection: 6
- occt_renderer_picking: 2
- occt_renderer_render_smoke: 3 (viewport×2 + render×1)

= **287 тестов**, все зелёные.

- [ ] **Step 3: Проверить отсутствие линкажа Qt в renderer**

Run:
```bash
nm build/default/src/coupecad/renderer/occt/libcoupecad_renderer_occt.a 2>/dev/null | grep -c " Q[A-Z]"
```

Expected: `0`. На macOS/Windows аналогично через соответствующий tool (`dumpbin /SYMBOLS` на Windows, `nm` на macOS).

- [ ] **Step 4: Проверить, что `make_occt_renderer` действительно выдаёт unique_ptr**

Sanity-checked в тестах через `Project::create_empty` + `OcctRenderer{...}`. Дополнительно — добавить один точечный тест в `tests/renderer/occt_renderer_sync_test.cpp` (просто чтобы factory была exercised):

```cpp
TEST(OcctRendererFactory, MakeOcctRendererReturnsNonNull) {
    Project project = Project::create_empty("factory");
    GeometryBuilder builder{project};
    auto r = coupecad::renderer::occt::make_occt_renderer(project, builder);
    EXPECT_TRUE(static_cast<bool>(r));
}
```

И прогнать:
```bash
cmake --build --preset default --target coupecad_renderer_test
ctest --preset default -R OcctRendererFactory --output-on-failure
```

Expected: 1 test passed.

- [ ] **Step 5: Coverage (опционально, локально)**

Если установлен `llvm-cov` или `gcovr`, запустить локальный coverage-pass на `coupecad_renderer_occt`. Целевой минимум — **60% lines**. Если ниже — проверить, какие ветки не покрыты, добавить тесты.

> Coverage-job в CI оставлен warning-only (см. Stage 1c) — здесь это локальная проверка перед PR.

- [ ] **Step 6: Закоммитить factory-тест (если ещё не закоммичен)**

```bash
git add tests/renderer/occt_renderer_sync_test.cpp
git commit -m "test(renderer): smoke test для make_occt_renderer factory"
```

- [ ] **Step 7: Запушить ветку и открыть PR**

```bash
git push -u origin stage-3-renderer
gh pr create --title "Stage 3: Renderer (OCCT backend)" --body "$(cat <<'EOF'
## Summary

Stage 3 реализует рендер-слой поверх Stage 2 геометрии: `IRenderer` интерфейс +
OCCT-бэкенд через `AIS_InteractiveContext`/`V3d_View`/`OpenGl_GraphicDriver`.

- Spec и плак: `docs/superpowers/specs/2026-05-04-stage-3-renderer-design.md`,
  `docs/superpowers/plans/2026-05-06-stage-3-renderer.md`.
- Новый CMake-таргет `coupecad_renderer_occt`, селектор `COUPECAD_RENDERER`.
- ChangeSet-driven sync, camera, picking, selection, цвет панелей из
  `Material.color_hint`, фурнитура — фиксированный металлический серый.
- Headless-fallback: рендерер работает без GL — все логические тесты проходят
  на CI без X-сервера.
- Тестов: +38 (всего 287).

## Test plan

- [ ] CI: Linux build passes
- [ ] CI: Windows build passes
- [ ] Все 287 тестов зелёные на обеих платформах
- [ ] Нет линкажа Qt в `coupecad_renderer_occt`

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

Это завершает Stage 3. Stage 4 (UI / QML viewport) подключит `IRenderer` к `apps/coupecad/`.

---

## Финальная самопроверка

Cross-check выполнен по разделам спеки:
- §1 Цель и объём → Tasks 1-13 покрывают весь in-scope.
- §2 Структура модуля → Task 1 (CMake skeleton), Task 2-13 заполняют файлы.
- §3 Public API → Task 2 (EntityId), Task 3 (IRenderer), Task 8 (factory).
- §4 ChangeSet sync → Task 9.
- §5 OCCT internals → Task 4 (view_driver), Task 5-7 (ais_scene, material_resolver), Task 10 (camera), Task 11 (selection), Task 12 (picking), Task 13 (render_to_image).
- §6 Conan/CMake → Task 1, Task 4 (CMake детали бэкенда).
- §7 Тестирование → Tasks 2, 4-13 (по тестовому файлу на компонент).
- §8 Логирование → встроено в Tasks 4, 6, 9, 13.
- §9 Коды ошибок → `renderer.material_not_found` (Task 5), `renderer.unknown_entity_in_selection` (Task 11), `renderer.invalid_viewport_size` (Task 13). `renderer.driver_init_failed` — strict-mode не реализуется в Stage 3 (см. §10).
- §11 Definition of Done → Task 14 (полный clean-rebuild + прогон + Qt-checkup).

Открытый вопрос §10 пункт 5 (strict-mode `make_occt_renderer`) — намеренно не реализован в Stage 3, поскольку нет потребителя для него; добавится при необходимости в Stage 10 (batch thumbnails).
