# Stage 2 — Geometry layer (OpenCASCADE) — Design

**Дата:** 2026-04-22
**Статус:** Утверждено для реализации
**Связанные документы:**
- [`2026-04-18-coupecad-design.md`](./2026-04-18-coupecad-design.md) — общая концепция CoupeCAD
- [`2026-04-20-stage-1-core-domain-design.md`](./2026-04-20-stage-1-core-domain-design.md) — модель домена Stage 1
- Планы Stage 1a/1b/1c — реализованы, тесты зелёные

---

## 1. Цель и объём Stage 2

Stage 2 добавляет **геометрический слой** поверх ядра домена: преобразование декларативного `Project` (Stage 1) в твёрдотельную геометрию OpenCASCADE Technology (OCCT).

### 1.1 В объёме

- Библиотека `coupecad_geometry` — отдельная статическая библиотека, зависит от `coupecad_core` и `OpenCASCADE`.
- **Панели как боксы** (`BRepPrimAPI_MakeBox`) — все 15 ролей панелей через `compute_panel_geometry` Stage 1a.
- **Корпус шкафа как `TopoDS_Compound`** — объединение всех панельных боксов в единое тело.
- **Фурнитура как bbox-солиды** — крепёж, петли и т.п. представляются прямоугольными параллелепипедами по `HardwareSpec.bbox`. Каждый `HardwareItem` имеет один или несколько `PanelAttachment` (привязка к панели в panel-local СК) — для каждого attachment строится свой solid в мировой СК шкафа через композицию panel-transform × attachment-transform.
- **Stateful builder с per-panel кешем** и инвалидацией по `ChangeSet`.
- **Pull-модель** доставки изменений: внешний код (Stage 4 UI) явно вызывает `apply_changes(cs)`.
- **OCCT в PUBLIC API**: `TopoDS_Shape`, `TopoDS_Solid`, `TopoDS_Compound` видны консьюмерам.

### 1.2 Вне объёма

- Булевы операции (вырезание присадок/пазов) — отложено до Stage 5+ (кройка/присадка).
- Тесселяция (триангуляция) для рендера — Stage 3 (renderer).
- Вьюер OCCT (`V3d_View`) — Stage 3.
- Фаски, скругления, edge banding в геометрии — пока только визуальный атрибут панели.
- DataExchange (STEP/IGES экспорт) — Stage 10.
- Application Framework (OCAF) — не используется.
- Сетки / mesher — не используется.

### 1.3 Не делим на 2a/2b

Stage 2 — единая стадия (~10–12 задач). Разделение преждевременно: scope зафиксирован, нет внутренних точек естественного релиза.

---

## 2. Структура модуля

### 2.1 Файлы и расположение

```
src/coupecad/geometry/
    CMakeLists.txt
    geometry_builder.h            # public API
    geometry_builder.cpp
    panel_shape.h                 # build_panel_solid(...) - инкапсулирует MakeBox
    panel_shape.cpp
    cabinet_shape.h               # сборка compound'а
    cabinet_shape.cpp
    hardware_shape.h              # build_hardware_solid(...)
    hardware_shape.cpp
    occt_helpers.h                # перевод coupecad::core::Vec3 ↔ gp_Pnt, mm-int ↔ double
    occt_helpers.cpp
```

### 2.2 CMake таргет

```cmake
# src/coupecad/geometry/CMakeLists.txt
find_package(OpenCASCADE REQUIRED)

add_library(coupecad_geometry STATIC
    geometry_builder.cpp
    panel_shape.cpp
    cabinet_shape.cpp
    hardware_shape.cpp
    occt_helpers.cpp
)

target_include_directories(coupecad_geometry
    PUBLIC ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(coupecad_geometry
    PUBLIC
        coupecad_core
        coupecad_logging
        # OCCT в PUBLIC: TopoDS_Shape светится наружу
        TKernel TKMath TKG2d TKG3d TKGeomBase TKBRep TKTopAlgo TKPrim
)

target_compile_features(coupecad_geometry PUBLIC cxx_std_20)
```

> **Замечание:** конкретный набор OCCT-таргетов (`TKernel`, `TKMath`, `TKBRep`, `TKTopAlgo`, `TKPrim`, `TKG3d`, `TKGeomBase`, `TKG2d`) подобран под минимум, необходимый для `BRepPrimAPI_MakeBox` + `BRep_Builder` + compound-операций. В плане задаче «настройка CMake» — проверить точный набор линковки на trial-build.

### 2.3 Зависимости (направленный граф)

```
coupecad_geometry  →  coupecad_core
                  →  coupecad_logging
                  →  OpenCASCADE (PUBLIC)
```

`coupecad_core` остаётся независимым от OCCT — сборка ядра без OCCT по-прежнему возможна (что важно для headless-тестов и потенциального запуска ядра в скриптах).

---

## 3. Координатная система и единицы

### 3.1 Перевод mm-int ↔ OCCT

OCCT работает в `double`. Внутренняя единица CoupeCAD — `core::Millimeters` (`int32_t`, см. `units.h`). Все числа OCCT мы интерпретируем **как миллиметры** (`Standard_Real == double mm`), без масштабирования.

Хелперы в `occt_helpers.h`:

```cpp
namespace coupecad::geometry {

// Перевод доменного Vec3 (mm-int) в OCCT gp_Pnt (mm-double).
gp_Pnt to_occt_point(const core::Vec3& v);

// Перевод размеров панели в три double-аргумента BRepPrimAPI_MakeBox.
struct BoxDims { double dx, dy, dz; };
BoxDims to_box_dims(const core::Vec3& size);

}  // namespace coupecad::geometry
```

### 3.2 Z-up

Stage 1 явно зафиксировал Z-up (X — вправо, Y — вглубь, Z — вверх). Это нативная ось OCCT (`gp::OZ()`), без поворота фрейма. Origin шкафа — `(0, 0, 0)`, левый-нижний-передний угол.

### 3.3 Кватернионы и orientation

`PanelGeometry::orientation` — `core::Quat`. Для role-based панелей всегда identity ⇒ преобразуем `BRepPrimAPI_MakeBox` напрямую без поворота. Для `Custom` панелей (orientation ≠ identity) применяем `gp_Trsf::SetRotation(gp_Quaternion)` через `BRepBuilderAPI_Transform`. Хелпер:

```cpp
gp_Trsf to_occt_transform(const core::Vec3& origin, const core::Quat& orientation);
```

---

## 4. Public API: `GeometryBuilder`

### 4.1 Заголовок `geometry_builder.h`

```cpp
#pragma once

#include "coupecad/core/cabinet.h"
#include "coupecad/core/commands/change_set.h"
#include "coupecad/core/hardware.h"
#include "coupecad/core/id.h"
#include "coupecad/core/panel.h"
#include "coupecad/core/project.h"

#include <TopoDS_Compound.hxx>
#include <TopoDS_Solid.hxx>

#include <unordered_map>

namespace coupecad::geometry {

// Stateful: владеет per-panel и per-hardware кешем TopoDS_Solid.
// Ссылается на Project снаружи (не владеет). Вызовы apply_changes(cs)
// инвалидируют закешированные solid'ы по id из ChangeSet.
//
// Не thread-safe. Все вызовы должны идти из одного потока (UI).
class GeometryBuilder {
public:
    explicit GeometryBuilder(const core::Project& project);

    // Перестроить кеш с нуля. Вызывается после загрузки проекта (Stage 1c).
    void rebuild_all();

    // Применить инкрементальную дельту: инвалидировать соответствующие
    // entries и пересобрать их при следующем запросе. Compound шкафа
    // помечается грязным, если cabinet_changed или любая панель
    // была добавлена/удалена/изменена.
    void apply_changes(const core::ChangeSet& cs);

    // Получить геометрию одной панели. Если в кеше нет — построить и закешировать.
    // Бросает core::DomainError, если compute_panel_geometry падает (роль/параметры
    // несовместимы с cabinet).
    const TopoDS_Solid& panel_solid(const core::PanelId& id);

    // Геометрия фурнитуры: один TopoDS_Compound на HardwareItem.
    // Compound содержит по одному TopoDS_Solid на каждый PanelAttachment
    // (фурнитура может быть привязана к нескольким панелям сразу — например,
    // петля к двери и боковине). Позиция каждого solid'а — в мировой СК
    // шкафа, через композицию panel transform × attachment transform.
    const TopoDS_Compound& hardware_compound(const core::HardwareItemId& id);

    // Compound всего шкафа: панели + фурнитура. Пересобирается лениво,
    // если хоть один компонент инвалидирован.
    const TopoDS_Compound& cabinet_compound();

    // Для тестов / диагностики.
    bool has_cached_panel(const core::PanelId& id) const noexcept;
    std::size_t cache_size() const noexcept;

private:
    const core::Project& project_;
    std::unordered_map<core::PanelId, TopoDS_Solid> panel_cache_;
    std::unordered_map<core::HardwareItemId, TopoDS_Compound> hardware_cache_;
    TopoDS_Compound cabinet_compound_;
    bool compound_dirty_ = true;
};

}  // namespace coupecad::geometry
```

### 4.2 Семантика возвратов

- Возвращаем `const TopoDS_Solid&` (а не копию) — OCCT shape'ы используют COW (`TopoDS_TShape*` по shared_ptr внутри). Консьюмер может скопировать `TopoDS_Solid` тривиально (это handle).
- Время жизни ссылки — до следующего `apply_changes(...)` или `rebuild_all()`. Документируется в комментарии.

### 4.3 Что **не** делает Builder

- Не хранит ссылку на `UndoStack`. Вызывающий код пробрасывает `ChangeSet`, полученный из `UndoStack::execute/undo/redo/commit_preview`.
- Не имеет методов мутации (`add_panel`/`remove_panel`/...) — единственный «вход» это `apply_changes`.
- Не подписан на `IProjectObserver`. Это сознательное ограничение Stage 2 (Pull-модель).

---

## 5. Кеш и инвалидация

### 5.1 Ключи кешей

- `panel_cache_: unordered_map<PanelId, TopoDS_Solid>` — по одному solid на панель.
- `hardware_cache_: unordered_map<HardwareItemId, TopoDS_Compound>` — compound на каждый HardwareItem (внутри по одному solid'у на каждый PanelAttachment).
- `cabinet_compound_: TopoDS_Compound` — общий compound шкафа, плюс булев флаг `compound_dirty_`.

> **Замечание про hardware-инвалидацию:** так как hardware-attachments позиционируются относительно панели, изменение **панели** сдвигает и привязанную к ней фурнитуру. Поэтому `apply_changes` инвалидирует hardware_cache не только по `cs.updated_hardware`, но и по любой панели в `cs.updated_panels`/`cs.removed_panels`/`cs.cabinet_changed`. Реализация — в §5.2.

### 5.2 Алгоритм `apply_changes(const ChangeSet& cs)`

```
for id in cs.removed_panels:    panel_cache_.erase(id)
for id in cs.removed_hardware:  hardware_cache_.erase(id)
for id in cs.updated_panels:    panel_cache_.erase(id)         # перестроится по запросу
for id in cs.updated_hardware:  hardware_cache_.erase(id)

# Hardware-attachments позиционируются относительно панели. Сдвинулась
# панель → надо пересчитать привязанную фурнитуру. Без анализа графа
# (panel → attached items) делаем консервативно: чистим hardware_cache
# целиком, если что-то менялось среди панелей.
if (!cs.updated_panels.empty() || !cs.removed_panels.empty() || cs.cabinet_changed) {
    hardware_cache_.clear();
}

if cs.cabinet_changed:
    # Изменились dimensions шкафа → role-based панели могут получить
    # другую геометрию (Bottom/Top/LeftSide/...). Без анализа разности
    # инвалидируем всё.
    panel_cache_.clear()

# added_panels / added_hardware — не трогаем кеш, build on demand.
# materials (added/removed/updated) — влияют на цвет/текстуру (Stage 3),
# не на shape. Игнорируются.

if !cs.empty():  # любая дельта влияет на компаунд
    compound_dirty_ = true
```

### 5.3 `rebuild_all()`

Очищает оба кеша + ставит `compound_dirty_ = true`. Не строит ничего eagerly — следующий `panel_solid` / `cabinet_compound` сделает работу. Используется после `JsonProjectSerializer::deserialize()`.

### 5.4 Lazy сборка `cabinet_compound()`

Если `compound_dirty_` ⇒ заново строим `TopoDS_Compound` через `BRep_Builder::MakeCompound` + `Add(...)` для всех текущих `panel_solid(id)` и `hardware_compound(id)` (compound добавляется как один shape). Это автоматически прогревает оба кеша для тех id, которых там ещё не было. Сбрасываем флаг.

### 5.5 Edge cases

- `Custom`-панель с невалидными параметрами (нулевой size) ⇒ `compute_panel_geometry` бросит `DomainError` ⇒ Builder перебрасывает дальше; в кеш ничего не кладёт.
- Двойной `apply_changes` с одинаковым id в `updated_panels` — идемпотентен (erase повторно — no-op).

---

## 6. Построение шейпов

### 6.1 Панели (`panel_shape.cpp`)

```cpp
TopoDS_Solid build_panel_solid(const core::Cabinet& cabinet,
                               const core::Panel& panel) {
    const auto pg = core::compute_panel_geometry(cabinet, panel);
    const auto dims = to_box_dims(pg.size);

    BRepPrimAPI_MakeBox box(gp_Pnt(0, 0, 0), dims.dx, dims.dy, dims.dz);
    TopoDS_Solid local = box.Solid();

    // Position the box: translate by origin, rotate by orientation.
    const gp_Trsf trsf = to_occt_transform(pg.origin, pg.orientation);
    return TopoDS::Solid(BRepBuilderAPI_Transform(local, trsf, /*Copy=*/false).Shape());
}
```

Никакой role-логики в geometry-слое **нет** — она вся уже сидит в Stage 1a `compute_panel_geometry`. Это критично: единый источник правды для расположения панелей.

### 6.2 Фурнитура (`hardware_shape.cpp`)

```cpp
// Возвращает compound: по одному TopoDS_Solid на каждый PanelAttachment.
TopoDS_Compound build_hardware_compound(const core::Project& project,
                                        const core::HardwareItem& item) {
    auto spec_it = project.hardware_catalog().find(item.ref);
    if (spec_it == project.hardware_catalog().end()) {
        throw core::DomainError("hardware spec not found: " + item.ref.value);
    }
    const core::Vec3& bbox = spec_it->second.bbox;

    TopoDS_Compound compound;
    BRep_Builder bb;
    bb.MakeCompound(compound);

    const auto dx = static_cast<double>(bbox.x.value());
    const auto dy = static_cast<double>(bbox.y.value());
    const auto dz = static_cast<double>(bbox.z.value());

    for (const auto& att : item.attachments) {
        // panel_id → PanelGeometry: позиция и ориентация панели в шкафу.
        const auto& panel = project.cabinet().panels.at(att.panel_id);
        const auto pg = core::compute_panel_geometry(project.cabinet(), panel);

        // World transform = panel transform ∘ attachment transform.
        // (att.local_position и att.orientation — в panel-local frame).
        const gp_Trsf panel_trsf = to_occt_transform(pg.origin, pg.orientation);
        const gp_Trsf attach_trsf = to_occt_transform(att.local_position, att.orientation);
        const gp_Trsf world_trsf = panel_trsf.Multiplied(attach_trsf);

        BRepPrimAPI_MakeBox box(gp_Pnt(0, 0, 0), dx, dy, dz);
        TopoDS_Shape positioned = BRepBuilderAPI_Transform(box.Solid(), world_trsf, /*Copy=*/false).Shape();
        bb.Add(compound, positioned);
    }
    return compound;
}
```

> Если у `HardwareItem` пустой список attachments, `Cabinet::validate()` уже бросает `DomainError` (см. `hardware.h:62`), так что эта функция вызывается только на валидных items.

### 6.3 Compound шкафа (`cabinet_shape.cpp`)

```cpp
TopoDS_Compound build_cabinet_compound(GeometryBuilder& builder,
                                       const core::Project& project) {
    TopoDS_Compound compound;
    BRep_Builder bb;
    bb.MakeCompound(compound);

    // panels — unordered_map<PanelId, Panel>.
    for (const auto& [panel_id, panel] : project.cabinet().panels) {
        bb.Add(compound, builder.panel_solid(panel_id));
    }
    // hardware — unordered_map<HardwareItemId, HardwareItem>.
    for (const auto& [hw_id, hw] : project.cabinet().hardware) {
        bb.Add(compound, builder.hardware_compound(hw_id));
    }
    return compound;
}
```

Метод `cabinet_compound()` Builder'а вызывает эту функцию (с `*this`), кеширует результат.

**Замечание про порядок**: `unordered_map` не гарантирует стабильный порядок итерации между запусками. Для compound'а это не критично (mathematical equivalence), но если в будущем понадобится детерминированный shape (например, для регрессионных тестов через хеш) — отсортировать ключи.

---

## 7. Зависимости и сборка

### 7.1 OCCT через Conan

Добавляем в `conanfile.py`:

```python
self.requires("opencascade/7.9.1")
```

**Опции** (определить точно при первом trial-install):
- `with_freetype=False` (нужно только для AIS-визуализации, в Stage 2 нет вьюера)
- `with_tk=False` (Tcl/Tk — не нужно)
- Если есть опция типа `with_data_exchange=False` — выключаем (STEP/IGES не сейчас).

### 7.2 Риск: время сборки OCCT

OCCT — крупный проект. Сборка из исходников (`--build=missing`) на CI runner'е может занять 30+ минут на первой сборке. **Митигация**:

1. Conan-кеш `actions/cache@v4` уже настроен и работает по `hashFiles('conanfile.py')` — после первого успешного билда кеш переиспользуется.
2. При первом запуске CI после добавления OCCT — таймаут джоба поднимаем с 60 до **120 минут** (в `.github/workflows/ci.yml` `timeout-minutes: 120`).
3. Если prebuilt-биндели OCCT доступны для нашего профиля (Linux GCC 11, Windows MSVC 2022, x86_64) — `--build=missing` подтянет их без локальной компиляции.
4. **Если** prebuilt-ов нет и сборка падает / висит дольше 90 минут — повторяем тактику, что использовали с Qt: исследуем альтернативу (system OCCT, vcpkg, либо ставим `7.6.2` / `7.6.0` где prebuilt'ы вероятнее). Решение принимается во время выполнения плана, не сейчас.

> Это **открытый риск** — фиксируется в §10. Не блокируем спеку.

### 7.3 macOS

macOS-job по-прежнему закомментирован в `.github/workflows/ci.yml` (см. комментарии в файле). При попытке локальной сборки на macOS используем тот же Conan-профиль; AGL-shim уже есть в `cmake/stubs/`. OCCT 7.9.1 поддерживает macOS arm64.

---

## 8. Тестирование

Тесты OCCT — отдельный челлендж: нет вьюера, нельзя «посмотреть глазами». Полагаемся на инвариантные проверки.

### 8.1 Файлы

```
tests/geometry/
    CMakeLists.txt
    occt_helpers_test.cpp           # to_occt_point, to_box_dims, to_occt_transform
    panel_shape_test.cpp            # build_panel_solid: bbox, volume, type
    hardware_shape_test.cpp         # build_hardware_solid аналогично
    cabinet_shape_test.cpp          # compound: количество солидов, общий bbox
    geometry_builder_test.cpp       # кеш, инвалидация, apply_changes
```

### 8.2 Базовые инварианты

Для каждого построенного `TopoDS_Solid`:

```cpp
EXPECT_EQ(solid.ShapeType(), TopAbs_SOLID);

GProp_GProps vol_props;
BRepGProp::VolumeProperties(solid, vol_props);
EXPECT_NEAR(vol_props.Mass(), expected_volume_mm3, 1e-3);

Bnd_Box bbox;
BRepBndLib::Add(solid, bbox);
double xmin, ymin, zmin, xmax, ymax, zmax;
bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);
EXPECT_NEAR(xmin, expected_origin.x, 1e-6);
// ... и так далее по 6 граням
```

### 8.3 Тесты `GeometryBuilder`

- `panel_solid` строит и кеширует: повторный вызов возвращает тот же handle (`solid1.IsSame(solid2)` или сравнение `TShape*`).
- `apply_changes` с `updated_panels = {id1}` инвалидирует только `id1`: `cache_size()` для остальных не меняется.
- `apply_changes` с `cabinet_changed = true` чистит весь panel-кеш.
- `cabinet_compound()` пересобирается, если что-то менялось; не пересобирается, если `apply_changes(empty)`.
- `rebuild_all()` после фейка-мутации модели возвращает корректный compound.

### 8.4 Compound тесты

- `cabinet_compound()` содержит ровно `N + M` подшейпов, где `N = panels.size()`, `M = hardware_items.size()`.
- Bounding box compound'а равен union'у bbox'ов всех компонент.

### 8.5 Без вьюера

В Stage 2 **не** добавляем визуальную проверку (вьюер — Stage 3). Для отладки разработчик может подключить локальный mini-script через OCCT `BRepTools::Write` (запись BREP-файла), но это не часть тестов.

---

## 9. Логирование

Используем существующий `coupecad::logging::Logger` со специальной категорией `"geometry"`.

- `Trace`: каждый `panel_solid` build (id, размеры).
- `Debug`: `apply_changes` с количеством инвалидированных entries.
- `Info`: `rebuild_all` (полная пересборка) — редкое событие.
- `Warn`: пустой compound (нет ни панелей, ни фурнитуры).
- `Error`: `DomainError` пробрасываемый из `compute_panel_geometry` (logged + rethrown).

---

## 10. Открытые вопросы / риски

1. **Точный набор Conan-опций для OCCT 7.9.1** — определяется при первом `conan install --build=missing`. Если prebuilt-бинари требуют другого набора опций — корректируем.
2. **Время сборки OCCT в CI** — см. §7.2. Если первый CI-run превысит 90 мин и prebuilt'ов нет — даунгрейд до 7.6.2 либо переход на system-OCCT (по аналогии с Qt-сагой).
3. **Точный набор `TKxxx` таргетов для линковки** — может варьироваться по версиям OCCT. Trial-build покажет.
4. **Поддержка Custom-панелей с не-identity orientation** — реализуется сразу, но в реальности использоваться начнёт только в Stage 5+ (UI редактирования произвольных панелей). Тест для одной случайной ориентации обязателен.
5. ~~**Hardware orientation**~~ — проверено: `HardwareItem` имеет `attachments: vector<PanelAttachment>`, каждый attachment несёт свои `local_position` и `orientation`. Размещение фурнитуры — относительно панели, не свободное.

---

## 11. Критерии готовности Stage 2

- [ ] `coupecad_geometry` собирается на Linux + Windows CI (macOS — по возможности локально).
- [ ] OCCT 7.9.1 установлен через Conan (или принято обоснованное решение о fallback'е).
- [ ] Все 15 ролей панелей покрыты тестами `panel_shape_test`.
- [ ] `GeometryBuilder` тесты покрывают: lazy build, инвалидацию по каждому полю `ChangeSet`, `cabinet_changed`, `rebuild_all`.
- [ ] Compound шкафа корректно содержит все панели + всю фурнитуру.
- [ ] Логирование работает по категории `"geometry"`.
- [ ] Не сломаны существующие 214+ тестов Stage 1.
