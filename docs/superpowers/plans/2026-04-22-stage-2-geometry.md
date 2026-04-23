# Stage 2 — Geometry layer (OpenCASCADE) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Добавить новую статическую библиотеку `coupecad_geometry`, которая преобразует декларативный `core::Project` (Stage 1) в `TopoDS_Shape` OpenCASCADE — панели как боксы, шкаф как `TopoDS_Compound`, фурнитура как bbox-солиды по PanelAttachment.

**Architecture:** Stateful `GeometryBuilder` владеет двумя кешами (`PanelId → TopoDS_Solid`, `HardwareItemId → TopoDS_Compound`) + lazy `cabinet_compound`. ChangeSet-driven инвалидация через явный `apply_changes(cs)` (Pull-модель). Реюзим `core::compute_panel_geometry` для всех 15 ролей панелей. OCCT экспортируется PUBLIC — `TopoDS_Shape` светится консьюмерам.

**Tech Stack:** OpenCASCADE Technology 7.9.1 (через Conan), C++20, CMake, GoogleTest.

**Связанная спека:** [`docs/superpowers/specs/2026-04-22-stage-2-geometry-design.md`](../specs/2026-04-22-stage-2-geometry-design.md).

---

## Файловая структура

**Новые файлы:**

```
src/coupecad/geometry/
    CMakeLists.txt               # добавляет coupecad_geometry STATIC
    occt_helpers.h               # to_occt_point/to_box_dims/to_occt_transform
    occt_helpers.cpp
    panel_shape.h                # build_panel_solid(cabinet, panel)
    panel_shape.cpp
    hardware_shape.h             # build_hardware_compound(project, item)
    hardware_shape.cpp
    cabinet_shape.h              # build_cabinet_compound(builder, project)
    cabinet_shape.cpp
    geometry_builder.h           # public API
    geometry_builder.cpp

tests/geometry/
    CMakeLists.txt
    occt_helpers_test.cpp
    panel_shape_test.cpp
    hardware_shape_test.cpp
    cabinet_shape_test.cpp
    geometry_builder_test.cpp
```

**Модифицируемые файлы:**

- `conanfile.py` — добавить `self.requires("opencascade/7.9.1")`
- `src/CMakeLists.txt` — добавить `add_subdirectory(coupecad/geometry)`
- `tests/CMakeLists.txt` — добавить `add_subdirectory(geometry)`
- `.github/workflows/ci.yml` — поднять `timeout-minutes: 60` до `120` для обоих job'ов (первая сборка OCCT может быть долгой)

---

## Соглашения для всех задач

- **Namespace всех новых файлов:** `coupecad::geometry`.
- **Headers с include guard'ом `#pragma once`** (как в `coupecad_core`).
- **Coding style:** следовать существующему `coupecad_core` (snake_case для функций/переменных, PascalCase для типов, комментарии — на русском языке).
- **TDD цикл:** test → red → impl → green → commit.
- **Коммит формата:** `feat(geometry): ...`, `chore(stage-2): ...`, `test(geometry): ...`. Все коммиты в конце включают co-author trailer:
  ```
  Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
  ```

---

## Task 1: Conan + CMake-скелет, proof-of-life OCCT

**Files:**
- Modify: `conanfile.py`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Modify: `.github/workflows/ci.yml`
- Create: `src/coupecad/geometry/CMakeLists.txt`
- Create: `src/coupecad/geometry/occt_helpers.h`
- Create: `src/coupecad/geometry/occt_helpers.cpp`
- Create: `tests/geometry/CMakeLists.txt`
- Create: `tests/geometry/occt_helpers_test.cpp`

Эта задача — единственная, где TDD-цикл частично сжат (создание lib + первый proof-of-life тест в одном шаге), потому что без скомпонованного таргета невозможно прогнать ни один OCCT-тест.

- [ ] **Step 1: Добавить OCCT в conanfile.py**

Изменить `conanfile.py`:

```python
from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, cmake_layout


class CoupeCADConan(ConanFile):
    name = "coupecad"
    version = "0.1.0"
    settings = "os", "compiler", "build_type", "arch"

    # Stage 0: только GoogleTest. Qt подключается извне (aqtinstall/install-qt-action).
    # Stage 1a: добавляются spdlog (логирование), fmt (форматирование),
    # stduuid (UUID для id.h).
    # Stage 1c: nlohmann_json + libzip для .ccad I/O.
    # Stage 2: OpenCASCADE для геометрического слоя.
    def requirements(self):
        self.requires("spdlog/1.13.0")
        self.requires("fmt/10.2.1")
        self.requires("stduuid/1.2.3")
        self.requires("nlohmann_json/3.11.3")
        self.requires("libzip/1.10.1")
        self.requires("opencascade/7.9.1")
        self.test_requires("gtest/1.14.0")

    def layout(self):
        cmake_layout(self)
        self.folders.build = "build/default"
        self.folders.generators = "build/default"

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self, generator="Ninja")
        tc.generate()
```

- [ ] **Step 2: Поднять CI timeout с 60 до 120 минут**

В `.github/workflows/ci.yml` для job'а `build-linux` и `build-windows` (а также в закомментированном `build-macos`) заменить `timeout-minutes: 60` на `timeout-minutes: 120`. OCCT — крупная библиотека; первая сборка из исходников может занять 30–90 минут на CI runner'е. После того как Conan-кеш прогреется (`actions/cache@v4` ключ `conan-linux-${{ hashFiles('conanfile.py') }}`), последующие билды будут быстрыми.

```yaml
  build-linux:
    name: Linux (Ubuntu 22.04, GCC, Qt 6.7.3)
    runs-on: ubuntu-22.04
    timeout-minutes: 120
    ...

  build-windows:
    name: Windows (windows-2022, MSVC 2022, Qt 6.7.3)
    runs-on: windows-2022
    timeout-minutes: 120
    ...
```

(Закомментированный `build-macos` тоже обновить для согласованности.)

- [ ] **Step 3: Прогнать `conan install` локально, чтобы убедиться, что OCCT 7.9.1 ставится**

```bash
conan install . --build=missing -s build_type=Debug -s compiler.cppstd=20
```

Ожидание: команда успешно завершается. Если для текущего профиля нет prebuilt'а OCCT — `--build=missing` начнёт собирать из исходников (может занять 30+ минут). Если сборка падает, попробовать понизить версию: `self.requires("opencascade/7.6.2")` и повторить.

> **Если 7.9.1 не собирается ни с какой версией:** документировать причину в комментарии в `conanfile.py`, выбрать рабочую версию (7.6.2 → 7.6.0 → 7.5.0) и продолжить с ней. Спека §10 п1–2 явно этот риск допускает.

- [ ] **Step 4: Создать `src/coupecad/geometry/occt_helpers.h`**

```cpp
#pragma once

#include "coupecad/core/units.h"

#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

namespace coupecad::geometry {

// Размеры бокса в double-миллиметрах для BRepPrimAPI_MakeBox.
struct BoxDims {
    double dx = 0.0;
    double dy = 0.0;
    double dz = 0.0;
};

// Перевод доменного Vec3 (mm-int) в OCCT gp_Pnt (mm-double).
// Internal unit совпадает: oba — миллиметры, без масштабирования.
gp_Pnt to_occt_point(const core::Vec3& v) noexcept;

// Перевод size-вектора в три аргумента BRepPrimAPI_MakeBox.
BoxDims to_box_dims(const core::Vec3& size) noexcept;

// Композиция translation(origin) ∘ rotation(orientation).
// orientation — w-first quaternion из core::Quat. Identity orientation
// даёт чистую трансляцию.
gp_Trsf to_occt_transform(const core::Vec3& origin,
                          const core::Quat& orientation) noexcept;

}  // namespace coupecad::geometry
```

- [ ] **Step 5: Создать `src/coupecad/geometry/occt_helpers.cpp` (заглушки, чтобы lib собиралась)**

```cpp
#include "coupecad/geometry/occt_helpers.h"

#include <gp_Quaternion.hxx>
#include <gp_Vec.hxx>

namespace coupecad::geometry {

gp_Pnt to_occt_point(const core::Vec3& v) noexcept {
    return gp_Pnt(static_cast<double>(v.x.value()),
                  static_cast<double>(v.y.value()),
                  static_cast<double>(v.z.value()));
}

BoxDims to_box_dims(const core::Vec3& size) noexcept {
    return BoxDims{static_cast<double>(size.x.value()),
                   static_cast<double>(size.y.value()),
                   static_cast<double>(size.z.value())};
}

gp_Trsf to_occt_transform(const core::Vec3& origin,
                          const core::Quat& orientation) noexcept {
    gp_Trsf trsf;
    // Поворот первым в локальной СК, потом перенос в мировую.
    const gp_Quaternion q(orientation.x, orientation.y, orientation.z, orientation.w);
    trsf.SetRotation(q);
    trsf.SetTranslationPart(gp_Vec(static_cast<double>(origin.x.value()),
                                   static_cast<double>(origin.y.value()),
                                   static_cast<double>(origin.z.value())));
    return trsf;
}

}  // namespace coupecad::geometry
```

- [ ] **Step 6: Создать `src/coupecad/geometry/CMakeLists.txt`**

```cmake
find_package(OpenCASCADE REQUIRED)

add_library(coupecad_geometry STATIC
    occt_helpers.cpp
)

target_include_directories(coupecad_geometry
    PUBLIC ${CMAKE_SOURCE_DIR}/src
)

# OCCT в PUBLIC: TopoDS_Shape светится консьюмерам.
# Подключаем минимум, нужный для bbox-операций. Если линковка падает
# на отсутствие символа — добавить недостающий TKxxx-таргет.
target_link_libraries(coupecad_geometry
    PUBLIC
        coupecad_core
        coupecad_logging
        TKernel
        TKMath
        TKG2d
        TKG3d
        TKGeomBase
        TKBRep
        TKTopAlgo
        TKPrim
)

target_compile_features(coupecad_geometry PUBLIC cxx_std_20)
```

- [ ] **Step 7: Прописать subdir в `src/CMakeLists.txt`**

Текущий `src/CMakeLists.txt`:
```cmake
add_subdirectory(coupecad/logging)
add_subdirectory(coupecad/core)
```

После правки:
```cmake
add_subdirectory(coupecad/logging)
add_subdirectory(coupecad/core)
add_subdirectory(coupecad/geometry)
```

- [ ] **Step 8: Создать `tests/geometry/CMakeLists.txt`**

```cmake
add_executable(coupecad_geometry_test
    occt_helpers_test.cpp
)

target_link_libraries(coupecad_geometry_test
    PRIVATE
        coupecad_geometry
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_geometry_test)
```

- [ ] **Step 9: Прописать subdir в `tests/CMakeLists.txt`**

Текущий:
```cmake
find_package(GTest REQUIRED)

add_subdirectory(smoke)
add_subdirectory(app)
add_subdirectory(logging)
add_subdirectory(core)
```

После:
```cmake
find_package(GTest REQUIRED)

add_subdirectory(smoke)
add_subdirectory(app)
add_subdirectory(logging)
add_subdirectory(core)
add_subdirectory(geometry)
```

- [ ] **Step 10: Создать proof-of-life тест `tests/geometry/occt_helpers_test.cpp`**

```cpp
#include "coupecad/geometry/occt_helpers.h"

#include <gtest/gtest.h>

using coupecad::core::Millimeters;
using coupecad::core::Quat;
using coupecad::core::Vec3;
using coupecad::geometry::to_occt_point;

TEST(OcctHelpersTest, ToOcctPoint_PreservesIntegerMillimetersAsDoubles) {
    const Vec3 v{Millimeters{600}, Millimeters{500}, Millimeters{2000}};
    const auto p = to_occt_point(v);
    EXPECT_DOUBLE_EQ(p.X(), 600.0);
    EXPECT_DOUBLE_EQ(p.Y(), 500.0);
    EXPECT_DOUBLE_EQ(p.Z(), 2000.0);
}
```

- [ ] **Step 11: Сконфигурировать и собрать**

```bash
cmake --preset default
cmake --build --preset default
```

Ожидание: всё собирается, включая `coupecad_geometry` и `coupecad_geometry_test`. Если линковка падает на отсутствующий OCCT-символ — добавить недостающий `TKxxx` в `target_link_libraries` и повторить.

- [ ] **Step 12: Прогнать тесты**

```bash
ctest --preset default --output-on-failure
```

Ожидание: новый тест `OcctHelpersTest.ToOcctPoint_PreservesIntegerMillimetersAsDoubles` проходит. Все существующие 214+ тестов Stage 1 продолжают проходить.

- [ ] **Step 13: Commit**

```bash
git add conanfile.py .github/workflows/ci.yml \
        src/CMakeLists.txt tests/CMakeLists.txt \
        src/coupecad/geometry/ tests/geometry/
git commit -m "$(cat <<'EOF'
chore(stage-2): подключить OpenCASCADE 7.9.1 и создать coupecad_geometry

Заводим новую статическую библиотеку coupecad/geometry со скелетом
occt_helpers (только to_occt_point пока). Это фундамент для последующих
задач Stage 2: panel_shape, hardware_shape, cabinet_shape, GeometryBuilder.

- conanfile.py: + opencascade/7.9.1
- ci.yml: timeout-minutes 60 → 120 (первая сборка OCCT долгая)
- src/coupecad/geometry/: lib + occt_helpers скелет
- tests/geometry/: smoke-тест на to_occt_point

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: occt_helpers — `to_box_dims`

**Files:**
- Test: `tests/geometry/occt_helpers_test.cpp`

`to_box_dims` уже реализована в Task 1 (как часть скелета). Эта задача добавляет к ней регрессионный тест.

- [ ] **Step 1: Дописать тест в `tests/geometry/occt_helpers_test.cpp`**

```cpp
#include "coupecad/geometry/occt_helpers.h"

using coupecad::geometry::to_box_dims;

TEST(OcctHelpersTest, ToBoxDims_PreservesPositiveSizes) {
    const Vec3 size{Millimeters{800}, Millimeters{600}, Millimeters{16}};
    const auto d = to_box_dims(size);
    EXPECT_DOUBLE_EQ(d.dx, 800.0);
    EXPECT_DOUBLE_EQ(d.dy, 600.0);
    EXPECT_DOUBLE_EQ(d.dz, 16.0);
}

TEST(OcctHelpersTest, ToBoxDims_ZeroVectorYieldsZeroBox) {
    const Vec3 size{};
    const auto d = to_box_dims(size);
    EXPECT_DOUBLE_EQ(d.dx, 0.0);
    EXPECT_DOUBLE_EQ(d.dy, 0.0);
    EXPECT_DOUBLE_EQ(d.dz, 0.0);
}
```

- [ ] **Step 2: Прогнать**

```bash
cmake --build --preset default
ctest --preset default --output-on-failure -R OcctHelpersTest
```

Ожидание: оба новых теста зелёные.

- [ ] **Step 3: Commit**

```bash
git add tests/geometry/occt_helpers_test.cpp
git commit -m "$(cat <<'EOF'
test(geometry): покрыть to_box_dims регрессионными тестами

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: occt_helpers — `to_occt_transform` (identity и rotation)

**Files:**
- Test: `tests/geometry/occt_helpers_test.cpp`

- [ ] **Step 1: Добавить тесты для трансформации**

Добавить в `tests/geometry/occt_helpers_test.cpp`:

```cpp
#include <gp_Pnt.hxx>

using coupecad::geometry::to_occt_transform;

TEST(OcctHelpersTest, ToOcctTransform_IdentityQuaternion_PureTranslation) {
    const Vec3 origin{Millimeters{100}, Millimeters{200}, Millimeters{300}};
    const auto trsf = to_occt_transform(origin, Quat::identity());

    // Применяем к точке (0,0,0): должна оказаться в (100, 200, 300).
    gp_Pnt p(0.0, 0.0, 0.0);
    p.Transform(trsf);
    EXPECT_DOUBLE_EQ(p.X(), 100.0);
    EXPECT_DOUBLE_EQ(p.Y(), 200.0);
    EXPECT_DOUBLE_EQ(p.Z(), 300.0);
}

TEST(OcctHelpersTest, ToOcctTransform_RotationAroundZ90Deg_AppliedThenTranslated) {
    // Кватернион поворота на 90° вокруг Z: (w=cos45°, x=0, y=0, z=sin45°).
    constexpr double s = 0.70710678118654752440;  // sin(45°) = cos(45°)
    const Quat q{s, 0.0, 0.0, s};
    const Vec3 origin{Millimeters{10}, Millimeters{20}, Millimeters{0}};
    const auto trsf = to_occt_transform(origin, q);

    // Точка (1,0,0) после поворота → (0,1,0), затем + (10,20,0) = (10,21,0).
    gp_Pnt p(1.0, 0.0, 0.0);
    p.Transform(trsf);
    EXPECT_NEAR(p.X(), 10.0, 1e-9);
    EXPECT_NEAR(p.Y(), 21.0, 1e-9);
    EXPECT_NEAR(p.Z(), 0.0, 1e-9);
}
```

- [ ] **Step 2: Прогнать**

```bash
cmake --build --preset default
ctest --preset default --output-on-failure -R OcctHelpersTest
```

Ожидание: оба теста зелёные. Если `RotationAroundZ90Deg` падает (например, из-за неправильного порядка translation/rotation) — исправить `to_occt_transform` в `occt_helpers.cpp`. Канонический порядок: сначала установить rotation, потом translation part (что и сделано в Task 1 Step 5).

- [ ] **Step 3: Commit**

```bash
git add tests/geometry/occt_helpers_test.cpp
git commit -m "$(cat <<'EOF'
test(geometry): покрыть to_occt_transform identity и Z-поворотом

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: `panel_shape` — `build_panel_solid`

**Files:**
- Create: `src/coupecad/geometry/panel_shape.h`
- Create: `src/coupecad/geometry/panel_shape.cpp`
- Modify: `src/coupecad/geometry/CMakeLists.txt`
- Create: `tests/geometry/panel_shape_test.cpp`
- Modify: `tests/geometry/CMakeLists.txt`

- [ ] **Step 1: Создать заголовок `src/coupecad/geometry/panel_shape.h`**

```cpp
#pragma once

#include "coupecad/core/cabinet.h"
#include "coupecad/core/panel.h"

#include <TopoDS_Solid.hxx>

namespace coupecad::geometry {

// Построить TopoDS_Solid одной панели в мировой СК шкафа.
// Использует core::compute_panel_geometry для получения origin/size/orientation
// (он же делает валидацию role-based параметров).
//
// Бросает core::DomainError, если compute_panel_geometry падает или
// рассчитанный размер имеет нулевое измерение (BRepPrimAPI_MakeBox требует
// положительные размеры).
TopoDS_Solid build_panel_solid(const core::Cabinet& cabinet,
                               const core::Panel& panel);

}  // namespace coupecad::geometry
```

- [ ] **Step 2: Создать тест-файл `tests/geometry/panel_shape_test.cpp` (RED)**

```cpp
#include "coupecad/geometry/panel_shape.h"

#include "coupecad/core/cabinet.h"
#include "coupecad/core/id.h"
#include "coupecad/core/material.h"
#include "coupecad/core/panel.h"
#include "coupecad/core/units.h"

#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <TopAbs_ShapeEnum.hxx>

#include <gtest/gtest.h>

namespace {

using coupecad::core::Cabinet;
using coupecad::core::CabinetId;
using coupecad::core::Dimensions;
using coupecad::core::MaterialId;
using coupecad::core::Millimeters;
using coupecad::core::Panel;
using coupecad::core::PanelId;
using coupecad::core::PanelRole;
using coupecad::core::make_seeded_uuid_generator;
using coupecad::geometry::build_panel_solid;

// Минимальная фабрика стандартного шкафа 800×500×2000, толщина панелей 16.
Cabinet make_cabinet() {
    auto gen = make_seeded_uuid_generator(42);
    Cabinet c;
    c.id = CabinetId{gen->next()};
    c.name = "Test cabinet";
    c.dimensions = Dimensions{Millimeters{800}, Millimeters{500}, Millimeters{2000}};
    c.default_panel_material = MaterialId{gen->next()};
    c.default_panel_thickness = Millimeters{16};
    c.default_back_thickness = Millimeters{4};
    return c;
}

Panel make_role_panel(PanelRole role) {
    auto gen = make_seeded_uuid_generator(7);
    Panel p;
    p.id = PanelId{gen->next()};
    p.role = role;
    return p;
}

}  // namespace

TEST(PanelShapeTest, Bottom_BBoxAndVolume) {
    const Cabinet c = make_cabinet();
    const Panel p = make_role_panel(PanelRole::Bottom);
    const TopoDS_Solid solid = build_panel_solid(c, p);

    EXPECT_EQ(solid.ShapeType(), TopAbs_SOLID);

    Bnd_Box bbox;
    BRepBndLib::Add(solid, bbox);
    double xmin, ymin, zmin, xmax, ymax, zmax;
    bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);

    // Bottom: origin (0,0,0), size = (W, D - t_back, t) = (800, 496, 16).
    EXPECT_NEAR(xmin, 0.0,   1e-6);
    EXPECT_NEAR(ymin, 0.0,   1e-6);
    EXPECT_NEAR(zmin, 0.0,   1e-6);
    EXPECT_NEAR(xmax, 800.0, 1e-6);
    EXPECT_NEAR(ymax, 496.0, 1e-6);
    EXPECT_NEAR(zmax, 16.0,  1e-6);

    GProp_GProps vp;
    BRepGProp::VolumeProperties(solid, vp);
    EXPECT_NEAR(vp.Mass(), 800.0 * 496.0 * 16.0, 1e-3);
}

TEST(PanelShapeTest, Top_OriginIsAtCabinetTop) {
    const Cabinet c = make_cabinet();
    const Panel p = make_role_panel(PanelRole::Top);
    const TopoDS_Solid solid = build_panel_solid(c, p);

    Bnd_Box bbox;
    BRepBndLib::Add(solid, bbox);
    double xmin, ymin, zmin, xmax, ymax, zmax;
    bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);

    // Top: origin = (0, 0, H - t) = (0, 0, 1984), size = (800, 496, 16).
    EXPECT_NEAR(zmin, 1984.0, 1e-6);
    EXPECT_NEAR(zmax, 2000.0, 1e-6);
}

TEST(PanelShapeTest, SideLeft_OccupiesFullHeight) {
    const Cabinet c = make_cabinet();
    const Panel p = make_role_panel(PanelRole::SideLeft);
    const TopoDS_Solid solid = build_panel_solid(c, p);

    Bnd_Box bbox;
    BRepBndLib::Add(solid, bbox);
    double xmin, ymin, zmin, xmax, ymax, zmax;
    bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);

    // SideLeft: origin (0,0,0), size = (t, D - t_back, H) = (16, 496, 2000).
    EXPECT_NEAR(xmax - xmin, 16.0,   1e-6);
    EXPECT_NEAR(ymax - ymin, 496.0,  1e-6);
    EXPECT_NEAR(zmax - zmin, 2000.0, 1e-6);
}
```

- [ ] **Step 3: Зарегистрировать тест в `tests/geometry/CMakeLists.txt`**

```cmake
add_executable(coupecad_geometry_test
    occt_helpers_test.cpp
    panel_shape_test.cpp
)

target_link_libraries(coupecad_geometry_test
    PRIVATE
        coupecad_geometry
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(coupecad_geometry_test)
```

- [ ] **Step 4: Прогнать (должны быть линковые ошибки или missing-symbol на `build_panel_solid`)**

```bash
cmake --build --preset default
```

Ожидание: ошибка компоновки `undefined reference to build_panel_solid`.

- [ ] **Step 5: Реализовать `src/coupecad/geometry/panel_shape.cpp`**

```cpp
#include "coupecad/geometry/panel_shape.h"

#include "coupecad/core/errors.h"
#include "coupecad/core/geometry.h"
#include "coupecad/geometry/occt_helpers.h"

#include <BRepBuilderAPI_Transform.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <TopoDS.hxx>
#include <gp_Pnt.hxx>

namespace coupecad::geometry {

TopoDS_Solid build_panel_solid(const core::Cabinet& cabinet,
                               const core::Panel& panel) {
    const auto pg = core::compute_panel_geometry(cabinet, panel);
    const auto dims = to_box_dims(pg.size);

    if (dims.dx <= 0.0 || dims.dy <= 0.0 || dims.dz <= 0.0) {
        throw core::DomainError{"PANEL_BOX_NON_POSITIVE",
                                "Panel size has non-positive dimension"};
    }

    BRepPrimAPI_MakeBox box(gp_Pnt(0.0, 0.0, 0.0), dims.dx, dims.dy, dims.dz);
    const TopoDS_Solid local_solid = box.Solid();

    const gp_Trsf trsf = to_occt_transform(pg.origin, pg.orientation);
    return TopoDS::Solid(
        BRepBuilderAPI_Transform(local_solid, trsf, /*Copy=*/false).Shape());
}

}  // namespace coupecad::geometry
```

- [ ] **Step 6: Добавить файл в библиотеку `src/coupecad/geometry/CMakeLists.txt`**

```cmake
add_library(coupecad_geometry STATIC
    occt_helpers.cpp
    panel_shape.cpp
)
```

- [ ] **Step 7: Прогнать**

```bash
cmake --build --preset default
ctest --preset default --output-on-failure -R PanelShapeTest
```

Ожидание: все три теста (`Bottom_BBoxAndVolume`, `Top_OriginIsAtCabinetTop`, `SideLeft_OccupiesFullHeight`) зелёные.

- [ ] **Step 8: Commit**

```bash
git add src/coupecad/geometry/panel_shape.h src/coupecad/geometry/panel_shape.cpp \
        src/coupecad/geometry/CMakeLists.txt \
        tests/geometry/panel_shape_test.cpp tests/geometry/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(geometry): build_panel_solid через BRepPrimAPI_MakeBox

Реюзим core::compute_panel_geometry — единый источник правды для
расположения панелей всех 15 ролей. На геометрическом слое только
маппинг Vec3 (mm-int) → MakeBox + transform.

Тесты покрывают Bottom/Top/SideLeft: bbox, объём, ShapeType.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: `panel_shape` — покрыть оставшиеся роли

**Files:**
- Test: `tests/geometry/panel_shape_test.cpp`

Реализация уже работает для всех ролей — `compute_panel_geometry` это берёт на себя. Тесты ниже подтверждают это для оставшихся 12 ролей.

- [ ] **Step 1: Добавить параметризованный тест для ролей без обязательных RoleParams**

Из `src/coupecad/core/panel.h` (enum `PanelRole`) роли без обязательных параметров:
`Top`, `Bottom`, `SideLeft`, `SideRight`, `Back`. Остальные требуют `RoleParams`-вариант (см. ShelfParams, DividerVerticalParams, DividerHorizontalParams, FacadeParams, DrawerBottomParams, DrawerFrontParams, DrawerSideParams, DrawerBackParams, и т.д.) и покрываются отдельно.

Добавить в `tests/geometry/panel_shape_test.cpp`:

```cpp
// Для каждой role-based роли без обязательных параметров: проверяем,
// что Stage 2-обёртка корректно конвертирует PanelGeometry в TopoDS_Solid.
// Точные bbox'ы для каждой роли уже покрыты Stage 1 тестами
// compute_panel_geometry — здесь убеждаемся, что Stage 2 ничего не теряет.
class PanelShapeAllRolesTest : public ::testing::TestWithParam<PanelRole> {};

TEST_P(PanelShapeAllRolesTest, ProducesValidNonEmptySolid) {
    const Cabinet c = make_cabinet();
    const Panel p = make_role_panel(GetParam());

    const TopoDS_Solid solid = build_panel_solid(c, p);

    EXPECT_EQ(solid.ShapeType(), TopAbs_SOLID);

    GProp_GProps vp;
    BRepGProp::VolumeProperties(solid, vp);
    EXPECT_GT(vp.Mass(), 0.0);
}

INSTANTIATE_TEST_SUITE_P(
    NoParamsRoles, PanelShapeAllRolesTest,
    ::testing::Values(
        PanelRole::Top,
        PanelRole::Bottom,
        PanelRole::SideLeft,
        PanelRole::SideRight,
        PanelRole::Back
    ));
```

- [ ] **Step 2: Добавить тест на Shelf-панель с ShelfParams**

```cpp
#include "coupecad/core/panel.h"  // ShelfParams, ShelfFullWidth

TEST(PanelShapeTest, Shelf_AtMiddleHeight_HasCorrectZ) {
    Cabinet c = make_cabinet();
    Panel p = make_role_panel(PanelRole::Shelf);
    // ShelfParams: высота полки от низа + ShelfExtent.
    p.role_params = ShelfParams{Millimeters{1000}, ShelfFullWidth{}};

    const TopoDS_Solid solid = build_panel_solid(c, p);

    Bnd_Box bbox;
    BRepBndLib::Add(solid, bbox);
    double xmin, ymin, zmin, xmax, ymax, zmax;
    bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);

    // Shelf: z от height_from_bottom до height_from_bottom + default_panel_thickness.
    EXPECT_NEAR(zmin, 1000.0, 1e-6);
    EXPECT_NEAR(zmax, 1016.0, 1e-6);
}
```

> **Замечание:** точные имена структур и полей берутся из `src/coupecad/core/panel.h`. На момент написания плана: `ShelfParams { Millimeters height_from_bottom; ShelfExtent extent = ShelfFullWidth{}; }`. `ShelfExtent = std::variant<ShelfFullWidth, ShelfBetweenDividers>`.

- [ ] **Step 3: Прогнать**

```bash
cmake --build --preset default
ctest --preset default --output-on-failure -R PanelShape
```

Ожидание: все параметризованные тесты + Shelf зелёные.

- [ ] **Step 4: Commit**

```bash
git add tests/geometry/panel_shape_test.cpp
git commit -m "$(cat <<'EOF'
test(geometry): покрыть все role-based панели + Shelf-параметры

Параметризованный тест проверяет, что build_panel_solid возвращает
валидный непустой solid для всех ролей без RoleParams. Shelf покрыт
отдельным тестом с проверкой высоты по Z.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: `hardware_shape` — `build_hardware_compound`

**Files:**
- Create: `src/coupecad/geometry/hardware_shape.h`
- Create: `src/coupecad/geometry/hardware_shape.cpp`
- Modify: `src/coupecad/geometry/CMakeLists.txt`
- Create: `tests/geometry/hardware_shape_test.cpp`
- Modify: `tests/geometry/CMakeLists.txt`

- [ ] **Step 1: Создать заголовок `src/coupecad/geometry/hardware_shape.h`**

```cpp
#pragma once

#include "coupecad/core/hardware.h"
#include "coupecad/core/project.h"

#include <TopoDS_Compound.hxx>

namespace coupecad::geometry {

// Построить TopoDS_Compound для одного HardwareItem.
// Compound содержит по одному TopoDS_Solid (bbox-параллелепипед) на каждый
// PanelAttachment в item.attachments. Позиция каждого solid'а — в мировой
// СК шкафа: world_trsf = panel_trsf ∘ attachment_trsf.
//
// Бросает core::DomainError, если HardwareSpec для item.ref не найден
// в project.hardware_catalog(), или если какой-то attachment.panel_id
// отсутствует в project.cabinet().panels.
TopoDS_Compound build_hardware_compound(const core::Project& project,
                                        const core::HardwareItem& item);

}  // namespace coupecad::geometry
```

- [ ] **Step 2: Создать тест `tests/geometry/hardware_shape_test.cpp` (RED)**

```cpp
#include "coupecad/geometry/hardware_shape.h"

#include "coupecad/core/cabinet.h"
#include "coupecad/core/errors.h"
#include "coupecad/core/hardware.h"
#include "coupecad/core/material.h"
#include "coupecad/core/panel.h"
#include "coupecad/core/project.h"
#include "coupecad/core/units.h"

#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>

#include <gtest/gtest.h>

namespace {

using namespace coupecad::core;
using coupecad::geometry::build_hardware_compound;

// Хелпер: проект с одним шкафом, одной SideLeft-панелью, одной hardware-spec
// (петля 50×30×20 мм) и одним HardwareItem с одним attachment к этой панели
// в локальной точке (10, 20, 30).
struct Fixture {
    Project project = Project::create_empty("test", make_seeded_uuid_generator(11));
    PanelId side_left_id;
    HardwareRef ref;
    HardwareItemId item_id;

    Fixture() {
        auto& cab = project.mutable_cabinet();
        cab.dimensions = Dimensions{Millimeters{800}, Millimeters{500}, Millimeters{2000}};
        cab.default_panel_material = project.materials().begin()->first;

        Panel side;
        side.id = PanelId{project.uuid_gen().next()};
        side.role = PanelRole::SideLeft;
        side_left_id = side.id;
        cab.panels.emplace(side.id, std::move(side));

        ref = HardwareRef{"hinge.blum.110"};
        HardwareSpec spec;
        spec.ref = ref;
        spec.kind = HardwareKind::Hinge;
        spec.name = "Blum 110°";
        spec.bbox = Vec3{Millimeters{50}, Millimeters{30}, Millimeters{20}};
        project.mutable_hardware_catalog().emplace(ref, std::move(spec));

        HardwareItem item;
        item.id = HardwareItemId{project.uuid_gen().next()};
        item.ref = ref;
        item.attachments.push_back(
            PanelAttachment{side_left_id,
                            Vec3{Millimeters{10}, Millimeters{20}, Millimeters{30}},
                            Quat::identity()});
        item_id = item.id;
        cab.hardware.emplace(item.id, std::move(item));
    }
};

std::size_t count_solids(const TopoDS_Compound& c) {
    std::size_t n = 0;
    for (TopExp_Explorer ex(c, TopAbs_SOLID); ex.More(); ex.Next()) ++n;
    return n;
}

}  // namespace

TEST(HardwareShapeTest, SingleAttachment_OneSolidInCompound) {
    Fixture fx;
    const auto& item = fx.project.cabinet().hardware.at(fx.item_id);
    const TopoDS_Compound compound = build_hardware_compound(fx.project, item);

    EXPECT_EQ(compound.ShapeType(), TopAbs_COMPOUND);
    EXPECT_EQ(count_solids(compound), 1u);
}

TEST(HardwareShapeTest, SingleAttachment_BBoxAtWorldPosition) {
    Fixture fx;
    const auto& item = fx.project.cabinet().hardware.at(fx.item_id);
    const TopoDS_Compound compound = build_hardware_compound(fx.project, item);

    Bnd_Box bbox;
    BRepBndLib::Add(compound, bbox);
    double xmin, ymin, zmin, xmax, ymax, zmax;
    bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);

    // SideLeft panel origin = (0,0,0), orientation identity.
    // Attachment local (10, 20, 30), bbox (50, 30, 20).
    // World box: from (10, 20, 30) to (60, 50, 50).
    EXPECT_NEAR(xmin, 10.0, 1e-6);
    EXPECT_NEAR(ymin, 20.0, 1e-6);
    EXPECT_NEAR(zmin, 30.0, 1e-6);
    EXPECT_NEAR(xmax, 60.0, 1e-6);
    EXPECT_NEAR(ymax, 50.0, 1e-6);
    EXPECT_NEAR(zmax, 50.0, 1e-6);
}

TEST(HardwareShapeTest, MultipleAttachments_OneSolidPerAttachment) {
    Fixture fx;
    auto& item = fx.project.mutable_cabinet().hardware.at(fx.item_id);
    item.attachments.push_back(
        PanelAttachment{fx.side_left_id,
                        Vec3{Millimeters{200}, Millimeters{20}, Millimeters{30}},
                        Quat::identity()});

    const TopoDS_Compound compound = build_hardware_compound(fx.project, item);
    EXPECT_EQ(count_solids(compound), 2u);
}

TEST(HardwareShapeTest, UnknownSpecRef_ThrowsDomainError) {
    Fixture fx;
    auto& item = fx.project.mutable_cabinet().hardware.at(fx.item_id);
    item.ref = HardwareRef{"does.not.exist"};

    EXPECT_THROW(build_hardware_compound(fx.project, item), DomainError);
}

TEST(HardwareShapeTest, UnknownAttachmentPanel_ThrowsDomainError) {
    Fixture fx;
    auto& item = fx.project.mutable_cabinet().hardware.at(fx.item_id);
    auto orphan = make_seeded_uuid_generator(99);
    item.attachments[0].panel_id = PanelId{orphan->next()};

    EXPECT_THROW(build_hardware_compound(fx.project, item), DomainError);
}
```

> **Замечание:** в этом тесте используется `Project::mutable_cabinet()` и `mutable_hardware_catalog()` — публичные API из Stage 1a, которые возвращают неконстантную ссылку (см. `project.h:52-58`). Это «лазейка» Stage 1a, которую закроют команды Stage 1b — но для unit-тестов геометрического слоя удобно использовать прямой доступ.

- [ ] **Step 3: Зарегистрировать тест в `tests/geometry/CMakeLists.txt`**

```cmake
add_executable(coupecad_geometry_test
    occt_helpers_test.cpp
    panel_shape_test.cpp
    hardware_shape_test.cpp
)
```

- [ ] **Step 4: Прогнать (должна быть линковая ошибка)**

```bash
cmake --build --preset default
```

Ожидание: `undefined reference to build_hardware_compound`.

- [ ] **Step 5: Реализовать `src/coupecad/geometry/hardware_shape.cpp`**

```cpp
#include "coupecad/geometry/hardware_shape.h"

#include "coupecad/core/cabinet.h"
#include "coupecad/core/errors.h"
#include "coupecad/core/geometry.h"
#include "coupecad/geometry/occt_helpers.h"

#include <BRepBuilderAPI_Transform.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRep_Builder.hxx>
#include <gp_Pnt.hxx>

namespace coupecad::geometry {

TopoDS_Compound build_hardware_compound(const core::Project& project,
                                        const core::HardwareItem& item) {
    const auto& catalog = project.hardware_catalog();
    const auto spec_it = catalog.find(item.ref);
    if (spec_it == catalog.end()) {
        throw core::DomainError{"HARDWARE_SPEC_NOT_FOUND",
                                "Hardware spec not found: " + item.ref.value()};
    }
    const core::Vec3& bbox_size = spec_it->second.bbox;
    const auto box = to_box_dims(bbox_size);

    TopoDS_Compound compound;
    BRep_Builder bb;
    bb.MakeCompound(compound);

    const auto& panels = project.cabinet().panels;
    for (const auto& att : item.attachments) {
        const auto panel_it = panels.find(att.panel_id);
        if (panel_it == panels.end()) {
            throw core::DomainError{"HARDWARE_ATTACHMENT_PANEL_NOT_FOUND",
                                    "Attachment refers to unknown panel"};
        }
        const auto pg = core::compute_panel_geometry(project.cabinet(), panel_it->second);

        const gp_Trsf panel_trsf = to_occt_transform(pg.origin, pg.orientation);
        const gp_Trsf attach_trsf = to_occt_transform(att.local_position, att.orientation);
        const gp_Trsf world_trsf = panel_trsf.Multiplied(attach_trsf);

        BRepPrimAPI_MakeBox mk(gp_Pnt(0.0, 0.0, 0.0), box.dx, box.dy, box.dz);
        const TopoDS_Shape positioned =
            BRepBuilderAPI_Transform(mk.Solid(), world_trsf, /*Copy=*/false).Shape();
        bb.Add(compound, positioned);
    }
    return compound;
}

}  // namespace coupecad::geometry
```

- [ ] **Step 6: Добавить файл в `src/coupecad/geometry/CMakeLists.txt`**

```cmake
add_library(coupecad_geometry STATIC
    occt_helpers.cpp
    panel_shape.cpp
    hardware_shape.cpp
)
```

- [ ] **Step 7: Прогнать**

```bash
cmake --build --preset default
ctest --preset default --output-on-failure -R HardwareShapeTest
```

Ожидание: все 5 тестов зелёные.

- [ ] **Step 8: Commit**

```bash
git add src/coupecad/geometry/hardware_shape.h src/coupecad/geometry/hardware_shape.cpp \
        src/coupecad/geometry/CMakeLists.txt \
        tests/geometry/hardware_shape_test.cpp tests/geometry/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(geometry): build_hardware_compound с composition трансформов

HardwareItem → TopoDS_Compound, по одному solid'у на каждый PanelAttachment.
World transform = panel_trsf ∘ attachment_trsf — фурнитура едет за панелью
автоматически.

Бросаем DomainError на неизвестный spec/ref или panel_id.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 7: `cabinet_shape` — `build_cabinet_compound`

**Files:**
- Create: `src/coupecad/geometry/cabinet_shape.h`
- Create: `src/coupecad/geometry/cabinet_shape.cpp`
- Modify: `src/coupecad/geometry/CMakeLists.txt`
- Create: `tests/geometry/cabinet_shape_test.cpp`
- Modify: `tests/geometry/CMakeLists.txt`

> **Замечание:** в этой задаче `cabinet_shape` принимает не `GeometryBuilder` (он будет в Task 8), а callback-функции для получения panel/hardware shape — чтобы тестировать assembler изолированно. В Task 9 `GeometryBuilder::cabinet_compound()` подставит свои методы.

- [ ] **Step 1: Создать заголовок `src/coupecad/geometry/cabinet_shape.h`**

```cpp
#pragma once

#include "coupecad/core/id.h"
#include "coupecad/core/project.h"

#include <TopoDS_Compound.hxx>
#include <TopoDS_Solid.hxx>

#include <functional>

namespace coupecad::geometry {

// Собрать TopoDS_Compound шкафа: для каждой панели получаем TopoDS_Solid
// через get_panel_solid, для каждого hardware-item — TopoDS_Compound через
// get_hardware_compound. Колбэки нужны, чтобы assembler тестировался без
// зависимости от GeometryBuilder.
//
// Колбэки могут бросать DomainError — пробрасывается дальше.
TopoDS_Compound build_cabinet_compound(
    const core::Project& project,
    const std::function<const TopoDS_Solid&(const core::PanelId&)>& get_panel_solid,
    const std::function<const TopoDS_Compound&(const core::HardwareItemId&)>&
        get_hardware_compound);

}  // namespace coupecad::geometry
```

- [ ] **Step 2: Создать тест `tests/geometry/cabinet_shape_test.cpp` (RED)**

```cpp
#include "coupecad/geometry/cabinet_shape.h"
#include "coupecad/geometry/hardware_shape.h"
#include "coupecad/geometry/panel_shape.h"

#include "coupecad/core/cabinet.h"
#include "coupecad/core/material.h"
#include "coupecad/core/panel.h"
#include "coupecad/core/project.h"
#include "coupecad/core/units.h"

#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>

#include <gtest/gtest.h>

#include <unordered_map>

namespace {

using namespace coupecad::core;
using coupecad::geometry::build_cabinet_compound;
using coupecad::geometry::build_hardware_compound;
using coupecad::geometry::build_panel_solid;

Project make_project_with_two_panels() {
    auto p = Project::create_empty("test", make_seeded_uuid_generator(31));
    auto& cab = p.mutable_cabinet();
    cab.dimensions = Dimensions{Millimeters{800}, Millimeters{500}, Millimeters{2000}};
    cab.default_panel_material = p.materials().begin()->first;

    Panel bottom;
    bottom.id = PanelId{p.uuid_gen().next()};
    bottom.role = PanelRole::Bottom;
    cab.panels.emplace(bottom.id, std::move(bottom));

    Panel top;
    top.id = PanelId{p.uuid_gen().next()};
    top.role = PanelRole::Top;
    cab.panels.emplace(top.id, std::move(top));
    return p;
}

std::size_t count_subshapes(const TopoDS_Compound& c, TopAbs_ShapeEnum kind) {
    std::size_t n = 0;
    for (TopExp_Explorer ex(c, kind); ex.More(); ex.Next()) ++n;
    return n;
}

}  // namespace

TEST(CabinetShapeTest, EmptyCabinet_EmptyCompound) {
    auto p = Project::create_empty("test", make_seeded_uuid_generator(1));
    auto& cab = p.mutable_cabinet();
    cab.dimensions = Dimensions{Millimeters{800}, Millimeters{500}, Millimeters{2000}};
    cab.default_panel_material = p.materials().begin()->first;

    std::unordered_map<PanelId, TopoDS_Solid> panels;
    std::unordered_map<HardwareItemId, TopoDS_Compound> hardware;
    auto get_p = [&](const PanelId& id) -> const TopoDS_Solid& { return panels.at(id); };
    auto get_h = [&](const HardwareItemId& id) -> const TopoDS_Compound& {
        return hardware.at(id);
    };

    const TopoDS_Compound result = build_cabinet_compound(p, get_p, get_h);
    EXPECT_EQ(result.ShapeType(), TopAbs_COMPOUND);
    EXPECT_EQ(count_subshapes(result, TopAbs_SOLID), 0u);
}

TEST(CabinetShapeTest, TwoPanels_TwoSolidsInCompound) {
    auto p = make_project_with_two_panels();

    std::unordered_map<PanelId, TopoDS_Solid> panels;
    for (const auto& [id, panel] : p.cabinet().panels) {
        panels.emplace(id, build_panel_solid(p.cabinet(), panel));
    }
    std::unordered_map<HardwareItemId, TopoDS_Compound> hardware;

    auto get_p = [&](const PanelId& id) -> const TopoDS_Solid& { return panels.at(id); };
    auto get_h = [&](const HardwareItemId& id) -> const TopoDS_Compound& {
        return hardware.at(id);
    };

    const TopoDS_Compound result = build_cabinet_compound(p, get_p, get_h);
    EXPECT_EQ(count_subshapes(result, TopAbs_SOLID), 2u);
}

TEST(CabinetShapeTest, BoundingBoxIsUnionOfPanelBoxes) {
    auto p = make_project_with_two_panels();

    std::unordered_map<PanelId, TopoDS_Solid> panels;
    for (const auto& [id, panel] : p.cabinet().panels) {
        panels.emplace(id, build_panel_solid(p.cabinet(), panel));
    }
    std::unordered_map<HardwareItemId, TopoDS_Compound> hardware;

    auto get_p = [&](const PanelId& id) -> const TopoDS_Solid& { return panels.at(id); };
    auto get_h = [&](const HardwareItemId& id) -> const TopoDS_Compound& {
        return hardware.at(id);
    };

    const TopoDS_Compound result = build_cabinet_compound(p, get_p, get_h);
    Bnd_Box bbox;
    BRepBndLib::Add(result, bbox);
    double xmin, ymin, zmin, xmax, ymax, zmax;
    bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);

    // Bottom: z in [0, 16]; Top: z in [1984, 2000]. Union: z in [0, 2000].
    EXPECT_NEAR(zmin, 0.0,    1e-6);
    EXPECT_NEAR(zmax, 2000.0, 1e-6);
    EXPECT_NEAR(xmin, 0.0,    1e-6);
    EXPECT_NEAR(xmax, 800.0,  1e-6);
}
```

- [ ] **Step 3: Зарегистрировать в `tests/geometry/CMakeLists.txt`**

```cmake
add_executable(coupecad_geometry_test
    occt_helpers_test.cpp
    panel_shape_test.cpp
    hardware_shape_test.cpp
    cabinet_shape_test.cpp
)
```

- [ ] **Step 4: Прогнать (RED)**

```bash
cmake --build --preset default
```

Ожидание: `undefined reference to build_cabinet_compound`.

- [ ] **Step 5: Реализовать `src/coupecad/geometry/cabinet_shape.cpp`**

```cpp
#include "coupecad/geometry/cabinet_shape.h"

#include <BRep_Builder.hxx>

namespace coupecad::geometry {

TopoDS_Compound build_cabinet_compound(
    const core::Project& project,
    const std::function<const TopoDS_Solid&(const core::PanelId&)>& get_panel_solid,
    const std::function<const TopoDS_Compound&(const core::HardwareItemId&)>&
        get_hardware_compound) {

    TopoDS_Compound compound;
    BRep_Builder bb;
    bb.MakeCompound(compound);

    for (const auto& [panel_id, _] : project.cabinet().panels) {
        bb.Add(compound, get_panel_solid(panel_id));
    }
    for (const auto& [hw_id, _] : project.cabinet().hardware) {
        bb.Add(compound, get_hardware_compound(hw_id));
    }
    return compound;
}

}  // namespace coupecad::geometry
```

- [ ] **Step 6: Добавить в библиотеку**

В `src/coupecad/geometry/CMakeLists.txt`:

```cmake
add_library(coupecad_geometry STATIC
    occt_helpers.cpp
    panel_shape.cpp
    hardware_shape.cpp
    cabinet_shape.cpp
)
```

- [ ] **Step 7: Прогнать**

```bash
cmake --build --preset default
ctest --preset default --output-on-failure -R CabinetShapeTest
```

Ожидание: все 3 теста зелёные.

- [ ] **Step 8: Commit**

```bash
git add src/coupecad/geometry/cabinet_shape.h src/coupecad/geometry/cabinet_shape.cpp \
        src/coupecad/geometry/CMakeLists.txt \
        tests/geometry/cabinet_shape_test.cpp tests/geometry/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(geometry): build_cabinet_compound через колбэки

Assembler принимает функции-колбэки для получения panel/hardware shape,
чтобы тестироваться без зависимости от GeometryBuilder. В Task 9
Builder подставит свои методы.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 8: `GeometryBuilder` — конструктор и `panel_solid` (lazy + cache)

**Files:**
- Create: `src/coupecad/geometry/geometry_builder.h`
- Create: `src/coupecad/geometry/geometry_builder.cpp`
- Modify: `src/coupecad/geometry/CMakeLists.txt`
- Create: `tests/geometry/geometry_builder_test.cpp`
- Modify: `tests/geometry/CMakeLists.txt`

- [ ] **Step 1: Создать заголовок `src/coupecad/geometry/geometry_builder.h`**

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

// Stateful builder с per-panel и per-hardware кешем. Pull-модель:
// внешний код после команды/undo вызывает apply_changes(cs).
//
// Не thread-safe. Все методы вызываются из одного потока (UI).
//
// Время жизни возвращаемых ссылок — до следующего apply_changes,
// rebuild_all или вызова панель/hardware-метода, который инвалидирует кеш.
class GeometryBuilder {
public:
    explicit GeometryBuilder(const core::Project& project);

    // Полный сброс кеша. Используется после deserialize() (Stage 1c).
    void rebuild_all();

    // Применить дельту: инвалидировать соответствующие entries.
    // Алгоритм — см. spec §5.2.
    void apply_changes(const core::ChangeSet& cs);

    // Геометрия одной панели; lazy — строится при первом запросе.
    const TopoDS_Solid& panel_solid(const core::PanelId& id);

    // Геометрия одного hardware-item; compound с одним solid на каждый attachment.
    const TopoDS_Compound& hardware_compound(const core::HardwareItemId& id);

    // Compound всего шкафа. Lazy — пересобирается, если что-то менялось.
    const TopoDS_Compound& cabinet_compound();

    // Диагностика / тесты.
    bool has_cached_panel(const core::PanelId& id) const noexcept;
    bool has_cached_hardware(const core::HardwareItemId& id) const noexcept;
    std::size_t panel_cache_size() const noexcept;
    std::size_t hardware_cache_size() const noexcept;

private:
    const core::Project& project_;
    std::unordered_map<core::PanelId, TopoDS_Solid> panel_cache_;
    std::unordered_map<core::HardwareItemId, TopoDS_Compound> hardware_cache_;
    TopoDS_Compound cabinet_compound_;
    bool compound_dirty_ = true;
};

}  // namespace coupecad::geometry
```

- [ ] **Step 2: Создать тест `tests/geometry/geometry_builder_test.cpp` (RED)**

```cpp
#include "coupecad/geometry/geometry_builder.h"

#include "coupecad/core/cabinet.h"
#include "coupecad/core/commands/change_set.h"
#include "coupecad/core/material.h"
#include "coupecad/core/panel.h"
#include "coupecad/core/project.h"
#include "coupecad/core/units.h"

#include <gtest/gtest.h>

namespace {

using namespace coupecad::core;
using coupecad::geometry::GeometryBuilder;

Project make_project_with_one_panel() {
    auto p = Project::create_empty("test", make_seeded_uuid_generator(101));
    auto& cab = p.mutable_cabinet();
    cab.dimensions = Dimensions{Millimeters{800}, Millimeters{500}, Millimeters{2000}};
    cab.default_panel_material = p.materials().begin()->first;

    Panel bottom;
    bottom.id = PanelId{p.uuid_gen().next()};
    bottom.role = PanelRole::Bottom;
    cab.panels.emplace(bottom.id, std::move(bottom));
    return p;
}

}  // namespace

TEST(GeometryBuilderTest, FreshBuilder_HasEmptyCaches) {
    auto p = make_project_with_one_panel();
    GeometryBuilder b(p);
    EXPECT_EQ(b.panel_cache_size(), 0u);
    EXPECT_EQ(b.hardware_cache_size(), 0u);
}

TEST(GeometryBuilderTest, PanelSolid_LazyBuildAndCache) {
    auto p = make_project_with_one_panel();
    GeometryBuilder b(p);

    const auto panel_id = p.cabinet().panels.begin()->first;
    EXPECT_FALSE(b.has_cached_panel(panel_id));

    const TopoDS_Solid& s1 = b.panel_solid(panel_id);
    EXPECT_TRUE(b.has_cached_panel(panel_id));
    EXPECT_EQ(b.panel_cache_size(), 1u);

    // Повторный вызов — берёт из кеша, тот же handle.
    const TopoDS_Solid& s2 = b.panel_solid(panel_id);
    EXPECT_TRUE(s1.IsSame(s2));
}
```

- [ ] **Step 3: Зарегистрировать в `tests/geometry/CMakeLists.txt`**

```cmake
add_executable(coupecad_geometry_test
    occt_helpers_test.cpp
    panel_shape_test.cpp
    hardware_shape_test.cpp
    cabinet_shape_test.cpp
    geometry_builder_test.cpp
)
```

- [ ] **Step 4: Прогнать (RED)**

```bash
cmake --build --preset default
```

Ожидание: ошибки компоновки/компиляции — нет реализации `GeometryBuilder`.

- [ ] **Step 5: Реализовать минимум — конструктор + `panel_solid` + `has_cached_panel` + размеры кешей**

`src/coupecad/geometry/geometry_builder.cpp`:

```cpp
#include "coupecad/geometry/geometry_builder.h"

#include "coupecad/geometry/panel_shape.h"

namespace coupecad::geometry {

GeometryBuilder::GeometryBuilder(const core::Project& project)
    : project_(project) {}

const TopoDS_Solid& GeometryBuilder::panel_solid(const core::PanelId& id) {
    auto it = panel_cache_.find(id);
    if (it != panel_cache_.end()) {
        return it->second;
    }
    const auto& panel = project_.cabinet().panels.at(id);
    auto [inserted_it, _] = panel_cache_.emplace(
        id, build_panel_solid(project_.cabinet(), panel));
    return inserted_it->second;
}

bool GeometryBuilder::has_cached_panel(const core::PanelId& id) const noexcept {
    return panel_cache_.count(id) > 0;
}

bool GeometryBuilder::has_cached_hardware(const core::HardwareItemId& id) const noexcept {
    return hardware_cache_.count(id) > 0;
}

std::size_t GeometryBuilder::panel_cache_size() const noexcept {
    return panel_cache_.size();
}

std::size_t GeometryBuilder::hardware_cache_size() const noexcept {
    return hardware_cache_.size();
}

// Заглушки — реализуются в следующих задачах.
const TopoDS_Compound& GeometryBuilder::hardware_compound(const core::HardwareItemId&) {
    static TopoDS_Compound empty;
    return empty;
}
const TopoDS_Compound& GeometryBuilder::cabinet_compound() {
    return cabinet_compound_;
}
void GeometryBuilder::rebuild_all() {
    panel_cache_.clear();
    hardware_cache_.clear();
    compound_dirty_ = true;
}
void GeometryBuilder::apply_changes(const core::ChangeSet&) {
    // Реализация в Task 11.
}

}  // namespace coupecad::geometry
```

- [ ] **Step 6: Добавить в `src/coupecad/geometry/CMakeLists.txt`**

```cmake
add_library(coupecad_geometry STATIC
    occt_helpers.cpp
    panel_shape.cpp
    hardware_shape.cpp
    cabinet_shape.cpp
    geometry_builder.cpp
)
```

- [ ] **Step 7: Прогнать**

```bash
cmake --build --preset default
ctest --preset default --output-on-failure -R GeometryBuilderTest
```

Ожидание: оба теста (`FreshBuilder_HasEmptyCaches`, `PanelSolid_LazyBuildAndCache`) зелёные.

- [ ] **Step 8: Commit**

```bash
git add src/coupecad/geometry/geometry_builder.h src/coupecad/geometry/geometry_builder.cpp \
        src/coupecad/geometry/CMakeLists.txt \
        tests/geometry/geometry_builder_test.cpp tests/geometry/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(geometry): GeometryBuilder skeleton + panel_solid lazy/cache

Конструктор владеет ссылкой на Project. panel_solid строит TopoDS_Solid
по запросу, кеширует в unordered_map<PanelId, TopoDS_Solid>. Повторный
вызов возвращает тот же handle.

hardware_compound, cabinet_compound, apply_changes — заглушки,
реализация в следующих задачах.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 9: `GeometryBuilder::hardware_compound` — lazy + cache

**Files:**
- Modify: `src/coupecad/geometry/geometry_builder.cpp`
- Modify: `tests/geometry/geometry_builder_test.cpp`

- [ ] **Step 1: Дописать тест в `tests/geometry/geometry_builder_test.cpp`**

```cpp
#include "coupecad/core/hardware.h"

TEST(GeometryBuilderTest, HardwareCompound_LazyBuildAndCache) {
    auto p = Project::create_empty("test", make_seeded_uuid_generator(303));
    auto& cab = p.mutable_cabinet();
    cab.dimensions = Dimensions{Millimeters{800}, Millimeters{500}, Millimeters{2000}};
    cab.default_panel_material = p.materials().begin()->first;

    Panel side;
    side.id = PanelId{p.uuid_gen().next()};
    side.role = PanelRole::SideLeft;
    const auto side_id = side.id;
    cab.panels.emplace(side.id, std::move(side));

    HardwareRef ref{"hinge.test"};
    HardwareSpec spec;
    spec.ref = ref;
    spec.kind = HardwareKind::Hinge;
    spec.name = "Test hinge";
    spec.bbox = Vec3{Millimeters{50}, Millimeters{30}, Millimeters{20}};
    p.mutable_hardware_catalog().emplace(ref, std::move(spec));

    HardwareItem item;
    item.id = HardwareItemId{p.uuid_gen().next()};
    item.ref = ref;
    item.attachments.push_back(
        PanelAttachment{side_id, Vec3{Millimeters{10}, Millimeters{20}, Millimeters{30}},
                        Quat::identity()});
    const auto item_id = item.id;
    cab.hardware.emplace(item.id, std::move(item));

    GeometryBuilder b(p);
    EXPECT_FALSE(b.has_cached_hardware(item_id));

    const TopoDS_Compound& c1 = b.hardware_compound(item_id);
    EXPECT_TRUE(b.has_cached_hardware(item_id));
    EXPECT_EQ(b.hardware_cache_size(), 1u);

    const TopoDS_Compound& c2 = b.hardware_compound(item_id);
    EXPECT_TRUE(c1.IsSame(c2));
}
```

- [ ] **Step 2: Прогнать (тест должен упасть — заглушка возвращает пустой compound)**

```bash
cmake --build --preset default
ctest --preset default --output-on-failure -R HardwareCompound_LazyBuildAndCache
```

Ожидание: FAIL — `has_cached_hardware` остаётся false, потому что заглушка ничего не кеширует.

- [ ] **Step 3: Реализовать `hardware_compound` в `geometry_builder.cpp`**

Заменить заглушку на:

```cpp
#include "coupecad/geometry/hardware_shape.h"

const TopoDS_Compound& GeometryBuilder::hardware_compound(const core::HardwareItemId& id) {
    auto it = hardware_cache_.find(id);
    if (it != hardware_cache_.end()) {
        return it->second;
    }
    const auto& item = project_.cabinet().hardware.at(id);
    auto [inserted_it, _] = hardware_cache_.emplace(
        id, build_hardware_compound(project_, item));
    return inserted_it->second;
}
```

- [ ] **Step 4: Прогнать**

```bash
cmake --build --preset default
ctest --preset default --output-on-failure -R GeometryBuilderTest
```

Ожидание: все тесты зелёные.

- [ ] **Step 5: Commit**

```bash
git add src/coupecad/geometry/geometry_builder.cpp tests/geometry/geometry_builder_test.cpp
git commit -m "$(cat <<'EOF'
feat(geometry): GeometryBuilder::hardware_compound lazy/cache

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 10: `GeometryBuilder::cabinet_compound` — lazy + dirty-flag

**Files:**
- Modify: `src/coupecad/geometry/geometry_builder.cpp`
- Modify: `tests/geometry/geometry_builder_test.cpp`

- [ ] **Step 1: Дописать тест в `tests/geometry/geometry_builder_test.cpp`**

```cpp
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>

namespace {
std::size_t count_solids(const TopoDS_Shape& s) {
    std::size_t n = 0;
    for (TopExp_Explorer ex(s, TopAbs_SOLID); ex.More(); ex.Next()) ++n;
    return n;
}
}  // namespace

TEST(GeometryBuilderTest, CabinetCompound_ContainsAllPanels) {
    auto p = make_project_with_one_panel();
    GeometryBuilder b(p);

    const TopoDS_Compound& c = b.cabinet_compound();
    EXPECT_EQ(c.ShapeType(), TopAbs_COMPOUND);
    EXPECT_EQ(count_solids(c), 1u);
}

TEST(GeometryBuilderTest, CabinetCompound_RepeatCallReturnsSameHandleIfNotDirty) {
    auto p = make_project_with_one_panel();
    GeometryBuilder b(p);

    const TopoDS_Compound& c1 = b.cabinet_compound();
    const TopoDS_Compound& c2 = b.cabinet_compound();
    EXPECT_TRUE(c1.IsSame(c2));
}
```

- [ ] **Step 2: Прогнать (RED — `count_solids` вернёт 0, потому что заглушка возвращает пустой compound)**

```bash
cmake --build --preset default
ctest --preset default --output-on-failure -R CabinetCompound
```

Ожидание: FAIL.

- [ ] **Step 3: Реализовать `cabinet_compound()` в `geometry_builder.cpp`**

Заменить заглушку:

```cpp
#include "coupecad/geometry/cabinet_shape.h"

const TopoDS_Compound& GeometryBuilder::cabinet_compound() {
    if (!compound_dirty_) {
        return cabinet_compound_;
    }
    cabinet_compound_ = build_cabinet_compound(
        project_,
        [this](const core::PanelId& id) -> const TopoDS_Solid& {
            return panel_solid(id);
        },
        [this](const core::HardwareItemId& id) -> const TopoDS_Compound& {
            return hardware_compound(id);
        });
    compound_dirty_ = false;
    return cabinet_compound_;
}
```

- [ ] **Step 4: Прогнать**

```bash
cmake --build --preset default
ctest --preset default --output-on-failure -R CabinetCompound
```

Ожидание: оба теста зелёные.

- [ ] **Step 5: Commit**

```bash
git add src/coupecad/geometry/geometry_builder.cpp tests/geometry/geometry_builder_test.cpp
git commit -m "$(cat <<'EOF'
feat(geometry): GeometryBuilder::cabinet_compound lazy + dirty-flag

cabinet_compound пересобирается только если compound_dirty_ == true.
Использует panel_solid/hardware_compound как колбэки в build_cabinet_compound,
что автоматически прогревает оба внутренних кеша.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 11: `GeometryBuilder::apply_changes` + `rebuild_all` — инвалидация

**Files:**
- Modify: `src/coupecad/geometry/geometry_builder.cpp`
- Modify: `tests/geometry/geometry_builder_test.cpp`

- [ ] **Step 1: Дописать тесты для всех веток инвалидации**

```cpp
#include "coupecad/core/commands/change_set.h"

TEST(GeometryBuilderTest, ApplyChanges_UpdatedPanelInvalidatesOnlyThatPanel) {
    auto p = make_project_with_one_panel();
    GeometryBuilder b(p);

    const auto panel_id = p.cabinet().panels.begin()->first;
    b.panel_solid(panel_id);
    EXPECT_EQ(b.panel_cache_size(), 1u);

    ChangeSet cs;
    cs.updated_panels.push_back(panel_id);
    b.apply_changes(cs);

    EXPECT_FALSE(b.has_cached_panel(panel_id));
    EXPECT_EQ(b.panel_cache_size(), 0u);
}

TEST(GeometryBuilderTest, ApplyChanges_RemovedPanelEvictsFromCache) {
    auto p = make_project_with_one_panel();
    GeometryBuilder b(p);

    const auto panel_id = p.cabinet().panels.begin()->first;
    b.panel_solid(panel_id);

    ChangeSet cs;
    cs.removed_panels.push_back(panel_id);
    b.apply_changes(cs);

    EXPECT_FALSE(b.has_cached_panel(panel_id));
}

TEST(GeometryBuilderTest, ApplyChanges_CabinetChangedClearsAllPanelCache) {
    auto p = make_project_with_one_panel();
    GeometryBuilder b(p);

    const auto panel_id = p.cabinet().panels.begin()->first;
    b.panel_solid(panel_id);
    EXPECT_EQ(b.panel_cache_size(), 1u);

    ChangeSet cs;
    cs.cabinet_changed = true;
    b.apply_changes(cs);

    EXPECT_EQ(b.panel_cache_size(), 0u);
}

TEST(GeometryBuilderTest, ApplyChanges_UpdatedPanelAlsoClearsHardwareCache) {
    // Конфигурация: панель + hardware-item с attachment к ней.
    auto p = Project::create_empty("test", make_seeded_uuid_generator(404));
    auto& cab = p.mutable_cabinet();
    cab.dimensions = Dimensions{Millimeters{800}, Millimeters{500}, Millimeters{2000}};
    cab.default_panel_material = p.materials().begin()->first;

    Panel side;
    side.id = PanelId{p.uuid_gen().next()};
    side.role = PanelRole::SideLeft;
    const auto side_id = side.id;
    cab.panels.emplace(side.id, std::move(side));

    HardwareRef ref{"hinge.test"};
    HardwareSpec spec;
    spec.ref = ref;
    spec.kind = HardwareKind::Hinge;
    spec.name = "Test";
    spec.bbox = Vec3{Millimeters{50}, Millimeters{30}, Millimeters{20}};
    p.mutable_hardware_catalog().emplace(ref, std::move(spec));

    HardwareItem item;
    item.id = HardwareItemId{p.uuid_gen().next()};
    item.ref = ref;
    item.attachments.push_back(
        PanelAttachment{side_id, Vec3{}, Quat::identity()});
    const auto item_id = item.id;
    cab.hardware.emplace(item.id, std::move(item));

    GeometryBuilder b(p);
    b.hardware_compound(item_id);
    EXPECT_EQ(b.hardware_cache_size(), 1u);

    ChangeSet cs;
    cs.updated_panels.push_back(side_id);
    b.apply_changes(cs);

    EXPECT_EQ(b.hardware_cache_size(), 0u)
        << "Updating a panel must invalidate hardware attached to it";
}

TEST(GeometryBuilderTest, ApplyChanges_EmptyChangeSetIsNoop) {
    auto p = make_project_with_one_panel();
    GeometryBuilder b(p);

    const auto panel_id = p.cabinet().panels.begin()->first;
    b.panel_solid(panel_id);
    b.cabinet_compound();  // прогрев compound, теперь !dirty
    EXPECT_EQ(b.panel_cache_size(), 1u);

    ChangeSet cs;
    b.apply_changes(cs);

    EXPECT_EQ(b.panel_cache_size(), 1u);
    // Compound не пересобирается — повторный вызов вернёт тот же handle.
    const TopoDS_Compound& c1 = b.cabinet_compound();
    const TopoDS_Compound& c2 = b.cabinet_compound();
    EXPECT_TRUE(c1.IsSame(c2));
}

TEST(GeometryBuilderTest, ApplyChanges_NonEmptyDeltaMarksCompoundDirty) {
    auto p = make_project_with_one_panel();
    GeometryBuilder b(p);

    const auto panel_id = p.cabinet().panels.begin()->first;
    const TopoDS_Compound& before = b.cabinet_compound();

    ChangeSet cs;
    cs.updated_panels.push_back(panel_id);
    b.apply_changes(cs);

    const TopoDS_Compound& after = b.cabinet_compound();
    // Compound был помечен dirty → пересобран → новый handle.
    EXPECT_FALSE(before.IsSame(after));
}

TEST(GeometryBuilderTest, RebuildAll_ClearsBothCachesAndDirtiesCompound) {
    auto p = make_project_with_one_panel();
    GeometryBuilder b(p);

    const auto panel_id = p.cabinet().panels.begin()->first;
    b.panel_solid(panel_id);
    b.cabinet_compound();
    EXPECT_EQ(b.panel_cache_size(), 1u);

    b.rebuild_all();

    EXPECT_EQ(b.panel_cache_size(), 0u);
    EXPECT_EQ(b.hardware_cache_size(), 0u);
    // Следующий cabinet_compound() должен пересобраться.
    const TopoDS_Compound& after = b.cabinet_compound();
    EXPECT_EQ(after.ShapeType(), TopAbs_COMPOUND);
    EXPECT_EQ(b.panel_cache_size(), 1u) << "cabinet_compound прогревает кеш";
}
```

- [ ] **Step 2: Прогнать (RED)**

```bash
cmake --build --preset default
ctest --preset default --output-on-failure -R GeometryBuilderTest
```

Ожидание: новые тесты падают (заглушка `apply_changes` ничего не делает; `rebuild_all` уже работает из Task 8).

- [ ] **Step 3: Реализовать `apply_changes` в `geometry_builder.cpp`**

Заменить заглушку:

```cpp
void GeometryBuilder::apply_changes(const core::ChangeSet& cs) {
    // 1. Удалённые / изменённые panels и hardware — выселяем из кешей.
    for (const auto& id : cs.removed_panels)    panel_cache_.erase(id);
    for (const auto& id : cs.updated_panels)    panel_cache_.erase(id);
    for (const auto& id : cs.removed_hardware)  hardware_cache_.erase(id);
    for (const auto& id : cs.updated_hardware)  hardware_cache_.erase(id);

    // 2. Любое изменение панелей или cabinet → инвалидируем весь
    // hardware_cache (фурнитура крепится к панелям в panel-local СК).
    if (!cs.updated_panels.empty() || !cs.removed_panels.empty() ||
        cs.cabinet_changed) {
        hardware_cache_.clear();
    }

    // 3. cabinet_changed → role-based панели могут получить другую геометрию.
    // Без анализа разности — чистим весь panel_cache_.
    if (cs.cabinet_changed) {
        panel_cache_.clear();
    }

    // 4. Materials (added/removed/updated) — на shape не влияют (shape
    // зависит от размеров/положения, цвет/текстура — Stage 3).

    // 5. Любая непустая дельта → compound пересобрать.
    if (!cs.empty()) {
        compound_dirty_ = true;
    }
}
```

- [ ] **Step 4: Прогнать**

```bash
cmake --build --preset default
ctest --preset default --output-on-failure -R GeometryBuilderTest
```

Ожидание: все тесты зелёные.

- [ ] **Step 5: Commit**

```bash
git add src/coupecad/geometry/geometry_builder.cpp tests/geometry/geometry_builder_test.cpp
git commit -m "$(cat <<'EOF'
feat(geometry): apply_changes — ChangeSet-driven инвалидация

Реализован алгоритм из spec §5.2:
- removed/updated panels и hardware — erase из кешей
- любое изменение панелей или cabinet_changed → hardware_cache.clear()
  (фурнитура позиционируется в panel-local СК)
- cabinet_changed → panel_cache.clear() (role-based панели зависят
  от размеров шкафа)
- materials игнорируются (shape от них не зависит)
- любая непустая дельта → compound_dirty_ = true

Тесты покрывают каждую ветку отдельно + noop на пустой дельте +
cabinet_compound rebuild через dirty-flag.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 12: Логирование + покрытие compound шкафа интеграционным тестом

**Files:**
- Modify: `src/coupecad/geometry/geometry_builder.cpp`
- Modify: `tests/geometry/geometry_builder_test.cpp`

- [ ] **Step 1: Добавить логирование в `geometry_builder.cpp`**

`Logger` (см. `src/coupecad/logging/logger.h:34-69`) предоставляет fmt-style шорткаты `trace/debug/info/warn/error` — используем их. В начало файла добавить:

```cpp
#include "coupecad/logging/logger.h"
```

Полные обновлённые версии всех четырёх методов (заменяют существующие в `geometry_builder.cpp` целиком):

```cpp
const TopoDS_Solid& GeometryBuilder::panel_solid(const core::PanelId& id) {
    auto it = panel_cache_.find(id);
    if (it != panel_cache_.end()) {
        return it->second;
    }
    coupecad::logging::Logger::instance().trace(
        "geometry", "build panel solid {}", id.to_string());
    const auto& panel = project_.cabinet().panels.at(id);
    auto [inserted_it, _] = panel_cache_.emplace(
        id, build_panel_solid(project_.cabinet(), panel));
    return inserted_it->second;
}

const TopoDS_Compound& GeometryBuilder::hardware_compound(const core::HardwareItemId& id) {
    auto it = hardware_cache_.find(id);
    if (it != hardware_cache_.end()) {
        return it->second;
    }
    coupecad::logging::Logger::instance().trace(
        "geometry", "build hardware compound {}", id.to_string());
    const auto& item = project_.cabinet().hardware.at(id);
    auto [inserted_it, _] = hardware_cache_.emplace(
        id, build_hardware_compound(project_, item));
    return inserted_it->second;
}

void GeometryBuilder::apply_changes(const core::ChangeSet& cs) {
    if (!cs.empty()) {
        coupecad::logging::Logger::instance().debug(
            "geometry",
            "apply_changes: panels(+/-/u)={}/{}/{} hardware(+/-/u)={}/{}/{} cabinet={}",
            cs.added_panels.size(), cs.removed_panels.size(), cs.updated_panels.size(),
            cs.added_hardware.size(), cs.removed_hardware.size(), cs.updated_hardware.size(),
            cs.cabinet_changed ? 1 : 0);
    }

    for (const auto& id : cs.removed_panels)    panel_cache_.erase(id);
    for (const auto& id : cs.updated_panels)    panel_cache_.erase(id);
    for (const auto& id : cs.removed_hardware)  hardware_cache_.erase(id);
    for (const auto& id : cs.updated_hardware)  hardware_cache_.erase(id);

    if (!cs.updated_panels.empty() || !cs.removed_panels.empty() ||
        cs.cabinet_changed) {
        hardware_cache_.clear();
    }

    if (cs.cabinet_changed) {
        panel_cache_.clear();
    }

    if (!cs.empty()) {
        compound_dirty_ = true;
    }
}

void GeometryBuilder::rebuild_all() {
    coupecad::logging::Logger::instance().info(
        "geometry", "rebuild_all: dropping all caches");
    panel_cache_.clear();
    hardware_cache_.clear();
    compound_dirty_ = true;
}
```

- [ ] **Step 2: Добавить интеграционный тест шкафа с панелями + фурнитурой**

В `tests/geometry/geometry_builder_test.cpp`:

```cpp
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>

TEST(GeometryBuilderTest, FullCabinet_CompoundContainsAllShapes) {
    auto p = Project::create_empty("test", make_seeded_uuid_generator(505));
    auto& cab = p.mutable_cabinet();
    cab.dimensions = Dimensions{Millimeters{800}, Millimeters{500}, Millimeters{2000}};
    cab.default_panel_material = p.materials().begin()->first;

    // 4 role-based панели: Bottom, Top, SideLeft, SideRight.
    for (auto role : {PanelRole::Bottom, PanelRole::Top,
                      PanelRole::SideLeft, PanelRole::SideRight}) {
        Panel pn;
        pn.id = PanelId{p.uuid_gen().next()};
        pn.role = role;
        cab.panels.emplace(pn.id, std::move(pn));
    }

    // 1 hardware-item с 1 attachment.
    HardwareRef ref{"hinge.test"};
    HardwareSpec spec;
    spec.ref = ref;
    spec.kind = HardwareKind::Hinge;
    spec.name = "Test";
    spec.bbox = Vec3{Millimeters{50}, Millimeters{30}, Millimeters{20}};
    p.mutable_hardware_catalog().emplace(ref, std::move(spec));

    auto first_panel_id = p.cabinet().panels.begin()->first;
    HardwareItem item;
    item.id = HardwareItemId{p.uuid_gen().next()};
    item.ref = ref;
    item.attachments.push_back(
        PanelAttachment{first_panel_id, Vec3{}, Quat::identity()});
    cab.hardware.emplace(item.id, std::move(item));

    GeometryBuilder b(p);
    const TopoDS_Compound& result = b.cabinet_compound();

    EXPECT_EQ(result.ShapeType(), TopAbs_COMPOUND);

    std::size_t solids = 0;
    for (TopExp_Explorer ex(result, TopAbs_SOLID); ex.More(); ex.Next()) ++solids;
    EXPECT_EQ(solids, 5u) << "4 panels + 1 hardware solid";

    // Bounding box должен покрывать весь шкаф (минимум: 0..800 x 0..500 x 0..2000).
    Bnd_Box bbox;
    BRepBndLib::Add(result, bbox);
    double xmin, ymin, zmin, xmax, ymax, zmax;
    bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);
    EXPECT_NEAR(xmin, 0.0,    1e-6);
    EXPECT_NEAR(xmax, 800.0,  1e-6);
    EXPECT_NEAR(zmin, 0.0,    1e-6);
    EXPECT_NEAR(zmax, 2000.0, 1e-6);
}
```

- [ ] **Step 3: Прогнать всё подряд — убедиться, что весь stage 1 + stage 2 зелёные**

```bash
cmake --build --preset default
ctest --preset default --output-on-failure
```

Ожидание: все тесты зелёные. Если падают какие-то stage 1 тесты — это регрессия, исследовать.

- [ ] **Step 4: Commit**

```bash
git add src/coupecad/geometry/geometry_builder.cpp tests/geometry/geometry_builder_test.cpp
git commit -m "$(cat <<'EOF'
feat(geometry): логирование build-событий + интеграционный тест шкафа

Логирование по категории "geometry":
- Trace: каждый panel_solid/hardware_compound build
- Debug: apply_changes с метриками дельты
- Info: rebuild_all

Интеграционный тест: шкаф из 4 role-based панелей + 1 hardware-item.
Compound содержит 5 solid'ов, bbox покрывает весь шкаф.

Это завершает Stage 2.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Финальная верификация

После Task 12 прогнать в свежем clone'е (или после `cmake --fresh`):

```bash
conan install . --build=missing -s build_type=Debug -s compiler.cppstd=20
cmake --preset default
cmake --build --preset default
ctest --preset default --output-on-failure
```

Ожидание:
- `coupecad_geometry` собирается на Linux (минимум) и Windows (предпочтительно).
- Все тесты Stage 1 (214+) + новые тесты Stage 2 (~15+) зелёные.
- Логи геометрических операций видны в `~/Library/Logs/CoupeCAD/coupecad.log` (macOS) или соответствующем пути под Linux/Windows при включённом Trace для категории `"geometry"`.

---

## Резерв для непредвиденных проблем

Если по ходу обнаружится:

1. **OCCT 7.9.1 не собирается / не найдены prebuilt'ы** → откатить Conan-зависимость на `7.6.2`, запротоколировать причину в `conanfile.py` комментарием. Это не блокирует план — поменяется только версия в Task 1 Step 1.

2. **Линковка падает на отсутствующий `TKxxx`** → добавить в `target_link_libraries(coupecad_geometry PUBLIC ...)`. Список из spec §2.2 — стартовая точка, не финальная.

3. **`Project::mutable_cabinet()` или другие mutable-API исчезли в Stage 1c** → проверить актуальную сигнатуру в `src/coupecad/core/project.h`. Тесты можно переписать на использование Stage 1b команд (`AddPanelCommand` и т.д.) если прямого доступа уже нет.

4. **Имя ChangeSet-полей отличается** → открыть `src/coupecad/core/commands/change_set.h` и взять реальные имена. На момент написания плана: `added_panels`, `removed_panels`, `updated_panels`, аналогично для hardware/materials, `cabinet_changed`, метод `empty()`, метод `merge(const ChangeSet&)`.

5. **`HardwareSpec` не имеет поля `bbox`** → проверить `src/coupecad/core/hardware.h`. На момент написания: `Vec3 bbox{}` — поле прямо в структуре (см. `hardware.h:30`).
