# Stage 3 — Renderer (OCCT backend) — Design

**Дата:** 2026-05-04
**Статус:** Утверждено для реализации
**Связанные документы:**
- [`2026-04-18-coupecad-design.md`](./2026-04-18-coupecad-design.md) — общая концепция CoupeCAD
- [`2026-04-20-stage-1-core-domain-design.md`](./2026-04-20-stage-1-core-domain-design.md) — модель домена Stage 1
- [`2026-04-22-stage-2-geometry-design.md`](./2026-04-22-stage-2-geometry-design.md) — `GeometryBuilder` и OCCT shape'ы

---

## 1. Цель и объём Stage 3

Stage 3 добавляет **рендер-слой** поверх геометрии Stage 2: визуализацию TopoDS-шейпов через OpenCASCADE Visualization (`AIS_InteractiveContext` + `V3d_View`), управление камерой, picking и selection. Без QML-интеграции — это Stage 4.

### 1.1 В объёме

- **Интерфейсная библиотека `coupecad_renderer`** — заголовок `IRenderer` + `EntityId`, без зависимостей на OCCT (PUBLIC API виден всем потенциальным консьюмерам).
- **Реализация `coupecad_renderer_occt`** — статическая библиотека, обёртка над OCCT Visualization (`AIS_InteractiveContext`, `V3d_Viewer`, `V3d_View`, `OpenGl_GraphicDriver`).
- **ChangeSet-driven sync** — рендерер потребляет тот же `core::ChangeSet`, что и `GeometryBuilder` Stage 2; внешний код вызывает `renderer.sync(cs)` симметрично `builder.apply_changes(cs)`.
- **Камера** — `set_camera`, `camera`, `fit_all`. Z-up по умолчанию, изометрический ракурс при первой загрузке.
- **Picking** — `pick(x, y) → optional<EntityId>`, где `EntityId = variant<PanelId, HardwareItemId>`.
- **Selection state** — `select`/`deselect`/`clear_selection`/`selection()`, владение состоянием в `AIS_InteractiveContext`.
- **Цвет панелей** — flat shading из `core::Material.color_hint`. Фурнитура — фиксированный металлический серый.
- **Offscreen render** — `render_to_image(w, h) → bytes` (RGBA8) для будущих thumbnails и экспорта; в тестах не assert'ится по пикселям.
- **Headless-fallback** — рендерер конструируется и работает без GL-драйвера (всё кроме `render_to_image` — логические структуры AIS/SelectMgr, не требуют framebuffer'а). Это ключ к тестам в CI без Mesa.
- **CMake-селектор бэкенда** — `COUPECAD_RENDERER=occt|qq3d` уже объявлен с Stage 0; в этом стейдже подключается реальная реализация `occt`. `qq3d` даёт явную CMake-ошибку (приходит в Stage 11).

### 1.2 Вне объёма

- QML-вьюпорт, `QQuickFramebufferObject`/`QOpenGLWidget`-обёртка — Stage 4.
- PBR-материалы, текстуры, HDRI, IBL — Stage 11 (бэкенд Qt Quick 3D).
- Тесселяция-тюнинг и LOD — отложено до оптимизационного прохода после Stage 4, когда появится визуальная обратная связь.
- `qq3d`-бэкенд — Stage 11.
- Image-golden тесты, screenshot-сравнения между прогонами CI — out of scope.
- Mesh-export (OBJ/STL из триангуляции) — Stage 10 (`io/`).

### 1.3 Не делим

Stage 3 — единая стадия (~10–11 задач). Нет внутренних точек естественного релиза: интерфейс `IRenderer` без работающей реализации бесполезен; `sync` без `pick`/`selection` — половина задачи; разделение породило бы фейковые milestones.

---

## 2. Структура модуля

### 2.1 Файлы и расположение

```
src/coupecad/renderer/
    CMakeLists.txt              # селектор бэкенда по COUPECAD_RENDERER
    i_renderer.h                # PUBLIC: интерфейс, без OCCT-типов
    entity_id.h                 # variant<PanelId, HardwareItemId> + helpers
    entity_id.cpp               # std::hash<EntityId> definition
    occt/
        CMakeLists.txt
        occt_renderer.h         # public-class header (возвращается из фабрики)
        occt_renderer.cpp
        ais_scene.h             # internal: владелец AIS_InteractiveContext, sync-логика
        ais_scene.cpp
        material_resolver.h     # internal: PanelId → Quantity_Color
        material_resolver.cpp
        view_driver.h           # internal: V3d_Viewer/V3d_View/Aspect_NeutralWindow + OpenGL driver
        view_driver.cpp
```

### 2.2 CMake-таргеты

`src/coupecad/renderer/CMakeLists.txt` — селектор и интерфейс-таргет:

```cmake
add_library(coupecad_renderer STATIC entity_id.cpp)
target_include_directories(coupecad_renderer PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(coupecad_renderer PUBLIC coupecad_core)
target_compile_features(coupecad_renderer PUBLIC cxx_std_20)

if(COUPECAD_RENDERER STREQUAL "occt")
    add_subdirectory(occt)
elseif(COUPECAD_RENDERER STREQUAL "qq3d")
    message(FATAL_ERROR "COUPECAD_RENDERER=qq3d backend lands in Stage 11")
else()
    message(FATAL_ERROR "Unknown COUPECAD_RENDERER=${COUPECAD_RENDERER}")
endif()
```

`src/coupecad/renderer/occt/CMakeLists.txt`:

```cmake
add_library(coupecad_renderer_occt STATIC
    occt_renderer.cpp
    ais_scene.cpp
    material_resolver.cpp
    view_driver.cpp
)

target_link_libraries(coupecad_renderer_occt
    PUBLIC
        coupecad_renderer
        coupecad_geometry
        coupecad_logging
    PRIVATE
        # OCCT visualization (поверх Stage 2 TKBRep/TKTopAlgo/TKPrim/etc.)
        TKV3d TKOpenGl TKService TKHLR TKMeshVS
)

target_compile_features(coupecad_renderer_occt PUBLIC cxx_std_20)
```

### 2.3 Зависимости (направленный граф)

```
coupecad_renderer  (interface)        →  coupecad_core
coupecad_renderer_occt                →  coupecad_renderer
                                      →  coupecad_geometry
                                      →  coupecad_logging
                                      →  OCCT visualization (PRIVATE)
```

OCCT visualization (`TKV3d`, `TKOpenGl`, ...) — **PRIVATE**: в публичные заголовки `coupecad_renderer_occt` (он же `occt_renderer.h`) типы `AIS_*`/`V3d_*` не утекают; всё, что видит консьюмер, — это `IRenderer` и фабрика `make_occt_renderer`.

`coupecad_geometry` остаётся независимым от Visualization-стека OCCT — рендерер потребляет его, не наоборот.

---

## 3. Public API

### 3.1 `i_renderer.h`

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
    core::Vec3  eye;          // mm, world coords
    core::Vec3  target;       // mm
    core::Vec3  up;           // unit, default {0, 0, 1}
    double      fov_deg;      // 0 = orthographic; >0 = perspective
};

struct ViewportSize {
    int width;
    int height;
};

// Stateful, single-threaded, не thread-safe.
// Время жизни: рендерер хранит non-owning ссылки на Project и GeometryBuilder
// (передаются в фабрику-конструктор бэкенда). Оба должны пережить рендерер.
class IRenderer {
public:
    virtual ~IRenderer() = default;

    // Применить дельту: добавить/удалить/обновить AIS-объекты соответствующих
    // panel_id/hardware_id. Шейпы тянутся из GeometryBuilder, переданного в
    // конструктор. Алгоритм — см. §4.
    virtual void sync(const core::ChangeSet& cs) = 0;

    // Полная пересборка AIS-сцены с нуля (после deserialize() или сильного
    // сбоя инвариантов). Симметрично GeometryBuilder::rebuild_all.
    virtual void rebuild_all() = 0;

    // Камера.
    virtual void           set_camera(const CameraState&) = 0;
    virtual CameraState    camera() const = 0;
    virtual void           fit_all() = 0;     // фрейм cabinet bbox

    // Picking. Координаты — пиксели в текущем viewport'е (origin top-left).
    // Возвращает std::nullopt, если ничего нет под курсором.
    virtual std::optional<EntityId> pick(int x, int y) = 0;

    // Selection.
    virtual void                          select(const EntityId&) = 0;
    virtual void                          deselect(const EntityId&) = 0;
    virtual void                          clear_selection() = 0;
    virtual std::vector<EntityId>         selection() const = 0;

    // Viewport size — влияет на projection picking'а и offscreen render.
    virtual void           set_viewport_size(ViewportSize) = 0;
    virtual ViewportSize   viewport_size() const = 0;

    // Offscreen render → RGBA8, row-major, top-left origin. Размер буфера =
    // viewport_size().width * viewport_size().height * 4. Если GL недоступен,
    // возвращает пустой vector + warning в лог.
    virtual std::vector<std::uint8_t> render_to_image() = 0;
};

}  // namespace coupecad::renderer
```

### 3.2 `entity_id.h`

```cpp
#pragma once

#include "coupecad/core/id.h"

#include <variant>

namespace coupecad::renderer {

using EntityId = std::variant<core::PanelId, core::HardwareItemId>;

}  // namespace coupecad::renderer

namespace std {

template <>
struct hash<coupecad::renderer::EntityId> {
    std::size_t operator()(const coupecad::renderer::EntityId& id) const noexcept;
};

}  // namespace std
```

### 3.3 Фабрика `occt_renderer.h`

```cpp
#pragma once

#include "coupecad/renderer/i_renderer.h"

#include <memory>

namespace coupecad::core { class Project; }
namespace coupecad::geometry { class GeometryBuilder; }

namespace coupecad::renderer::occt {

// Создать OCCT-рендерер. project и builder должны пережить рендерер.
// При невозможности инициализировать GL-драйвер рендерер всё равно
// возвращается (логический режим: sync/pick/selection работают,
// render_to_image() возвращает пустой буфер с warning в лог).
std::unique_ptr<IRenderer> make_occt_renderer(const core::Project& project,
                                              geometry::GeometryBuilder& builder);

}  // namespace coupecad::renderer::occt
```

### 3.4 Семантика и инварианты

- После `sync(cs)` множества id в AIS-сцене равны множествам ключей `project.cabinet().panels` и `.hardware`.
- `selection()` сохраняется через `update`/`replace` для тех id, что остались в сцене; для удалённых — выселяется.
- `EntityId` в `select`/`deselect` для отсутствующего id → `core::DomainError{"renderer.unknown_entity_in_selection", ...}`.
- `set_viewport_size({0, _})` или `({_, 0})` → `core::DomainError{"renderer.invalid_viewport_size", ...}`.
- `pick(x, y)` за границами viewport'а → `std::nullopt`, не throw.
- `render_to_image()` без GL → пустой `vector<uint8_t>` + `warn`-лог.

---

## 4. ChangeSet sync

### 4.1 Алгоритм

```
sync(cs):
    log debug counts (тот же формат, что GeometryBuilder.apply_changes)

    # Удаления — освободить AIS handle до повторного добавления.
    for id in cs.removed_panels:    ais_scene_.remove_panel(id)
    for id in cs.removed_hardware:  ais_scene_.remove_hardware(id)

    # Updates = remove + add. AIS handle swap дёшев.
    for id in cs.updated_panels:    ais_scene_.replace_panel(id, builder_.panel_solid(id))
    for id in cs.updated_hardware:  ais_scene_.replace_hardware(id, builder_.hardware_compound(id))

    # Adds.
    for id in cs.added_panels:      ais_scene_.add_panel(id, builder_.panel_solid(id))
    for id in cs.added_hardware:    ais_scene_.add_hardware(id, builder_.hardware_compound(id))

    # cabinet_changed → role-based панели могли получить новую геометрию.
    # Консервативно: replace всё, что сейчас в сцене (синхронно с подходом
    # GeometryBuilder §5.2).
    if cs.cabinet_changed:
        for id in ais_scene_.panel_ids():
            ais_scene_.replace_panel(id, builder_.panel_solid(id))
        for id in ais_scene_.hardware_ids():
            ais_scene_.replace_hardware(id, builder_.hardware_compound(id))

    # Material-only дельты: re-resolve color, без rebuild геометрии.
    for mat_id in cs.added_materials ∪ cs.removed_materials ∪ cs.updated_materials:
        ais_scene_.refresh_colors_for_material(mat_id)

    if !cs.empty():
        ais_scene_.invalidate_view()    # помечает V3d_View грязным, без redraw сейчас
```

### 4.2 Edge cases

- Двойной `sync` с пересекающимися id (`removed` и `added` одного id) — обрабатывается корректно за счёт порядка операций (remove first → add later).
- `sync(empty)` — no-op, без логов.
- `cabinet_changed` без изменений в `panels`/`hardware` — пересборка существующих в сцене ids, новых не появляется.
- Materials: если поменялся `default_panel_material` шкафа, это идёт через `cs.cabinet_changed` (а не через `updated_materials`); `refresh_colors_for_material` дополнительно ловит изменения цвета конкретного материала.

### 4.3 `rebuild_all()`

```
rebuild_all():
    log info
    ais_scene_.clear()
    selection_ = empty
    for [id, panel] in project_.cabinet().panels:
        ais_scene_.add_panel(id, builder_.panel_solid(id))
    for [id, hw] in project_.cabinet().hardware:
        ais_scene_.add_hardware(id, builder_.hardware_compound(id))
    ais_scene_.invalidate_view()
```

Builder отвечает за то, чтобы свои кеши тоже были актуальны (вызывающий код обычно делает `builder_.rebuild_all()` непосредственно перед `renderer_.rebuild_all()`).

### 4.4 Цветовое разрешение (`material_resolver`)

- **Панель:** `effective_material_id = panel.material_override.value_or(cabinet.default_panel_material)`. Лукап в `project.materials()`. `Quantity_Color` строится из `material.color_hint` (RGBA, alpha игнорируется на текущем этапе).
- **Фурнитура:** фиксированный `Quantity_Color(0.55, 0.57, 0.60, Quantity_TOC_RGB)` (металлический серый). `HardwareSpec` пока не имеет color-поля — это в открытых вопросах §10.
- **Missing material id:** `core::DomainError{"renderer.material_not_found", "Panel <id> references missing material <mat_id>"}`. Согласовано с конвенцией кодов ошибок core (lowercase, dot-separated). Бросается во время `add_panel`/`replace_panel` (ленивое разрешение цвета на момент построения AIS-объекта). По инвариантам Core (Stage 1 §3.4) удалить материал, используемый панелью, нельзя — поэтому в честных сценариях этот error не должен возникать; срабатывает только при corrupted state.

---

## 5. OCCT internals

### 5.1 `view_driver` — V3d setup

- `OpenGl_GraphicDriver` — process-wide singleton, lazy-init через `Aspect_DisplayConnection`. Один driver на весь процесс достаточен; несколько `V3d_Viewer` могут разделять его.
- `V3d_Viewer` — на каждый `OcctRenderer`.
- `V3d_View` — на каждый `OcctRenderer`, прикреплён к `Aspect_NeutralWindow` (нет нативного окна). Это даёт корректную projection-математику для picking без необходимости создавать GL-surface.
- `set_viewport_size(w, h)` → `Aspect_NeutralWindow::SetSize(w, h)` + `view_->MustBeResized()`.
- `render_to_image()` → `view_->ToPixMap(image, w, h, Graphic3d_BT_RGBA)` — единственный путь, который реально растеризует. В тестах не assert'ится по пикселям, но smoke-тестируется на non-throw на 64×64 буфере, если GL доступен.
- **Headless-fallback:** если `Aspect_DisplayConnection`/`OpenGl_GraphicDriver` инициализация бросает (типично на CI без X-сервера), `view_driver` ловит exception, логирует `warn` и переходит в **logical mode**:
  - `V3d_Viewer`/`V3d_View` всё равно создаются (они не требуют GL для in-memory структур).
  - `Aspect_NeutralWindow` создаётся.
  - `render_to_image()` короткозамыкается на `return {}` + warn.
  - Picking и selection продолжают работать через `SelectMgr_ViewerSelector` — тот работает на projection-математике, не требует framebuffer.

### 5.2 `ais_scene` — AIS state

- Владеет `Handle(AIS_InteractiveContext)`, привязанным к `view_driver_->viewer()`.
- Два словаря для быстрого лукапа:
  - `unordered_map<core::PanelId, Handle(AIS_Shape)> panel_objects_`
  - `unordered_map<core::HardwareItemId, Handle(AIS_Shape)> hardware_objects_`
- Обратный словарь `unordered_map<AIS_InteractiveObject*, EntityId> ais_to_entity_` — нужен `pick()` и `selection()` для перевода AIS-объектов обратно в `EntityId`. Ключ — raw-pointer внутри `Handle(...)` (валидный пока handle жив; синхронно очищается в `remove_*`/`replace_*`).
- `add_*`/`remove_*`/`replace_*` идут через `AIS_InteractiveContext::Display`/`Erase`/`Remove` с `updateViewer=Standard_False` (batch-redraw откладывается до `invalidate_view`, который тоже не редрозит сразу — это задача внешнего кода в Stage 4, который позовёт `view_->Redraw()` на нужном тике).
- Цвет: на каждый `AIS_Shape` ставится `Quantity_Color` через `SetColor(...)`. Атрибут `Graphic3d_AspectFillArea3d` дефолтный (без PBR).

### 5.3 Picking

- `pick(x, y)` → `context_->MoveTo(x, y, view_, /*redraw=*/false)`; если `context_->HasDetected()`, лукап `context_->DetectedInteractive()` в `ais_to_entity_` → возврат `EntityId`.
- В headless-режиме `MoveTo` работает: `SelectMgr_ViewerSelector` использует projection-матрицу `V3d_View` без обращения к framebuffer'у.
- Композитные shape'ы: `HardwareItem` — это `TopoDS_Compound` из нескольких bbox-солидов (по одному на attachment). Pick любого из подсолидов возвращает родительский `HardwareItemId` — гранулярность по umbrella-объекту, не по attachment'у.

### 5.4 Selection

- `select(id)`: лукап AIS-объекта в `panel_objects_`/`hardware_objects_`, `context_->AddOrRemoveSelected(obj, Standard_False)`. Если id отсутствует в сцене → `DomainError`.
- `deselect(id)`: симметрично, `AddOrRemoveSelected` снимет выделение, если оно было.
- `clear_selection()` → `context_->ClearSelected(Standard_False)`.
- `selection()` → итерация `context_->InitSelected(); MoreSelected(); NextSelected()`, перевод через `ais_to_entity_`.

### 5.5 Камера

- Default initial state (применяется в конструкторе): `eye=(2000,-2000,2000)`, `target=(0,0,0)`, `up=(0,0,1)`, `fov_deg=45` — изометрический ракурс. Переопределяется на первом `fit_all()` после загрузки проекта.
- `set_camera(state)`: записывает в `view_->Camera()` (Eye, Center, Up, FOVy). FOV=0 → orthographic (`Camera::SetProjectionType(Orthographic)`); FOV>0 → perspective.
- `camera()`: чтение обратно из `view_->Camera()`.
- `fit_all()` → `view_->FitAll()` (использует bbox всех AIS-объектов).

### 5.6 Тесселяция

- На текущем этапе — дефолты OCCT (`Prs3d_Drawer::SetMaximalChordialDeviation`, авто-пропорциональное).
- Если после Stage 4 окажется, что 60 FPS на Iris Xe / 200 панелей не держится из-за избыточной триангуляции — добавляем явный `Prs3d_Drawer` в `ais_scene` (открытый риск §10).
- При создании AIS-объекта проверяется триангуляция через `BRepMesh_IncrementalMesh`; если получается > 100k треугольников, пишем `info`-лог (диагностика, не error).

---

## 6. Зависимости и сборка

### 6.1 Conan

`conanfile.py` **не меняется**. Пакет `opencascade/7.9.1`, добавленный в Stage 2, уже включает Visualization-компоненты (`TKV3d`, `TKOpenGl`, `TKService`, `TKHLR`, `TKMeshVS`). Достаточно линкануть их в `coupecad_renderer_occt`.

### 6.2 OCCT viz транзитивные зависимости

- **Linux**: `TKOpenGl` тянет `libGL`, `libGLX`, `libX11`, `libXext`, `libXmu`. CI на Ubuntu 22.04 — после Stage 2 коммита `b395e24` Conan имеет sudo-права на установку X11-зависимостей; того же набора достаточно для viz.
- **Windows**: `TKOpenGl` тянет `opengl32.lib` (системный, в SDK).
- **macOS**: `TKOpenGl` использует CGL/AGL. macOS в CI отключён (Stage 0/2 решение); локальная сборка должна работать с уже существующим `cmake/stubs/AGL.framework`.

### 6.3 Корневой CMakeLists

В `src/CMakeLists.txt` добавляется четвёртой строкой `add_subdirectory(coupecad/renderer)` (после `logging`, `core`, `geometry`). `apps/coupecad/CMakeLists.txt` пока не линкуется с рендерером — это Stage 4.

---

## 7. Тестирование

### 7.1 Файлы

```
tests/renderer/
    CMakeLists.txt
    occt_renderer_sync_test.cpp        # sync(ChangeSet): id'шники в AIS-сцене
    occt_renderer_selection_test.cpp   # select/deselect/clear/selection round-trip
    occt_renderer_picking_test.cpp     # pick() для синтетической camera+viewport
    occt_renderer_camera_test.cpp      # set_camera/camera/fit_all
    material_resolver_test.cpp         # color: override → cabinet default → throw
    ais_scene_test.cpp                 # internal: id ↔ AIS handle invariants
    occt_renderer_render_smoke_test.cpp  # render_to_image: либо bytes, либо warn
```

### 7.2 Базовые инварианты

Для каждого теста, который трогает scene state:

```cpp
EXPECT_EQ(renderer.has_in_scene_panel_count(), expected_count);
EXPECT_TRUE(renderer.has_in_scene_panel(panel_id));
EXPECT_FALSE(renderer.has_in_scene_panel(removed_id));
```

Эти диагностические геттеры — на отдельном `OcctRendererInspector`-фасаде (только для тестов, не часть `IRenderer`).

### 7.3 Тесты `sync(ChangeSet)`

- `sync` с `added_panels = {id1, id2}` после пустой сцены → 2 панели в сцене.
- `sync` с `removed_panels = {id1}` → 1 панель остаётся, id1 нет.
- `sync` с `updated_panels = {id1}` → панель остаётся, AIS handle сменился (assert через before/after pointer comparison через inspector).
- `sync` с `cabinet_changed=true` → все panel-AIS-объекты пересобраны.
- `sync` с `updated_materials = {mat1}` → AIS-объекты не пересобираются, но цвет панели с этим материалом обновляется.
- `sync` с `removed_materials` для используемого материала → `DomainError` при следующем `refresh_colors_for_material` лукапе. (Альтернативно: материал может быть удалён только если не используется ни одной панелью — это инвариант Core. Тест проверяет: corrupted state ⇒ throw, без графической сцены в полусломанном виде.)

### 7.4 Selection и picking

- `select(panel_id)` → `selection()` содержит этот id.
- `select(unknown_id)` → throw `renderer.unknown_entity_in_selection`.
- `select` → `remove` этого id (через `sync`) → `selection()` больше не содержит id.
- Picking: установить камеру, чтобы конкретная панель занимала центр viewport'а; `pick(viewport.center)` возвращает её `EntityId`.
- Picking за пределами viewport → `nullopt`.

### 7.5 Camera

- `set_camera(s)` → `camera() == s` (с допуском на double-rounding).
- `fit_all()` после загрузки проекта → камера обрамляет cabinet bbox (assert: bbox-проекция целиком в viewport'е, ±N% margin).

### 7.6 Render smoke

Один тест вызывает `render_to_image()`:
- Если буфер пустой → `EXPECT_TRUE(captured_logs_contain("renderer.gl_unavailable"))` — заварка fallback'а сработала.
- Если буфер непустой → `EXPECT_EQ(buffer.size(), 64 * 64 * 4)`.

Это позволяет тесту проходить и на CI без GL, и на локальной машине разработчика.

### 7.7 Coverage

- `coupecad_renderer_occt` ≥ **60% lines** (ниже 70% Stage 2 потому, что `view_driver` GL-пути исполняются только в одном smoke-тесте; все логические пути должны быть покрыты).
- Все публичные методы `IRenderer` затронуты как минимум одним тестом.

### 7.8 Без вьюера / без Qt

Тесты — обычные GoogleTest-бинарники, без `QT_QPA_PLATFORM`. Рендерер Qt-free; единственный графический компонент в нём — OCCT, который через headless-fallback не требует X-сервера.

---

## 8. Логирование

Категория `"renderer"` в `coupecad::logging::Logger`.

| Уровень | Когда |
|---|---|
| `Info` | `make_occt_renderer` ctor: GL-driver init success/fallback. `rebuild_all()`. |
| `Debug` | `sync(cs)` с counts добавлений/удалений/обновлений (тот же формат, что `GeometryBuilder.apply_changes`). |
| `Trace` | per-id add/remove/replace; pick hits; selection deltas. |
| `Warn` | GL-driver init failed → logical-mode fallback (`renderer.gl_unavailable`). Панель с triangulation > 100k треугольников. |
| `Error` | `core::DomainError` пробрасываемый из `GeometryBuilder` во время `sync` (logged + rethrown). |

---

## 9. Коды ошибок

Все — подклассы `core::DomainError`, lowercase, dot-separated (по конвенции core):

| Код | Когда |
|---|---|
| `renderer.material_not_found` | Панель ссылается на отсутствующий material id |
| `renderer.unknown_entity_in_selection` | `select(id)` для id, отсутствующего в AIS-сцене |
| `renderer.invalid_viewport_size` | `set_viewport_size({0, _})` или `({_, 0})` |
| `renderer.driver_init_failed` | (опциональный strict-mode) Если caller передал флаг `strict_gl=true`, и GL не поднялся |

Дефолтное поведение `make_occt_renderer` — `strict_gl=false`: фолбэк, не throw. Strict-mode используется в местах, где рендерер обязан быть полнофункциональным (например, batch-генерация thumbnails в Stage 10).

---

## 10. Открытые вопросы / риски

1. **Picking-гранулярность для composite hardware.** `HardwareItem` — `TopoDS_Compound` из bbox-солидов на каждый attachment. Picking возвращает родительский `HardwareItemId`, не attachment-индекс. Если в Stage 4 UX потребует «выделить конкретную петлю в door+side группе» — расширим `EntityId` опциональным attachment-индексом (`variant<PanelId, HardwareItemPick>`, где `HardwareItemPick = {id, optional<attachment_index>}`). Не блокируем Stage 3.
2. **Тесселяция-дефолты.** Авто-deflection OCCT может оказаться слишком грубой для крупных корпусов или слишком тонкой для мелкой фурнитуры. Реальные данные появятся только в Stage 4 (UI). Если визуальное качество страдает — добавляем `Prs3d_Drawer`-тюнинг как отдельный пост-Stage-3 коммит.
3. **Цвет фурнитуры.** `HardwareSpec` сейчас не имеет color-поля; рендерер использует фиксированный металлический серый. Расширение `HardwareSpec` потребует bump'а формата `.ccad` (Stage 1c golden files), поэтому решается либо в рамках Stage 6 (catalogs), либо как отдельная мини-задача после Stage 5.
4. **CI macOS.** macOS-job по-прежнему отключён (Stage 0/2 решение). Линкаж OCCT visualization на macOS не покрывается CI; локальная сборка проверяется вручную разработчиком.
5. **GL в CI.** Все тесты Stage 3 — логические; ни один не валидирует, что реальный рендеринг через GL работает на CI runner'е. Опционально в будущем — добавить smoke-job с Mesa swrast (низкий приоритет, дублирует то, что и так проверится при первом запуске app в Stage 4).
6. **AIS handle pointer как ключ обратного словаря.** Требует синхронной очистки `ais_to_entity_` при `remove_*`/`replace_*`, чтобы не оставлять dangling-ключи. Тест на это явный (`ais_scene_test`).

---

## 11. Критерии готовности Stage 3

- [ ] `coupecad_renderer` (interface) + `coupecad_renderer_occt` собираются на Linux + Windows CI.
- [ ] Все методы `IRenderer` покрыты тестами.
- [ ] `OcctRenderer` корректно отслеживает AIS scene state по каждому полю `ChangeSet` (added/removed/updated panels & hardware, cabinet_changed, materials).
- [ ] Picking возвращает корректный `EntityId` для known camera + viewport.
- [ ] Цвет панелей разрешается из effective `Material.color_hint` (override → cabinet default → throw).
- [ ] Логирование работает по категории `"renderer"`.
- [ ] Покрытие `coupecad_renderer_occt` ≥ 60% lines.
- [ ] Не сломаны существующие 249 тестов Stage 2.
- [ ] В `coupecad_renderer_occt` нет ни одной зависимости от Qt.
- [ ] `COUPECAD_RENDERER=qq3d` даёт чистую CMake-ошибку (без полусобранного таргета).
- [ ] Headless-fallback: тесты проходят на CI runner'е без X-сервера / GL.
