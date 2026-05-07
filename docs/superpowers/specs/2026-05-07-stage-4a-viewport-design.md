# Stage 4a — Viewport (Qt Quick + OCCT) — Design

**Дата:** 2026-05-07
**Статус:** Утверждено для реализации
**Связанные документы:**
- [`2026-04-18-coupecad-design.md`](./2026-04-18-coupecad-design.md) — общая концепция CoupeCAD
- [`2026-04-22-stage-2-geometry-design.md`](./2026-04-22-stage-2-geometry-design.md) — `GeometryBuilder`
- [`2026-05-04-stage-3-renderer-design.md`](./2026-05-04-stage-3-renderer-design.md) — `IRenderer` + OCCT-бэкенд

---

## 1. Цель и объём Stage 4a

Stage 4a добавляет **Qt-Quick-вьюпорт** поверх `IRenderer` Stage 3: пользователь запускает приложение, видит реалтайм-3D-картинку демо-шкафа, орбитит камеру, кликает по панелям и видит подсветку выделения. Без property-панелей, без файлового меню, без undo/redo UI — это Stage 4b и далее.

### 1.1 В объёме

- **Новый Qt-линкуемый модуль `src/coupecad/viewport/`** (статическая библиотека `coupecad_viewport`).
- **`ViewportController`** — pure-C++ слой, владеет рендером и связью с моделью; подписывается на `core::IProjectObserver`; обрабатывает мышь/клавиатуру в команды для рендера; содержит camera-math (orbit/pan/zoom). Тестируется без Qt.
- **`OcctViewportItem : QQuickFramebufferObject`** — Qt Quick item, экспортируется в QML как `OcctViewportItem`. Создаёт `QQuickFramebufferObject::Renderer`, который интегрирует OCCT в Qt-managed GL-контекст.
- **`IOcctGlBackend`** — OCCT-специфичное расширение `IRenderer` (отдельный интерфейс), даёт два метода: `attach_external_gl_driver` и `render_into_current_context`. `OcctRenderer` мульти-наследует `IRenderer` + `IOcctGlBackend`. Публичный `IRenderer` остаётся Qt/OCCT-free.
- **`ViewDriver` рефакторинг** — второй конструктор, принимающий внешний `Handle(OpenGl_GraphicDriver)`. Существующий headless-fallback ctor сохраняется для тестов Stage 3.
- **Демо-шкаф** — `apps/coupecad/demo_project.{h,cpp}` собирает 1200×600×2000 мм шкаф с 6+ панелями при старте приложения. Stage 4b заменит на «открыть `.ccad`».
- **Взаимодействия:** orbit (LMB drag), pan (MMB drag), zoom (wheel), pick (RMB click), fit-all (`F`). Selection-highlight — встроенный `AIS_InteractiveContext`.
- **QML-side debug-overlay:** `Text { text: "Selected: " + count }` через `Q_PROPERTY` биндинги.
- **Тесты:** `tests/viewport/` — pure-C++ тесты `ViewportController` через моковый `IRenderer`, плюс один QML-smoke-тест на `offscreen`-платформе.

### 1.2 Вне объёма

- Property-панели (cabinet/panel/material/hardware editors) — Stage 4b.
- `.ccad` open/save — Stage 4b/c.
- Undo/redo UI — Stage 4b.
- Multi-select / Shift+click / box-select — Stage 4b.
- Keyboard shortcuts кроме `F` — Stage 4b.
- Tooltips, hover-preview — позже.
- Recovery от GL-context loss / окно скрыли-показали — Stage 4b.
- Полная толщина IProjectObserver-семантики (ускоренные обновления конкретных полей) — Stage 4b.
- Stage 11 (Qt Quick 3D backend) — отдельный stage, не сейчас.

### 1.3 Подзадача 4a внутри Stage 4

Полный Stage 4 (UI) слишком крупный для одного спека: viewport + property-panels + file-menu + undo-buttons + toolbar — это пять подсистем. Делим:
- **Stage 4a** (этот документ) — viewport + минимальный run-loop.
- **Stage 4b** — property-panels + selection-driven UI.
- **Stage 4c** — file menu (.ccad open/save) + undo/redo buttons.
- **Stage 4d (опц.)** — toolbar/status bar полировка.

Каждый подстейдж заканчивается набором зелёных тестов и работающим демо.

---

## 2. Структура модуля

### 2.1 Файлы и расположение

```
src/coupecad/viewport/
    CMakeLists.txt
    viewport_controller.h       # pure C++, IProjectObserver, camera/mouse logic
    viewport_controller.cpp
    occt_viewport_item.h        # QQuickFramebufferObject + Renderer
    occt_viewport_item.cpp

src/coupecad/renderer/occt/
    i_occt_gl_backend.h         # NEW — расширение IRenderer для OCCT-GL hooks
    occt_renderer.h             # +inherits IOcctGlBackend
    occt_renderer.cpp           # +impl новых методов
    view_driver.h               # +2nd ctor (external driver)
    view_driver.cpp             # +impl

apps/coupecad/
    main.cpp                    # +регистрация коупэкад-стека и controller
    demo_project.h              # NEW
    demo_project.cpp            # NEW
    qml/Main.qml                # +OcctViewportItem
```

### 2.2 CMake-таргеты

`src/coupecad/viewport/CMakeLists.txt`:

```cmake
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Quick OpenGL)

add_library(coupecad_viewport STATIC
    viewport_controller.cpp
    occt_viewport_item.cpp
)

target_include_directories(coupecad_viewport PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(coupecad_viewport
    PUBLIC
        coupecad_core
        coupecad_geometry
        coupecad_renderer
        coupecad_renderer_occt
        coupecad_logging
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::OpenGL
)

target_compile_features(coupecad_viewport PUBLIC cxx_std_20)

set_target_properties(coupecad_viewport PROPERTIES
    AUTOMOC ON
)
```

`apps/coupecad/CMakeLists.txt` (изменения):

```cmake
qt_add_executable(coupecad
    WIN32 MACOSX_BUNDLE
    main.cpp
    demo_project.cpp
)

qt_add_qml_module(coupecad
    URI coupecad
    VERSION 1.0
    QML_FILES qml/Main.qml
)

target_link_libraries(coupecad
    PRIVATE
        Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
        coupecad_viewport
)

# ... existing MACOSX_BUNDLE/RUNTIME_OUTPUT props ...
```

`src/CMakeLists.txt` — добавить `add_subdirectory(coupecad/viewport)` после renderer.

### 2.3 Зависимости (направленный граф)

```
coupecad_viewport       →  coupecad_renderer (interface)
                        →  coupecad_renderer_occt (PUBLIC: для IOcctGlBackend cast)
                        →  coupecad_geometry
                        →  coupecad_core
                        →  coupecad_logging
                        →  Qt6::Core/Gui/Quick/OpenGL
apps/coupecad           →  coupecad_viewport
                        →  Qt6::Qml/Quick
```

`coupecad_renderer_occt` остаётся Qt-free; `coupecad_viewport` — это слой, в котором Qt и OCCT встречаются.

---

## 3. Public API

### 3.1 `ViewportController` (pure C++)

`src/coupecad/viewport/viewport_controller.h`:

```cpp
#pragma once

#include "coupecad/core/commands/change_set.h"
#include "coupecad/core/project.h"
#include "coupecad/renderer/entity_id.h"
#include "coupecad/renderer/i_renderer.h"

#include <vector>

namespace coupecad::geometry { class GeometryBuilder; }

namespace coupecad::viewport {

enum class MouseButton { None, Left, Middle, Right };

// Pure-C++ контроллер: связывает Project, GeometryBuilder и IRenderer,
// обрабатывает мышь/клавиатуру, толкает ChangeSet'ы рендеру.
//
// Не thread-safe. Все вызовы — из UI-потока. QFBO Renderer общается
// с контроллером ТОЛЬКО через synchronize() (блокирующая точка).
class ViewportController : public core::IProjectObserver {
public:
    ViewportController(core::Project& project,
                       geometry::GeometryBuilder& builder,
                       renderer::IRenderer& renderer);
    ~ViewportController() override;

    ViewportController(const ViewportController&) = delete;
    ViewportController& operator=(const ViewportController&) = delete;

    // IProjectObserver — вызывается из UndoStack после execute/undo/redo.
    void on_changed(const core::Project&, const core::ChangeSet& cs) override;

    // Полная пересборка builder'а и рендера. Вызвать после конструктора
    // (демо-шкаф уже собран в Project) и после deserialize.
    void rebuild_from_scratch();

    // Mouse / keyboard — пиксельные координаты, viewport-relative,
    // origin = top-left.
    void on_mouse_press(int x, int y, MouseButton btn);
    void on_mouse_move(int x, int y, MouseButton btn);
    void on_mouse_release(int x, int y, MouseButton btn);
    void on_wheel(int x, int y, double delta_steps);
    void fit_all();

    // Состояние, нужное QML-биндингам.
    renderer::CameraState         camera() const;
    std::vector<renderer::EntityId> selection() const;
    bool                          selection_empty() const;
    std::size_t                   selection_count() const;

    // QFBO redraw signaling.
    bool dirty() const noexcept;
    void mark_clean() noexcept;

    // Sets the viewport size (called from QFBO synchronize).
    void set_viewport_size(int width, int height);

private:
    core::Project&             project_;
    geometry::GeometryBuilder& builder_;
    renderer::IRenderer&       renderer_;
    bool                       dirty_ = true;

    // Drag state.
    MouseButton active_drag_ = MouseButton::None;
    int         drag_last_x_ = 0;
    int         drag_last_y_ = 0;
    int         drag_total_dx_ = 0;
    int         drag_total_dy_ = 0;
};

}  // namespace coupecad::viewport
```

### 3.2 Поведение `ViewportController`

**`on_changed(cs)`:**
1. `builder_.apply_changes(cs);`
2. `renderer_.sync(cs);`
3. `dirty_ = true;`

**`rebuild_from_scratch()`:**
1. `builder_.rebuild_all();`
2. `renderer_.rebuild_all();`
3. `renderer_.fit_all();`
4. `dirty_ = true;`

**`on_mouse_press(x, y, btn)`:**
- Записывает `active_drag_ = btn`, `drag_last_x_/y_ = x, y`, `drag_total_dx_/dy_ = 0`.
- Никаких немедленных side-effects (pick срабатывает на release без drag).

**`on_mouse_move(x, y, btn)`:**
- Если `btn != active_drag_` — игнорировать (no-op).
- Δx = x - drag_last_x_, Δy = y - drag_last_y_; drag_total_dx_ += |Δx|; drag_total_dy_ += |Δy|.
- LMB: orbit. Маппинг: 0.5° на пиксель вокруг target. Реализация — в §3.4.
- MMB: pan. Δworld = -Δscreen × view_distance × tan(fov/2) / viewport_height; trasnlate eye+target в plane right/up.
- RMB: drag не реагирует (до release).
- `drag_last_x_ = x; drag_last_y_ = y; dirty_ = true;`

**`on_mouse_release(x, y, btn)`:**
- Если `btn == Right` и `drag_total_dx_ + drag_total_dy_ < pick_drag_threshold` (5 пикселей) — это **click**, не drag:
  - `auto hit = renderer_.pick(x, y);`
  - Если есть — `renderer_.clear_selection(); renderer_.select(*hit);`
  - Иначе — `renderer_.clear_selection();`
  - `dirty_ = true;`
- Сбросить drag-state: `active_drag_ = MouseButton::None;`

**`on_wheel(x, y, delta_steps)`:**
- factor = `pow(1.1, -delta_steps)` (делёное на 1.1 на каждый wheel-tick вверх; колесо вверх → zoom in).
- Camera state: `eye = target + (eye - target) × factor;`
- `renderer_.set_camera(updated); dirty_ = true;`

**`fit_all()`:** `renderer_.fit_all(); dirty_ = true;`

**`set_viewport_size(w, h)`:** `renderer_.set_viewport_size({w, h}); dirty_ = true;`

### 3.3 Mouse-button decision: RMB=pick, LMB=orbit

Stage 4a имеет минимальный selection-UX. Сделаем «drag = orbit, click = pick» через RMB, чтобы не разделять одну кнопку на два режима. Stage 4b пересмотрит, когда появятся property-panels и стандарт станет важнее.

### 3.4 Камера: orbit-математика

Track-ball модель:
- Преобразуем (eye - target) в спор. координаты вокруг up-axis.
- Δazimuth = -Δx × 0.5°, Δelevation = -Δy × 0.5°.
- Ограничиваем elevation на [-89°, +89°].
- Восстанавливаем eye = target + R(azimuth, elevation) × |eye - target|.

Возможна потеря точности из-за int32 mm (см. §10 риск 1) — для Stage 4a допустимо, drift измеряем визуально.

### 3.5 `IOcctGlBackend` — OCCT-специфичные хуки

`src/coupecad/renderer/occt/i_occt_gl_backend.h`:

```cpp
#pragma once

#include <OpenGl_GraphicDriver.hxx>

namespace coupecad::renderer::occt {

// OCCT-специфичный интерфейс, ОТДЕЛЬНЫЙ от IRenderer. Концретный
// OcctRenderer реализует и IRenderer (Qt-Quick-3D-portable), и
// IOcctGlBackend (OCCT-only). QFBO Renderer dynamic_cast'ит к этому.
class IOcctGlBackend {
public:
    virtual ~IOcctGlBackend() = default;

    // Перевести рендерер в режим внешнего GL-драйвера. Старая AIS-сцена
    // теряется (ViewDriver/AisScene пересоздаются). Должен быть вызван
    // ДО первого render_into_current_context().
    virtual void attach_external_gl_driver(
        const Handle(OpenGl_GraphicDriver)& driver) = 0;

    // Отрендерить текущий View в текущий bound GL framebuffer.
    // Caller отвечает за то, что FBO привязан и контекст активен.
    // Без software-fallback. Бросает renderer.gl_unavailable, если
    // attach_external_gl_driver() не был вызван.
    virtual void render_into_current_context() = 0;
};

}  // namespace coupecad::renderer::occt
```

`OcctRenderer` (изменения):
- Наследует `IRenderer, public IOcctGlBackend`.
- `attach_external_gl_driver(drv)`: пересоздаёт `driver_` (ViewDriver) и `scene_` (AisScene) с новым OCCT-драйвером. Перед этим — текущая сцена очищается (`scene_.clear()`); после — `rebuild_all()` повторно набивает её из `project_` (lazy через builder_).
- `render_into_current_context()`: `driver_.view()->Redraw();`. Если `gl_available_ == false` — throw `renderer.gl_unavailable`.

`make_occt_renderer` продолжает возвращать `unique_ptr<IRenderer>`; QFBO Renderer делает `dynamic_cast<IOcctGlBackend*>(ptr)`.

### 3.6 `ViewDriver` рефакторинг

Дополнительный конструктор:

```cpp
class ViewDriver {
public:
    ViewDriver();                                                    // existing
    explicit ViewDriver(const Handle(OpenGl_GraphicDriver)& driver); // NEW

    // ... existing public API unchanged ...
};
```

Реализация: новый ctor пропускает `try_make_gl_driver`, ставит `driver_ = driver; gl_available_ = true;` и собирает Viewer/View/Window как обычно.

### 3.7 `OcctViewportItem` (Qt Quick)

`src/coupecad/viewport/occt_viewport_item.h`:

```cpp
#pragma once

#include "coupecad/viewport/viewport_controller.h"

#include <QQuickFramebufferObject>
#include <QtQml/qqmlregistration.h>

namespace coupecad::viewport {

class OcctViewportItem : public QQuickFramebufferObject {
    Q_OBJECT
    Q_PROPERTY(bool selectionEmpty READ selection_empty NOTIFY selectionChanged)
    Q_PROPERTY(int  selectionCount READ selection_count NOTIFY selectionChanged)

public:
    explicit OcctViewportItem(QQuickItem* parent = nullptr);
    ~OcctViewportItem() override;

    Q_INVOKABLE void    set_controller(ViewportController* c);
    ViewportController* controller() const noexcept { return controller_; }

    Renderer* createRenderer() const override;

    bool   selection_empty() const;
    int    selection_count() const;

protected:
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

signals:
    void selectionChanged();

private:
    ViewportController* controller_ = nullptr;
};

}  // namespace coupecad::viewport
```

### 3.8 `OcctFboRenderer` (private, в `occt_viewport_item.cpp`)

```cpp
class OcctFboRenderer : public QQuickFramebufferObject::Renderer {
public:
    explicit OcctFboRenderer(ViewportController* c);

    QOpenGLFramebufferObject* createFramebufferObject(const QSize& size) override;
    void synchronize(QQuickFramebufferObject* item) override;
    void render() override;

private:
    void initialise_occt_if_needed();

    ViewportController*         controller_;
    Handle(OpenGl_GraphicDriver) driver_;
    bool                        gl_initialised_ = false;
    QSize                       last_fbo_size_;
};
```

**Lifecycle:**
- `createRenderer()` (called by Qt on item realize) returns a fresh `OcctFboRenderer`.
- `synchronize(item)` — runs with both threads blocked. Reads `controller_->dirty()`, lazy-initializes OCCT GL on first call (here we have access to the live Qt GL context and the item's `window()`), and forwards FBO size changes to the controller via `controller_->set_viewport_size(last_fbo_size_.width(), last_fbo_size_.height())` whenever `last_fbo_size_` differs from previous.
- `render()` — render-thread only. Calls `dynamic_cast<IOcctGlBackend*>(&controller_->renderer())->render_into_current_context()`. The currently-bound FBO is the one Qt expects us to draw into.
- `createFramebufferObject(size)` — returns `QOpenGLFramebufferObject` with `Attachment::CombinedDepthStencil`, MSAA samples = 4. On size change, sets controller's viewport size in next `synchronize()`.

### 3.9 Mouse → Controller threading

QML / `OcctViewportItem` events run in UI-thread:
- `mousePressEvent` → `controller_->on_mouse_press(x, y, btn);`
- `mouseMoveEvent` → `controller_->on_mouse_move(x, y, btn);`
- `mouseReleaseEvent` → `controller_->on_mouse_release(x, y, btn);` + emit `selectionChanged()` if needed.
- `wheelEvent` → `controller_->on_wheel(x, y, delta);`
- `keyPressEvent` (`Qt::Key_F`) → `controller_->fit_all();`

После каждого вызова: `update();` чтобы Qt запланировал перерисовку. `synchronize()` потом увидит `controller_->dirty() == true`, передаст состояние, `render()` сделает работу.

---

## 4. App startup и demo-project

### 4.1 `apps/coupecad/main.cpp`

```cpp
#include "coupecad/core/project.h"
#include "coupecad/core/undo_stack.h"
#include "coupecad/geometry/geometry_builder.h"
#include "coupecad/renderer/occt/occt_renderer.h"
#include "coupecad/viewport/viewport_controller.h"
#include "demo_project.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QString>

#include <iostream>
#include <memory>

namespace { constexpr const char* kAppVersion = "0.1.0"; }

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("CoupeCAD"));
    app.setApplicationVersion(QString::fromLatin1(kAppVersion));

    for (int i = 1; i < argc; ++i) {
        const QString a = QString::fromLocal8Bit(argv[i]);
        if (a == QStringLiteral("--version") || a == QStringLiteral("-v")) {
            std::cout << "CoupeCAD " << kAppVersion << std::endl;
            return 0;
        }
    }

    // Build the data layer.
    auto project    = std::make_unique<coupecad::core::Project>(
        coupecad::app::make_demo_project());
    auto undo       = std::make_unique<coupecad::core::UndoStack>(*project);
    auto builder    = std::make_unique<coupecad::geometry::GeometryBuilder>(
        *project);
    auto renderer   = coupecad::renderer::occt::make_occt_renderer(
        *project, *builder);
    auto controller = std::make_unique<coupecad::viewport::ViewportController>(
        *project, *builder, *renderer);
    undo->add_observer(controller.get());
    controller->rebuild_from_scratch();

    // Register OcctViewportItem before loading QML. coupecad_viewport
    // is a plain static lib (not qt_add_qml_module), so explicit
    // qmlRegisterType is required.
    qmlRegisterType<coupecad::viewport::OcctViewportItem>(
        "coupecad", 1, 0, "OcctViewportItem");

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(
        "viewportController", QVariant::fromValue(controller.get()));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("coupecad", "Main");
    return app.exec();
}
```

### 4.2 `apps/coupecad/demo_project.h`

```cpp
#pragma once

#include "coupecad/core/project.h"

namespace coupecad::app {

// Build a sample 1200×600×2000 mm cabinet for the demo. No hardware in 4a.
core::Project make_demo_project();

}  // namespace coupecad::app
```

### 4.3 `apps/coupecad/demo_project.cpp`

~30-line implementation: `Project::create_empty("Demo")`, sets cabinet dimensions, pushes Bottom + Top + SideLeft + SideRight + Back + 2 Shelves into `mutable_cabinet().panels` with `next_id<PanelIdTag>()`. Returns by value.

### 4.4 `apps/coupecad/qml/Main.qml`

```qml
import QtQuick
import QtQuick.Window
import coupecad

Window {
    id: root
    width: 1280
    height: 720
    minimumWidth: 640
    minimumHeight: 480
    visible: true
    title: qsTr("CoupeCAD")

    OcctViewportItem {
        id: viewport
        anchors.fill: parent
        focus: true

        Component.onCompleted: viewport.setController(viewportController)
    }

    Text {
        anchors { left: parent.left; top: parent.top; margins: 8 }
        text: viewport.selectionEmpty
              ? qsTr("No selection")
              : qsTr("Selected: ") + viewport.selectionCount
        color: "white"
        font.pixelSize: 14
        style: Text.Outline
        styleColor: "black"
    }
}
```

> `viewport.setController(viewportController)` — wraps `set_controller` exposed via `Q_INVOKABLE`. Add `Q_INVOKABLE` to `OcctViewportItem::set_controller` to make it callable from QML.

---

## 5. Тестирование

### 5.1 Файлы

```
tests/viewport/
    CMakeLists.txt
    fake_renderer.h                          # mock IRenderer
    viewport_controller_test.cpp             # rebuild_from_scratch, dirty flag, on_changed
    viewport_controller_camera_test.cpp      # orbit/pan/zoom math
    viewport_controller_pick_test.cpp        # RMB click → pick → select
    viewport_controller_observer_test.cpp    # UndoStack → controller → renderer
    viewport_qml_smoke_test.cpp              # offscreen QML load
```

### 5.2 `FakeRenderer`

```cpp
class FakeRenderer : public coupecad::renderer::IRenderer {
public:
    std::vector<core::ChangeSet>           sync_calls;
    std::vector<renderer::CameraState>     set_camera_calls;
    std::vector<renderer::EntityId>        select_calls;
    bool                                   clear_selection_called = false;
    bool                                   fit_all_called = false;
    bool                                   rebuild_all_called = false;
    std::optional<renderer::EntityId>      next_pick_result;

    void sync(const core::ChangeSet& cs) override { sync_calls.push_back(cs); }
    void rebuild_all() override                   { rebuild_all_called = true; }
    void set_camera(const renderer::CameraState& s) override
                                                  { set_camera_calls.push_back(s); current_ = s; }
    renderer::CameraState camera() const override { return current_; }
    void fit_all() override                       { fit_all_called = true; }
    std::optional<renderer::EntityId> pick(int, int) override
                                                  { return next_pick_result; }
    void select(const renderer::EntityId& id) override
                                                  { select_calls.push_back(id); selection_.push_back(id); }
    void deselect(const renderer::EntityId&) override {}
    void clear_selection() override               { clear_selection_called = true; selection_.clear(); }
    std::vector<renderer::EntityId> selection() const override { return selection_; }
    void set_viewport_size(renderer::ViewportSize) override {}
    renderer::ViewportSize viewport_size() const override { return {800, 600}; }
    std::vector<std::uint8_t> render_to_image() override { return {}; }

private:
    renderer::CameraState current_{};
    std::vector<renderer::EntityId> selection_;
};
```

### 5.3 Тесты `ViewportController`

**`viewport_controller_test.cpp`:**
- `RebuildFromScratchClearsAndPopulates`: создать controller, вызвать `rebuild_from_scratch()`, ожидать `fake.rebuild_all_called == true`, `fake.fit_all_called == true`, `controller.dirty() == true`.
- `MarkCleanClearsDirty`: после rebuild, `mark_clean()` → `dirty() == false`.
- `OnChangedForwardsCsToRenderer`: построить `ChangeSet` с `added_panels = {pid}`, вызвать `controller.on_changed(project, cs)`, ожидать `fake.sync_calls.size() == 1` и `fake.sync_calls[0].added_panels == {pid}`.

**`viewport_controller_camera_test.cpp`:**
- `LmbDragOrbits`: установить камеру `eye=(0,-1000,0) target=(0,0,0)`, вызвать `mouse_press(LMB)`, `mouse_move(+50px, 0)`, ожидать `fake.set_camera_calls.size() >= 1`. Eye после поворота на 25° по azimuth: `x ≈ 1000*sin(25°) ≈ 423`, `y ≈ -1000*cos(25°) ≈ -906`.
- `MmbDragPans`: аналогично с MMB, ожидать сдвиг target/eye в plane.
- `WheelZoomsIn`: `on_wheel(0, 0, +1)`, ожидать distance × (1/1.1).
- `FitAllDelegates`: `controller.fit_all()` → `fake.fit_all_called == true`.

**`viewport_controller_pick_test.cpp`:**
- `RmbClickWithHitSelectsId`: `fake.next_pick_result = pid;`, RMB press+release без drag (Δ < 5px), ожидать `fake.clear_selection_called == true`, `fake.select_calls == {pid}`.
- `RmbClickWithMissClearsSelection`: `next_pick_result = nullopt;`, RMB click → `clear_selection_called == true`, `select_calls.empty()`.
- `RmbDragDoesNotPick`: RMB press, mouse_move(+10px, 0) (выше threshold), release → ни pick, ни select.

**`viewport_controller_observer_test.cpp`:**
- Wire реальный `Project + UndoStack + GeometryBuilder + FakeRenderer + ViewportController`.
- `UndoStackExecuteForwardsChangeSet`: execute `AddPanel(...)` → ожидать `fake.sync_calls.size() == 1` с правильным `added_panels`.
- `UndoStackUndoForwardsRevertChangeSet`: после execute → undo, ожидать `sync_calls.size() == 2`, второй вызов с `removed_panels = {pid}`.

**`viewport_qml_smoke_test.cpp`:**
- Запустить `QGuiApplication` с `QT_QPA_PLATFORM=offscreen`, создать `QQmlApplicationEngine`, `loadFromModule("coupecad", "Main")`.
- `findChild<OcctViewportItem*>` → не null.
- Проверить, что `OcctViewportItem` создан без exception.
- Не вызывать `render()` — на offscreen FBO/GL может не работать. Только wiring.

### 5.4 Coverage

`coupecad_viewport` ≥ 60% lines (QFBO `render()` GL-путь не покрыт; всё остальное — да).

### 5.5 Без визуальной верификации

В Stage 4a не делаем pixel-comparison или screenshot тестов. Visual quality assessment — manual на macOS dev-машине разработчика.

---

## 6. Логирование

Категория `"viewport"` в `coupecad::logging::Logger`.

| Уровень | Когда |
|---|---|
| `Info` | `ViewportController` ctor; `attach_external_gl_driver` success; `rebuild_from_scratch`. |
| `Debug` | mouse-drag deltas (Δazimuth, Δpan, zoom factor); pick hits per-id. |
| `Trace` | per-frame dirty flag flips; `synchronize()` data exchange. |
| `Warn` | pick miss inside viewport (informational); `update()` без attached controller'а. |
| `Error` | `IOcctGlBackend` cast failure; `attach_external_gl_driver` throw. |

---

## 7. Коды ошибок

Все — подклассы `core::DomainError`, lowercase, dot-separated:

| Код | Когда |
|---|---|
| `viewport.controller_not_attached` | `OcctViewportItem` рендерится без attached `ViewportController`. |
| `renderer.external_gl_unsupported` | `attach_external_gl_driver` на бэкенде, который не реализует `IOcctGlBackend` (Stage 11 qq3d, future). |
| `renderer.gl_unavailable` (existing) | `render_into_current_context()` до `attach_external_gl_driver()`. |

---

## 8. Открытые вопросы / риски

1. **Camera precision (carry-over from Stage 3 review).** `OcctRenderer::camera()` truncates double-mm to int32 mm. Orbit accumulates drift. Stage 4a: принимаем; если drift visible после 60-секундной orbit-сессии — отдельный follow-up до Stage 4b. (План фиксируется не сейчас.)
2. **Multi-context lifetime.** Если Qt пересоздаёт QSG render-thread (window hide/show, GPU reset, screen change) — наш `OpenGl_GraphicDriver` становится stale, рендер ломается. Stage 4a не обрабатывает; документируем как known issue, фиксим в 4b.
3. **MSAA коллизия.** OCCT `V3d_View` имеет свой anti-aliasing; Qt FBO имеет MSAA samples. Двойной AA → артефакты. Stage 4a: отключаем OCCT-side AA, оставляем только Qt FBO MSAA. Verified visually.
4. **High-DPI.** `QQuickFramebufferObject` honors `devicePixelRatio` автоматически; FBO size = logical-size × dpr. Verify, что OCCT view receives physical, not logical, size в `set_viewport_size`. Если QFBO даёт `QSize` в physical — норм; если в logical — multiply внутри renderer.
5. **`Q_PROPERTY` notify timing.** `selectionChanged` signal должен emitted, когда `ViewportController::selection()` меняется. В Stage 4a это происходит из `OcctViewportItem::mouseReleaseEvent` (после возврата из controller'а сравниваем `selection_count()` до/после; если изменился — emit). Корректно для нашего flow, но не покрывает изменения через `IProjectObserver`. Stage 4b — generic mechanism.
6. **macOS GL drift.** На macOS Apple deprecated OpenGL; Qt 6 использует Cocoa GL surface. OCCT 7.9.1 поддерживает Apple, но deprecated path. Если возникнут проблемы — fallback на Metal-через-MoltenVK is out of scope, downgrade to QQuickPaintedItem mode (option from §1) на этом backend.
7. **`AIS_InteractiveContext::Activate` для selection-mode.** Stage 3 заметил, что AIS_Shape default mode 0 работает; в Stage 4a проверим, что pick на real Qt-GL контексте даёт same result. Если нет — explicit `Activate(ais, 0)` в `AisScene::add_panel/add_hardware`.

---

## 9. Критерии готовности Stage 4a

- [ ] `coupecad_viewport` собирается на Linux + Windows CI.
- [ ] `apps/coupecad` запускается, открывает окно, отображает демо-шкаф (manual check на macOS-машине разработчика).
- [ ] LMB-drag вращает камеру; MMB-drag — pan; колесо — zoom (manual).
- [ ] RMB click на панели → выделение, debug-overlay показывает «Selected: 1» (manual).
- [ ] `F` фитит-all (manual).
- [ ] `ViewportController` тесты — все паттерны mouse/wheel/observer покрыты.
- [ ] QML smoke-test проходит на `offscreen` платформе.
- [ ] Existing 307 тестов Stage 3 не сломаны.
- [ ] Public `IRenderer` остался без OCCT/Qt-зависимостей; OCCT-side hooks — только в `IOcctGlBackend`.
- [ ] No crashes/leaks в 60-секундной orbit-сессии (manual run).
- [ ] Coverage `coupecad_viewport` ≥ 60% lines.
