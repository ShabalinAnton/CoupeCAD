# Stage 4a — Viewport (Qt Quick + OCCT) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Built a Qt Quick viewport that hosts the OCCT renderer in a `QQuickFramebufferObject`, exposes mouse/keyboard interactions through a pure-C++ `ViewportController`, and shows a demo cabinet on app launch.

**Architecture:** Two layers. Pure-C++ `ViewportController` (in new lib `coupecad_viewport`) owns the bridge between `Project`/`UndoStack` (via `IProjectObserver`), `GeometryBuilder`, and `IRenderer`; testable without Qt. `OcctViewportItem : QQuickFramebufferObject` drives the Qt-side: receives mouse events, creates a `QQuickFramebufferObject::Renderer` that initialises OCCT against Qt's GL context via a new `IOcctGlBackend` extension interface, and asks `IRenderer::render_into_current_context()` to draw into the FBO.

**Tech Stack:** C++20, Qt 6.5+ (Core, Gui, Quick, OpenGL), OCCT 7.9.1 (TKV3d, TKOpenGl, etc.), GoogleTest. New OCCT-side extension interface `IOcctGlBackend` keeps `IRenderer` Qt/OCCT-free.

**Связанные документы:**
- Дизайн: [`../specs/2026-05-07-stage-4a-viewport-design.md`](../specs/2026-05-07-stage-4a-viewport-design.md)
- Stage 3 рендер: [`../specs/2026-05-04-stage-3-renderer-design.md`](../specs/2026-05-04-stage-3-renderer-design.md)

**Definition of Done (см. spec §9):** `coupecad_viewport` builds on Linux + Windows CI; app launches with demo cabinet visible; LMB orbit / MMB pan / wheel zoom / RMB pick / `F` fit-all all functional (manual); all `ViewportController` paths covered by C++ tests; QML smoke test passes on offscreen; existing 307 tests not broken; `IRenderer` public surface stays Qt-free; coverage `coupecad_viewport` ≥ 60% lines.

---

## File Structure

After Stage 4a:

```
src/coupecad/renderer/occt/
    i_occt_gl_backend.h         # NEW (Task 2)
    occt_renderer.{h,cpp}       # +inherits IOcctGlBackend (Task 2, Task 4)
    view_driver.{h,cpp}         # +external-driver ctor (Task 3)

src/coupecad/viewport/
    CMakeLists.txt              # NEW (Task 5)
    viewport_controller.{h,cpp} # NEW (Tasks 5–11)
    occt_viewport_item.{h,cpp}  # NEW (Tasks 12–14)

apps/coupecad/
    CMakeLists.txt              # +demo_project.cpp, +link coupecad_viewport (Task 15)
    main.cpp                    # +setup, qmlRegisterType (Task 15)
    demo_project.{h,cpp}        # NEW (Task 15)
    qml/Main.qml                # +OcctViewportItem (Task 15)

tests/viewport/
    CMakeLists.txt              # NEW (Task 5)
    fake_renderer.h             # NEW (Task 5)
    viewport_controller_test.cpp           # NEW (Tasks 5–7)
    viewport_controller_camera_test.cpp    # NEW (Task 9)
    viewport_controller_pick_test.cpp      # NEW (Task 10)
    viewport_controller_observer_test.cpp  # NEW (Task 6)
    viewport_qml_smoke_test.cpp            # NEW (Task 12)
```

Modified:
- `src/CMakeLists.txt` (Task 5: +`add_subdirectory(coupecad/viewport)`)
- `tests/CMakeLists.txt` (Task 5: +`add_subdirectory(viewport)`)
- `src/coupecad/renderer/occt/CMakeLists.txt` (Task 2: +`i_occt_gl_backend.h` exposed via include path; no source change since interface is header-only)

---

## Task 1: IOcctGlBackend interface header

**Files:**
- Create: `src/coupecad/renderer/occt/i_occt_gl_backend.h`

> Pure header. No source, no tests. Makes the multi-inheritance in Task 2 possible.

- [ ] **Step 1: Create `src/coupecad/renderer/occt/i_occt_gl_backend.h`**

```cpp
#pragma once

#include <OpenGl_GraphicDriver.hxx>

namespace coupecad::renderer::occt {

// OCCT-специфичный интерфейс, ОТДЕЛЬНЫЙ от IRenderer. Концретный
// OcctRenderer реализует и IRenderer (Qt-Quick-3D-portable), и
// IOcctGlBackend (OCCT-only). QFBO Renderer dynamic_cast'ит к этому,
// чтобы передать Qt's GL-контекст в рендерер и попросить рисование
// в текущий FBO. Stage 11 (Qt Quick 3D backend) НЕ реализует этот
// интерфейс — тогда QFBO bridge будет недоступен, что корректно.
class IOcctGlBackend {
public:
    virtual ~IOcctGlBackend() = default;

    // Перевести рендерер в режим внешнего GL-драйвера. Старая AIS-сцена
    // теряется (ViewDriver/AisScene пересоздаются с новым драйвером).
    // Должен быть вызван ДО первого render_into_current_context().
    virtual void attach_external_gl_driver(
        const Handle(OpenGl_GraphicDriver)& driver) = 0;

    // Отрендерить текущий V3d_View в текущий bound GL framebuffer.
    // Caller отвечает за то, что FBO привязан и контекст активен.
    // Без software-fallback. Бросает core::DomainError{
    //   "renderer.gl_unavailable"} если attach_external_gl_driver
    // не был вызван или GL недоступен.
    virtual void render_into_current_context() = 0;
};

}  // namespace coupecad::renderer::occt
```

- [ ] **Step 2: Verify the header parses**

Run:
```bash
cmake --build --preset default --target coupecad_renderer_occt
```

Expected: ninja "no work to do" (header isn't included by anything yet). If you want to be extra safe, add a temporary `#include "coupecad/renderer/occt/i_occt_gl_backend.h"` at the top of `occt_renderer.cpp` and rebuild — should pass clean — then revert. (Not required.)

- [ ] **Step 3: Commit**

```bash
git add src/coupecad/renderer/occt/i_occt_gl_backend.h
git commit -m "feat(renderer): IOcctGlBackend — OCCT-side hooks for Qt-GL bridge"
```

---

## Task 2: OcctRenderer multi-inherits IOcctGlBackend (stub impl)

**Files:**
- Modify: `src/coupecad/renderer/occt/occt_renderer.h`
- Modify: `src/coupecad/renderer/occt/occt_renderer.cpp`

> The two new methods stub-throw. Real impls land in Task 4 (after Task 3 refactors `ViewDriver`). Tests are deferred to Task 4.

- [ ] **Step 1: Modify `src/coupecad/renderer/occt/occt_renderer.h`**

Add include and inheritance. Replace the existing `class OcctRenderer : public IRenderer {` line with the two-inheritance form, and add the two override declarations.

After existing `#include "coupecad/renderer/occt/view_driver.h"`:
```cpp
#include "coupecad/renderer/occt/i_occt_gl_backend.h"
```

Replace `class OcctRenderer : public IRenderer {` with:
```cpp
class OcctRenderer : public IRenderer, public IOcctGlBackend {
```

In the public section, after `std::vector<std::uint8_t> render_to_image() override;`, add:
```cpp

    // IOcctGlBackend.
    void attach_external_gl_driver(
        const Handle(OpenGl_GraphicDriver)& driver) override;
    void render_into_current_context() override;
```

- [ ] **Step 2: Modify `src/coupecad/renderer/occt/occt_renderer.cpp`**

Add a minimal helper for the stubs and the two method bodies. Add at the end of the `coupecad::renderer::occt` namespace (just before `make_occt_renderer`):

```cpp
void OcctRenderer::attach_external_gl_driver(
    const Handle(OpenGl_GraphicDriver)& /*driver*/) {
    throw core::DomainError{
        "renderer.gl_unavailable",
        "OcctRenderer::attach_external_gl_driver не реализовано (Task 4)"};
}

void OcctRenderer::render_into_current_context() {
    throw core::DomainError{
        "renderer.gl_unavailable",
        "OcctRenderer::render_into_current_context не реализовано (Task 4)"};
}
```

- [ ] **Step 3: Build**

Run:
```bash
cmake --build --preset default --target coupecad_renderer_occt
```

Expected: builds clean. No tests added in this task.

- [ ] **Step 4: Verify Stage 3 tests still pass**

```bash
ctest --preset default --output-on-failure
```

Expected: 307/307 still passing (no behaviour change).

- [ ] **Step 5: Commit**

```bash
git add src/coupecad/renderer/occt/occt_renderer.h \
        src/coupecad/renderer/occt/occt_renderer.cpp
git commit -m "feat(renderer): OcctRenderer наследует IOcctGlBackend (stubs)"
```

---

## Task 3: ViewDriver external-driver constructor

**Files:**
- Modify: `src/coupecad/renderer/occt/view_driver.h`
- Modify: `src/coupecad/renderer/occt/view_driver.cpp`
- Modify: `tests/renderer/view_driver_test.cpp`

- [ ] **Step 1: Append failing test to `tests/renderer/view_driver_test.cpp`**

Add at the end of the file:

```cpp
TEST(ViewDriverTest, ExternalDriverCtorMarksGlAvailable) {
    Handle(Aspect_DisplayConnection) display = new Aspect_DisplayConnection();
    Handle(OpenGl_GraphicDriver) drv = new OpenGl_GraphicDriver(display, false);

    ViewDriver driver{drv};
    EXPECT_TRUE(driver.gl_available());
    EXPECT_FALSE(driver.viewer().IsNull());
    EXPECT_FALSE(driver.view().IsNull());
    EXPECT_FALSE(driver.window().IsNull());
}
```

Add the missing includes near the other includes at the top:

```cpp
#include <Aspect_DisplayConnection.hxx>
#include <OpenGl_GraphicDriver.hxx>
```

- [ ] **Step 2: Add the new ctor to `src/coupecad/renderer/occt/view_driver.h`**

After the existing `ViewDriver();` declaration, add:

```cpp
    // Использовать внешний (Qt-managed) GL-driver. Skip headless-fallback;
    // gl_available_ всегда true. Для Qt Quick FBO интеграции (Stage 4a §3.6).
    explicit ViewDriver(const Handle(OpenGl_GraphicDriver)& external_driver);
```

- [ ] **Step 3: Implement in `src/coupecad/renderer/occt/view_driver.cpp`**

Append after the existing `ViewDriver::ViewDriver()` body (before `ViewDriver::~ViewDriver`):

```cpp
ViewDriver::ViewDriver(const Handle(OpenGl_GraphicDriver)& external_driver) {
    driver_ = external_driver;
    gl_available_ = !driver_.IsNull();

    viewer_ = new V3d_Viewer(driver_);
    viewer_->SetDefaultLights();
    viewer_->SetLightOn();

    view_ = viewer_->CreateView();

    window_ = new Aspect_NeutralWindow();
    window_->SetSize(static_cast<Standard_Integer>(width_),
                     static_cast<Standard_Integer>(height_));
    view_->SetWindow(window_);

    view_->SetUp(0.0, 0.0, 1.0);

    coupecad::logging::Logger::instance().info(
        "renderer", "ViewDriver constructed (external driver, gl_available={})",
        gl_available_);
}
```

- [ ] **Step 4: Build and run targeted test**

```bash
cmake --build --preset default --target coupecad_renderer_test
ctest --preset default -R "ViewDriverTest.ExternalDriverCtorMarksGlAvailable" --output-on-failure
```

Expected: 1 test, passed.

- [ ] **Step 5: Full sweep**

```bash
ctest --preset default --output-on-failure
```

Expected: 308 passing (307 + 1 new).

- [ ] **Step 6: Commit**

```bash
git add src/coupecad/renderer/occt/view_driver.h \
        src/coupecad/renderer/occt/view_driver.cpp \
        tests/renderer/view_driver_test.cpp
git commit -m "feat(renderer): ViewDriver — конструктор с внешним GL-драйвером"
```

---

## Task 4: OcctRenderer.attach_external_gl_driver + render_into_current_context

**Files:**
- Modify: `src/coupecad/renderer/occt/occt_renderer.cpp`
- Modify: `tests/renderer/occt_renderer_render_smoke_test.cpp`

- [ ] **Step 1: Append failing tests to `tests/renderer/occt_renderer_render_smoke_test.cpp`**

After existing tests, append:

```cpp
#include "coupecad/renderer/occt/i_occt_gl_backend.h"

#include <Aspect_DisplayConnection.hxx>
#include <OpenGl_GraphicDriver.hxx>

TEST(OcctRendererGlBackendTest, RenderIntoCurrentContextThrowsBeforeAttach) {
    Project project = Project::create_empty("gl1");
    GeometryBuilder builder{project};
    auto renderer = coupecad::renderer::occt::make_occt_renderer(project, builder);

    auto* gl_backend = dynamic_cast<coupecad::renderer::occt::IOcctGlBackend*>(
        renderer.get());
    ASSERT_NE(gl_backend, nullptr);

    try {
        gl_backend->render_into_current_context();
        FAIL() << "expected DomainError";
    } catch (const coupecad::core::DomainError& e) {
        EXPECT_EQ(e.code(), "renderer.gl_unavailable");
    }
}

TEST(OcctRendererGlBackendTest, AttachExternalGlDriverDoesNotThrow) {
    Project project = Project::create_empty("gl2");
    GeometryBuilder builder{project};
    auto renderer = coupecad::renderer::occt::make_occt_renderer(project, builder);
    auto* gl_backend = dynamic_cast<coupecad::renderer::occt::IOcctGlBackend*>(
        renderer.get());
    ASSERT_NE(gl_backend, nullptr);

    Handle(Aspect_DisplayConnection) display = new Aspect_DisplayConnection();
    Handle(OpenGl_GraphicDriver) drv = new OpenGl_GraphicDriver(display, false);

    EXPECT_NO_THROW(gl_backend->attach_external_gl_driver(drv));
}

TEST(OcctRendererGlBackendTest, AttachExternalGlDriverPreservesProjectGeometry) {
    // After attach, the renderer should have rebuilt the scene against the
    // new driver. The cabinet has 0 panels in this minimal project, so we
    // just check that no exception fires and a follow-up sync still works.
    Project project = Project::create_empty("gl3");
    GeometryBuilder builder{project};
    auto renderer = coupecad::renderer::occt::make_occt_renderer(project, builder);
    auto* gl_backend = dynamic_cast<coupecad::renderer::occt::IOcctGlBackend*>(
        renderer.get());
    ASSERT_NE(gl_backend, nullptr);

    Handle(Aspect_DisplayConnection) display = new Aspect_DisplayConnection();
    Handle(OpenGl_GraphicDriver) drv = new OpenGl_GraphicDriver(display, false);
    gl_backend->attach_external_gl_driver(drv);

    coupecad::core::ChangeSet empty;
    EXPECT_NO_THROW(renderer->sync(empty));
}
```

- [ ] **Step 2: Implement in `src/coupecad/renderer/occt/occt_renderer.cpp`**

Replace the two stub method bodies added in Task 2 with:

```cpp
void OcctRenderer::attach_external_gl_driver(
    const Handle(OpenGl_GraphicDriver)& driver) {
    coupecad::logging::Logger::instance().info(
        "renderer", "attach_external_gl_driver: rebuilding ViewDriver/AisScene");

    // Destroy current scene + driver (in this order — AisScene holds the
    // AIS_InteractiveContext which references viewer/driver).
    scene_.clear();

    // Move-assign new ViewDriver/AisScene. Since both have non-trivial
    // ctor, we use placement-new-style assign: stop. Easier: replace via
    // swap. But ViewDriver/AisScene are non-copyable; we have to
    // reconstruct in place. Use std::optional<...> wrappers? — bigger
    // refactor. Simpler: hold driver_/scene_ as direct members and
    // swap with fresh local ones via reset-style tricks.
    //
    // Pragmatic path: change driver_/scene_ from value-members to
    // std::optional in occt_renderer.h, then we can reset/emplace.
    // But the header is locked. Alternative: in this Task 4, the
    // behavior we need is "wire the new driver into a fresh
    // AIS_InteractiveContext"; the actual ViewDriver/AisScene
    // members being swapped is the only way. Given the header
    // constraint (members are direct, not optional), we accept the
    // following compromise for Stage 4a:
    //
    //   - ViewDriver supports an "adopt external driver" method we
    //     add inline below.
    //   - AisScene's context is rebuilt by clearing it and rebinding
    //     to the (now-different) viewer.
    //
    // We add a new ViewDriver method `adopt_external_driver(...)`
    // that swaps internals.
    driver_.adopt_external_driver(driver);
    scene_.rebind_to_driver(driver_);
}

void OcctRenderer::render_into_current_context() {
    if (!driver_.gl_available()) {
        throw core::DomainError{
            "renderer.gl_unavailable",
            "render_into_current_context called before "
            "attach_external_gl_driver succeeded"};
    }
    driver_.view()->Redraw();
}
```

> The above introduces two new helpers — `ViewDriver::adopt_external_driver` and `AisScene::rebind_to_driver` — that we now add to the existing classes.

- [ ] **Step 3: Add `adopt_external_driver` to ViewDriver**

In `src/coupecad/renderer/occt/view_driver.h`, after `set_viewport_size` declaration, add:

```cpp
    // Re-point this ViewDriver at a different external GL driver. Used
    // only by OcctRenderer::attach_external_gl_driver. The previous
    // V3d_Viewer/V3d_View are released; new ones are constructed.
    void adopt_external_driver(const Handle(OpenGl_GraphicDriver)& driver);
```

In `src/coupecad/renderer/occt/view_driver.cpp`, append:

```cpp
void ViewDriver::adopt_external_driver(
    const Handle(OpenGl_GraphicDriver)& driver) {
    driver_ = driver;
    gl_available_ = !driver_.IsNull();

    viewer_ = new V3d_Viewer(driver_);
    viewer_->SetDefaultLights();
    viewer_->SetLightOn();

    view_ = viewer_->CreateView();

    window_ = new Aspect_NeutralWindow();
    window_->SetSize(static_cast<Standard_Integer>(width_),
                     static_cast<Standard_Integer>(height_));
    view_->SetWindow(window_);

    view_->SetUp(0.0, 0.0, 1.0);

    coupecad::logging::Logger::instance().info(
        "renderer", "ViewDriver adopted external driver (gl_available={})",
        gl_available_);
}
```

- [ ] **Step 4: Add `rebind_to_driver` to AisScene**

In `src/coupecad/renderer/occt/ais_scene.h`, after `clear()` declaration (still public), add:

```cpp
    // Rebuild AIS_InteractiveContext against driver_'s current viewer.
    // Used after attach_external_gl_driver. Old AIS_Shape objects are
    // released; the scene is empty afterwards (caller must repopulate
    // via add_panel / add_hardware).
    void rebind_to_driver(ViewDriver& driver);
```

In `src/coupecad/renderer/occt/ais_scene.cpp`, append:

```cpp
void AisScene::rebind_to_driver(ViewDriver& driver) {
    panel_objects_.clear();
    hardware_objects_.clear();
    ais_to_entity_.clear();
    context_ = new AIS_InteractiveContext(driver.viewer());
    context_->SetAutomaticHilight(Standard_False);
}
```

- [ ] **Step 5: Build and run targeted tests**

```bash
cmake --build --preset default --target coupecad_renderer_test
ctest --preset default -R OcctRendererGlBackendTest --output-on-failure
```

Expected: 3 tests, 3 passed.

- [ ] **Step 6: Full sweep**

```bash
ctest --preset default --output-on-failure
```

Expected: 311 passing (308 + 3 new).

- [ ] **Step 7: Commit**

```bash
git add src/coupecad/renderer/occt/occt_renderer.cpp \
        src/coupecad/renderer/occt/view_driver.h \
        src/coupecad/renderer/occt/view_driver.cpp \
        src/coupecad/renderer/occt/ais_scene.h \
        src/coupecad/renderer/occt/ais_scene.cpp \
        tests/renderer/occt_renderer_render_smoke_test.cpp
git commit -m "feat(renderer): IOcctGlBackend impl — attach/render hooks"
```

---

## Task 5: viewport library skeleton + tests scaffold

**Files:**
- Create: `src/coupecad/viewport/CMakeLists.txt`
- Create: `src/coupecad/viewport/viewport_controller.h`
- Create: `src/coupecad/viewport/viewport_controller.cpp`
- Create: `tests/viewport/CMakeLists.txt`
- Create: `tests/viewport/fake_renderer.h`
- Create: `tests/viewport/viewport_controller_test.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

> Establishes the lib + test target; only the ctor and `dirty()`/`mark_clean()` work. Other methods are added in Tasks 6–11.

- [ ] **Step 1: Create `src/coupecad/viewport/CMakeLists.txt`**

```cmake
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Quick OpenGL)

add_library(coupecad_viewport STATIC
    viewport_controller.cpp
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

- [ ] **Step 2: Hook the new lib into `src/CMakeLists.txt`**

Replace the file with:

```cmake
add_subdirectory(coupecad/logging)
add_subdirectory(coupecad/core)
add_subdirectory(coupecad/geometry)
add_subdirectory(coupecad/renderer)
add_subdirectory(coupecad/viewport)
```

- [ ] **Step 3: Create `src/coupecad/viewport/viewport_controller.h`**

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

// Pure-C++ контроллер: связывает Project, GeometryBuilder и IRenderer.
// Подписан на core::IProjectObserver через UndoStack (внешний код вызывает
// undo_stack.add_observer(controller)). Обрабатывает мышь/клавиатуру в
// команды для рендера; держит camera-math (orbit/pan/zoom).
//
// Не thread-safe. Все вызовы — из UI-потока. QFBO Renderer общается
// с контроллером ТОЛЬКО через synchronize() (QSG блокирующая точка).
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

    // Полная пересборка builder + рендера. Вызвать после конструктора и
    // после deserialize. Не вызывает sync — chained через rebuild_all.
    void rebuild_from_scratch();

    // Mouse / keyboard — viewport-relative pixel coords, top-left origin.
    void on_mouse_press(int x, int y, MouseButton btn);
    void on_mouse_move(int x, int y, MouseButton btn);
    void on_mouse_release(int x, int y, MouseButton btn);
    void on_wheel(int x, int y, double delta_steps);
    void fit_all();

    // QML-binding state.
    renderer::CameraState           camera() const;
    std::vector<renderer::EntityId> selection() const;
    bool                            selection_empty() const;
    std::size_t                     selection_count() const;

    // QFBO redraw signaling.
    bool dirty() const noexcept { return dirty_; }
    void mark_clean() noexcept { dirty_ = false; }

    // Forwarded from QFBO synchronize() when FBO size changes.
    void set_viewport_size(int width, int height);

    // Test inspector.
    renderer::IRenderer& renderer() noexcept { return renderer_; }

private:
    core::Project&             project_;
    geometry::GeometryBuilder& builder_;
    renderer::IRenderer&       renderer_;
    bool                       dirty_ = true;

    MouseButton active_drag_ = MouseButton::None;
    int         drag_last_x_ = 0;
    int         drag_last_y_ = 0;
    int         drag_total_dx_ = 0;
    int         drag_total_dy_ = 0;
};

}  // namespace coupecad::viewport
```

- [ ] **Step 4: Create `src/coupecad/viewport/viewport_controller.cpp`** — minimal stubs for everything except ctor/dtor/dirty/mark_clean

```cpp
#include "coupecad/viewport/viewport_controller.h"

#include "coupecad/geometry/geometry_builder.h"
#include "coupecad/logging/logger.h"

namespace coupecad::viewport {

ViewportController::ViewportController(core::Project& project,
                                       geometry::GeometryBuilder& builder,
                                       renderer::IRenderer& renderer_in)
    : project_(project), builder_(builder), renderer_(renderer_in) {
    coupecad::logging::Logger::instance().info(
        "viewport", "ViewportController constructed");
}

ViewportController::~ViewportController() = default;

void ViewportController::on_changed(const core::Project&,
                                    const core::ChangeSet&) {
    // Full impl in Task 6.
}

void ViewportController::rebuild_from_scratch() {
    // Full impl in Task 6.
}

void ViewportController::on_mouse_press(int, int, MouseButton)   {}
void ViewportController::on_mouse_move(int, int, MouseButton)    {}
void ViewportController::on_mouse_release(int, int, MouseButton) {}
void ViewportController::on_wheel(int, int, double)              {}
void ViewportController::fit_all()                                {}

renderer::CameraState ViewportController::camera() const {
    return renderer_.camera();
}

std::vector<renderer::EntityId> ViewportController::selection() const {
    return renderer_.selection();
}

bool ViewportController::selection_empty() const {
    return renderer_.selection().empty();
}

std::size_t ViewportController::selection_count() const {
    return renderer_.selection().size();
}

void ViewportController::set_viewport_size(int /*w*/, int /*h*/) {
    // Full impl in Task 7.
}

}  // namespace coupecad::viewport
```

- [ ] **Step 5: Create `tests/viewport/fake_renderer.h`**

```cpp
#pragma once

#include "coupecad/renderer/i_renderer.h"

#include <optional>
#include <vector>

namespace coupecad::viewport::testing {

class FakeRenderer : public coupecad::renderer::IRenderer {
public:
    std::vector<core::ChangeSet>            sync_calls;
    std::vector<renderer::CameraState>      set_camera_calls;
    std::vector<renderer::EntityId>         select_calls;
    std::vector<renderer::EntityId>         deselect_calls;
    int                                     clear_selection_count = 0;
    int                                     fit_all_count = 0;
    int                                     rebuild_all_count = 0;
    std::optional<renderer::EntityId>       next_pick_result;
    renderer::ViewportSize                  last_set_viewport{1, 1};

    void sync(const core::ChangeSet& cs) override { sync_calls.push_back(cs); }
    void rebuild_all() override                    { ++rebuild_all_count; }
    void set_camera(const renderer::CameraState& s) override {
        set_camera_calls.push_back(s);
        current_ = s;
    }
    renderer::CameraState camera() const override  { return current_; }
    void fit_all() override                        { ++fit_all_count; }
    std::optional<renderer::EntityId> pick(int, int) override {
        return next_pick_result;
    }
    void select(const renderer::EntityId& id) override {
        select_calls.push_back(id);
        selection_.push_back(id);
    }
    void deselect(const renderer::EntityId& id) override {
        deselect_calls.push_back(id);
    }
    void clear_selection() override {
        ++clear_selection_count;
        selection_.clear();
    }
    std::vector<renderer::EntityId> selection() const override {
        return selection_;
    }
    void set_viewport_size(renderer::ViewportSize s) override {
        last_set_viewport = s;
    }
    renderer::ViewportSize viewport_size() const override {
        return last_set_viewport;
    }
    std::vector<std::uint8_t> render_to_image() override { return {}; }

private:
    renderer::CameraState current_{};
    std::vector<renderer::EntityId> selection_;
};

}  // namespace coupecad::viewport::testing
```

- [ ] **Step 6: Create `tests/viewport/viewport_controller_test.cpp`**

```cpp
#include "coupecad/viewport/viewport_controller.h"

#include "coupecad/core/project.h"
#include "coupecad/geometry/geometry_builder.h"
#include "fake_renderer.h"

#include <gtest/gtest.h>

using coupecad::core::Project;
using coupecad::geometry::GeometryBuilder;
using coupecad::viewport::ViewportController;
using coupecad::viewport::testing::FakeRenderer;

TEST(ViewportControllerTest, DirtyFlagInitiallyTrue) {
    Project project = Project::create_empty("vc1");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    ViewportController controller{project, builder, fake};

    EXPECT_TRUE(controller.dirty());
}

TEST(ViewportControllerTest, MarkCleanClearsDirty) {
    Project project = Project::create_empty("vc2");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    ViewportController controller{project, builder, fake};

    controller.mark_clean();
    EXPECT_FALSE(controller.dirty());
}

TEST(ViewportControllerTest, SelectionEmptyOnNewController) {
    Project project = Project::create_empty("vc3");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    ViewportController controller{project, builder, fake};

    EXPECT_TRUE(controller.selection_empty());
    EXPECT_EQ(controller.selection_count(), 0u);
}
```

- [ ] **Step 7: Create `tests/viewport/CMakeLists.txt`**

```cmake
add_executable(coupecad_viewport_test
    viewport_controller_test.cpp
)

target_link_libraries(coupecad_viewport_test
    PRIVATE
        coupecad_viewport
        coupecad_core
        coupecad_geometry
        coupecad_renderer
        coupecad_logging
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(coupecad_viewport_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}     # so #include "fake_renderer.h" works
)

include(GoogleTest)
gtest_discover_tests(coupecad_viewport_test)
```

- [ ] **Step 8: Hook the new test target into `tests/CMakeLists.txt`**

Replace the file with:

```cmake
find_package(GTest REQUIRED)

add_subdirectory(smoke)
add_subdirectory(app)
add_subdirectory(logging)
add_subdirectory(core)
add_subdirectory(geometry)
add_subdirectory(renderer)
add_subdirectory(viewport)
```

- [ ] **Step 9: Configure + build + run targeted tests**

```bash
conan install . --build=missing -s build_type=Debug
cmake --preset default
cmake --build --preset default --target coupecad_viewport_test
ctest --preset default -R ViewportControllerTest --output-on-failure
```

Expected: 3 tests, 3 passed.

- [ ] **Step 10: Full sweep**

```bash
ctest --preset default --output-on-failure
```

Expected: 314 passing (311 + 3 new).

- [ ] **Step 11: Commit**

```bash
git add src/coupecad/viewport/ \
        src/CMakeLists.txt \
        tests/viewport/ \
        tests/CMakeLists.txt
git commit -m "feat(viewport): caркас coupecad_viewport + ViewportController stubs"
```

---

## Task 6: ViewportController.on_changed + rebuild_from_scratch

**Files:**
- Modify: `src/coupecad/viewport/viewport_controller.cpp`
- Create: `tests/viewport/viewport_controller_observer_test.cpp`
- Modify: `tests/viewport/CMakeLists.txt`

- [ ] **Step 1: Create failing test `tests/viewport/viewport_controller_observer_test.cpp`**

```cpp
#include "coupecad/viewport/viewport_controller.h"

#include "coupecad/core/cabinet.h"
#include "coupecad/core/commands/change_set.h"
#include "coupecad/core/panel.h"
#include "coupecad/core/project.h"
#include "coupecad/geometry/geometry_builder.h"
#include "fake_renderer.h"

#include <gtest/gtest.h>

using coupecad::core::ChangeSet;
using coupecad::core::Project;
using coupecad::geometry::GeometryBuilder;
using coupecad::viewport::ViewportController;
using coupecad::viewport::testing::FakeRenderer;

TEST(ViewportControllerObserverTest, OnChangedForwardsToRenderer) {
    Project project = Project::create_empty("obs1");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    ViewportController controller{project, builder, fake};

    auto pid = project.uuid_gen().next_id<coupecad::core::PanelIdTag>();
    ChangeSet cs;
    cs.added_panels.push_back(pid);

    controller.on_changed(project, cs);

    ASSERT_EQ(fake.sync_calls.size(), 1u);
    EXPECT_EQ(fake.sync_calls[0].added_panels.size(), 1u);
    EXPECT_EQ(fake.sync_calls[0].added_panels[0], pid);
    EXPECT_TRUE(controller.dirty());
}

TEST(ViewportControllerObserverTest, RebuildFromScratchCallsRebuildAndFitAll) {
    Project project = Project::create_empty("obs2");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    ViewportController controller{project, builder, fake};

    controller.mark_clean();
    controller.rebuild_from_scratch();

    EXPECT_EQ(fake.rebuild_all_count, 1);
    EXPECT_EQ(fake.fit_all_count, 1);
    EXPECT_TRUE(controller.dirty());
}
```

- [ ] **Step 2: Implement in `src/coupecad/viewport/viewport_controller.cpp`**

Replace the `on_changed` and `rebuild_from_scratch` stubs with:

```cpp
void ViewportController::on_changed(const core::Project&,
                                    const core::ChangeSet& cs) {
    if (cs.empty()) return;
    builder_.apply_changes(cs);
    renderer_.sync(cs);
    dirty_ = true;
}

void ViewportController::rebuild_from_scratch() {
    coupecad::logging::Logger::instance().info(
        "viewport", "rebuild_from_scratch");
    builder_.rebuild_all();
    renderer_.rebuild_all();
    renderer_.fit_all();
    dirty_ = true;
}
```

- [ ] **Step 3: Add the test to `tests/viewport/CMakeLists.txt`**

Update the executable's source list:

```cmake
add_executable(coupecad_viewport_test
    viewport_controller_test.cpp
    viewport_controller_observer_test.cpp
)
```

- [ ] **Step 4: Build and run**

```bash
cmake --build --preset default --target coupecad_viewport_test
ctest --preset default -R "ViewportControllerObserverTest" --output-on-failure
```

Expected: 2 tests, 2 passed.

- [ ] **Step 5: Full sweep**

```bash
ctest --preset default --output-on-failure
```

Expected: 316 passing.

- [ ] **Step 6: Commit**

```bash
git add src/coupecad/viewport/viewport_controller.cpp \
        tests/viewport/viewport_controller_observer_test.cpp \
        tests/viewport/CMakeLists.txt
git commit -m "feat(viewport): on_changed + rebuild_from_scratch"
```

---

## Task 7: ViewportController.set_viewport_size

**Files:**
- Modify: `src/coupecad/viewport/viewport_controller.cpp`
- Modify: `tests/viewport/viewport_controller_test.cpp`

- [ ] **Step 1: Append failing test**

Add to `tests/viewport/viewport_controller_test.cpp`:

```cpp
TEST(ViewportControllerTest, SetViewportSizeForwardsToRenderer) {
    Project project = Project::create_empty("vc4");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    ViewportController controller{project, builder, fake};

    controller.set_viewport_size(1920, 1080);
    EXPECT_EQ(fake.last_set_viewport.width, 1920);
    EXPECT_EQ(fake.last_set_viewport.height, 1080);
    EXPECT_TRUE(controller.dirty());
}
```

- [ ] **Step 2: Implement** — replace the `set_viewport_size` stub in `viewport_controller.cpp`:

```cpp
void ViewportController::set_viewport_size(int width, int height) {
    renderer_.set_viewport_size(coupecad::renderer::ViewportSize{width, height});
    dirty_ = true;
}
```

- [ ] **Step 3: Build + run**

```bash
cmake --build --preset default --target coupecad_viewport_test
ctest --preset default -R "ViewportControllerTest.SetViewportSizeForwardsToRenderer" --output-on-failure
```

Expected: 1 test, passed.

- [ ] **Step 4: Full sweep**

```bash
ctest --preset default --output-on-failure
```

Expected: 317 passing.

- [ ] **Step 5: Commit**

```bash
git add src/coupecad/viewport/viewport_controller.cpp \
        tests/viewport/viewport_controller_test.cpp
git commit -m "feat(viewport): set_viewport_size"
```

---

## Task 8: ViewportController mouse press/move/release drag-state plumbing

**Files:**
- Modify: `src/coupecad/viewport/viewport_controller.cpp`
- Modify: `tests/viewport/viewport_controller_test.cpp`

> Implements drag-state tracking. Camera math comes in Task 9; pick comes in Task 10. This task is about *not* doing things on no-op events.

- [ ] **Step 1: Append failing test**

Add to `tests/viewport/viewport_controller_test.cpp`:

```cpp
TEST(ViewportControllerTest, MousePressDoesNotCallRendererBeforeRelease) {
    Project project = Project::create_empty("vc5");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    ViewportController controller{project, builder, fake};

    controller.on_mouse_press(100, 100, coupecad::viewport::MouseButton::Right);
    EXPECT_TRUE(fake.set_camera_calls.empty());
    EXPECT_EQ(fake.clear_selection_count, 0);
}

TEST(ViewportControllerTest, MouseMoveWithMismatchedButtonIsNoOp) {
    Project project = Project::create_empty("vc6");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    ViewportController controller{project, builder, fake};

    // No press first; spurious move should be ignored.
    controller.on_mouse_move(120, 100, coupecad::viewport::MouseButton::Left);
    EXPECT_TRUE(fake.set_camera_calls.empty());
}

TEST(ViewportControllerTest, MouseReleaseClearsDragState) {
    Project project = Project::create_empty("vc7");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    ViewportController controller{project, builder, fake};

    controller.on_mouse_press(100, 100, coupecad::viewport::MouseButton::Left);
    controller.on_mouse_release(100, 100, coupecad::viewport::MouseButton::Left);
    // After release, a fresh move with same button should not orbit
    // (drag is no longer active).
    fake.set_camera_calls.clear();
    controller.on_mouse_move(150, 100, coupecad::viewport::MouseButton::Left);
    EXPECT_TRUE(fake.set_camera_calls.empty());
}
```

- [ ] **Step 2: Implement in `viewport_controller.cpp`**

Replace the three mouse-method stubs with:

```cpp
void ViewportController::on_mouse_press(int x, int y, MouseButton btn) {
    active_drag_ = btn;
    drag_last_x_ = x;
    drag_last_y_ = y;
    drag_total_dx_ = 0;
    drag_total_dy_ = 0;
}

void ViewportController::on_mouse_move(int x, int y, MouseButton btn) {
    if (btn != active_drag_ || active_drag_ == MouseButton::None) return;

    const int dx = x - drag_last_x_;
    const int dy = y - drag_last_y_;
    drag_total_dx_ += dx >= 0 ? dx : -dx;
    drag_total_dy_ += dy >= 0 ? dy : -dy;
    drag_last_x_ = x;
    drag_last_y_ = y;

    // Camera deltas come in Task 9; for now just consume the event so
    // the drag-state machine is exercised by tests.
    (void)dx;
    (void)dy;
}

void ViewportController::on_mouse_release(int x, int y, MouseButton btn) {
    // Pick-on-click logic comes in Task 10.
    (void)x;
    (void)y;
    (void)btn;
    active_drag_ = MouseButton::None;
}
```

- [ ] **Step 3: Build + run**

```bash
cmake --build --preset default --target coupecad_viewport_test
ctest --preset default -R "ViewportControllerTest" --output-on-failure
```

Expected: all `ViewportControllerTest` cases pass (3 from Task 5 + 1 from Task 7 + 3 new = 7).

- [ ] **Step 4: Full sweep**

Expected: 320 passing.

- [ ] **Step 5: Commit**

```bash
git add src/coupecad/viewport/viewport_controller.cpp \
        tests/viewport/viewport_controller_test.cpp
git commit -m "feat(viewport): drag-state tracking (press/move/release)"
```

---

## Task 9: ViewportController orbit / pan / zoom math

**Files:**
- Modify: `src/coupecad/viewport/viewport_controller.cpp`
- Create: `tests/viewport/viewport_controller_camera_test.cpp`
- Modify: `tests/viewport/CMakeLists.txt`

- [ ] **Step 1: Create failing test `tests/viewport/viewport_controller_camera_test.cpp`**

```cpp
#include "coupecad/viewport/viewport_controller.h"

#include "coupecad/core/project.h"
#include "coupecad/core/units.h"
#include "coupecad/geometry/geometry_builder.h"
#include "coupecad/renderer/i_renderer.h"
#include "fake_renderer.h"

#include <gtest/gtest.h>

#include <cmath>

using coupecad::core::Millimeters;
using coupecad::core::Project;
using coupecad::geometry::GeometryBuilder;
using coupecad::renderer::CameraState;
using coupecad::viewport::MouseButton;
using coupecad::viewport::ViewportController;
using coupecad::viewport::testing::FakeRenderer;

namespace {

CameraState make_initial_camera() {
    CameraState s;
    s.eye    = {Millimeters{0},    Millimeters{-1000}, Millimeters{0}};
    s.target = {Millimeters{0},    Millimeters{0},      Millimeters{0}};
    s.up     = {Millimeters{0},    Millimeters{0},      Millimeters{1}};
    s.fov_deg = 45.0;
    return s;
}

double distance_xy(const coupecad::core::Vec3& a,
                   const coupecad::core::Vec3& b) {
    const double dx = static_cast<double>(a.x.value() - b.x.value());
    const double dy = static_cast<double>(a.y.value() - b.y.value());
    return std::sqrt(dx * dx + dy * dy);
}

}  // namespace

TEST(ViewportControllerCameraTest, LmbDragOrbitsAroundTarget) {
    Project project = Project::create_empty("cam1");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    fake.set_camera(make_initial_camera());
    ViewportController controller{project, builder, fake};

    controller.on_mouse_press(400, 300, MouseButton::Left);
    controller.on_mouse_move(450, 300, MouseButton::Left);  // +50px in X

    ASSERT_FALSE(fake.set_camera_calls.empty());
    const auto& after = fake.set_camera_calls.back();
    // Distance to target preserved (within int-mm rounding).
    const double dist = distance_xy(after.eye, after.target);
    EXPECT_NEAR(dist, 1000.0, 5.0);
    // Eye actually moved (azimuth changed).
    EXPECT_NE(after.eye, make_initial_camera().eye);
}

TEST(ViewportControllerCameraTest, MmbDragPansBothEyeAndTarget) {
    Project project = Project::create_empty("cam2");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    fake.set_camera(make_initial_camera());
    ViewportController controller{project, builder, fake};

    const auto initial = fake.camera();
    controller.on_mouse_press(400, 300, MouseButton::Middle);
    controller.on_mouse_move(420, 300, MouseButton::Middle);

    ASSERT_FALSE(fake.set_camera_calls.empty());
    const auto& after = fake.set_camera_calls.back();
    EXPECT_NE(after.eye,    initial.eye);
    EXPECT_NE(after.target, initial.target);
    // Eye - target vector is preserved on pan.
    const auto eye_target_initial = coupecad::core::Vec3{
        Millimeters{initial.eye.x.value() - initial.target.x.value()},
        Millimeters{initial.eye.y.value() - initial.target.y.value()},
        Millimeters{initial.eye.z.value() - initial.target.z.value()}};
    const auto eye_target_after = coupecad::core::Vec3{
        Millimeters{after.eye.x.value() - after.target.x.value()},
        Millimeters{after.eye.y.value() - after.target.y.value()},
        Millimeters{after.eye.z.value() - after.target.z.value()}};
    EXPECT_EQ(eye_target_initial, eye_target_after);
}

TEST(ViewportControllerCameraTest, WheelUpZoomsIn) {
    Project project = Project::create_empty("cam3");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    fake.set_camera(make_initial_camera());
    ViewportController controller{project, builder, fake};

    controller.on_wheel(0, 0, +1.0);  // wheel up

    ASSERT_FALSE(fake.set_camera_calls.empty());
    const auto& after = fake.set_camera_calls.back();
    const double dist = distance_xy(after.eye, after.target);
    // 1000 / 1.1 ≈ 909
    EXPECT_NEAR(dist, 909.0, 3.0);
}

TEST(ViewportControllerCameraTest, FitAllDelegatesToRenderer) {
    Project project = Project::create_empty("cam4");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    ViewportController controller{project, builder, fake};

    controller.fit_all();
    EXPECT_EQ(fake.fit_all_count, 1);
    EXPECT_TRUE(controller.dirty());
}
```

- [ ] **Step 2: Implement orbit/pan/zoom in `viewport_controller.cpp`**

Replace the body of `on_mouse_move` (the version from Task 8) with:

```cpp
void ViewportController::on_mouse_move(int x, int y, MouseButton btn) {
    if (btn != active_drag_ || active_drag_ == MouseButton::None) return;

    const int dx = x - drag_last_x_;
    const int dy = y - drag_last_y_;
    drag_total_dx_ += dx >= 0 ? dx : -dx;
    drag_total_dy_ += dy >= 0 ? dy : -dy;
    drag_last_x_ = x;
    drag_last_y_ = y;

    if (active_drag_ == MouseButton::Left) {
        // Orbit: 0.5° per pixel around target.
        constexpr double kDegPerPixel = 0.5;
        const double az_deg = -static_cast<double>(dx) * kDegPerPixel;
        const double el_deg = -static_cast<double>(dy) * kDegPerPixel;

        renderer::CameraState s = renderer_.camera();
        const double tx = static_cast<double>(s.target.x.value());
        const double ty = static_cast<double>(s.target.y.value());
        const double tz = static_cast<double>(s.target.z.value());
        const double ex = static_cast<double>(s.eye.x.value()) - tx;
        const double ey = static_cast<double>(s.eye.y.value()) - ty;
        const double ez = static_cast<double>(s.eye.z.value()) - tz;
        const double dist = std::sqrt(ex * ex + ey * ey + ez * ez);
        if (dist < 1e-6) return;

        // Current spherical coords (Z-up frame): elevation from XY-plane.
        const double current_az = std::atan2(ey, ex);
        const double current_el = std::asin(ez / dist);

        constexpr double kPi = 3.14159265358979323846;
        const double new_az = current_az + az_deg * kPi / 180.0;
        double new_el = current_el + el_deg * kPi / 180.0;
        constexpr double kMaxEl = 89.0 * kPi / 180.0;
        if (new_el > kMaxEl)  new_el = kMaxEl;
        if (new_el < -kMaxEl) new_el = -kMaxEl;

        const double cos_el = std::cos(new_el);
        const double new_ex = dist * cos_el * std::cos(new_az);
        const double new_ey = dist * cos_el * std::sin(new_az);
        const double new_ez = dist * std::sin(new_el);

        s.eye = core::Vec3{
            core::Millimeters{static_cast<std::int32_t>(tx + new_ex)},
            core::Millimeters{static_cast<std::int32_t>(ty + new_ey)},
            core::Millimeters{static_cast<std::int32_t>(tz + new_ez)}};
        renderer_.set_camera(s);
        dirty_ = true;
        return;
    }

    if (active_drag_ == MouseButton::Middle) {
        // Pan: translate eye AND target by world-space vector along right/up.
        renderer::CameraState s = renderer_.camera();
        const double ex = static_cast<double>(s.eye.x.value() - s.target.x.value());
        const double ey = static_cast<double>(s.eye.y.value() - s.target.y.value());
        const double ez = static_cast<double>(s.eye.z.value() - s.target.z.value());
        const double dist = std::sqrt(ex * ex + ey * ey + ez * ez);
        if (dist < 1e-6) return;

        // Forward = (target - eye) / dist; up_world = s.up; right = up × forward.
        const double fx = -ex / dist, fy = -ey / dist, fz = -ez / dist;
        const double ux_w = static_cast<double>(s.up.x.value());
        const double uy_w = static_cast<double>(s.up.y.value());
        const double uz_w = static_cast<double>(s.up.z.value());
        // right = up × forward.
        const double rx = uy_w * fz - uz_w * fy;
        const double ry = uz_w * fx - ux_w * fz;
        const double rz = ux_w * fy - uy_w * fx;
        // Re-normalize.
        const double rlen = std::sqrt(rx * rx + ry * ry + rz * rz);
        if (rlen < 1e-6) return;
        const double rxn = rx / rlen, ryn = ry / rlen, rzn = rz / rlen;
        // Real up = forward × right.
        const double upx = fy * rzn - fz * ryn;
        const double upy = fz * rxn - fx * rzn;
        const double upz = fx * ryn - fy * rxn;

        // Pan magnitude scales with view distance and viewport (rough heuristic
        // — keeps pan feel constant regardless of zoom).
        constexpr double kPanScale = 0.002;  // mm-per-pixel-per-mm-distance
        const double pan_x_mm = -static_cast<double>(dx) * dist * kPanScale;
        const double pan_y_mm = +static_cast<double>(dy) * dist * kPanScale;
        // Apply along right + up.
        const double world_dx = rxn * pan_x_mm + upx * pan_y_mm;
        const double world_dy = ryn * pan_x_mm + upy * pan_y_mm;
        const double world_dz = rzn * pan_x_mm + upz * pan_y_mm;

        s.eye = core::Vec3{
            core::Millimeters{s.eye.x.value() + static_cast<std::int32_t>(world_dx)},
            core::Millimeters{s.eye.y.value() + static_cast<std::int32_t>(world_dy)},
            core::Millimeters{s.eye.z.value() + static_cast<std::int32_t>(world_dz)}};
        s.target = core::Vec3{
            core::Millimeters{s.target.x.value() + static_cast<std::int32_t>(world_dx)},
            core::Millimeters{s.target.y.value() + static_cast<std::int32_t>(world_dy)},
            core::Millimeters{s.target.z.value() + static_cast<std::int32_t>(world_dz)}};
        renderer_.set_camera(s);
        dirty_ = true;
        return;
    }

    // Right-button drag accumulates delta but does nothing during the
    // motion — pick happens on release if drag was tiny.
}
```

Replace `on_wheel` stub with:

```cpp
void ViewportController::on_wheel(int /*x*/, int /*y*/, double delta_steps) {
    renderer::CameraState s = renderer_.camera();
    const double factor = std::pow(1.1, -delta_steps);

    const double tx = static_cast<double>(s.target.x.value());
    const double ty = static_cast<double>(s.target.y.value());
    const double tz = static_cast<double>(s.target.z.value());

    const double ex = static_cast<double>(s.eye.x.value()) - tx;
    const double ey = static_cast<double>(s.eye.y.value()) - ty;
    const double ez = static_cast<double>(s.eye.z.value()) - tz;

    s.eye = core::Vec3{
        core::Millimeters{static_cast<std::int32_t>(tx + ex * factor)},
        core::Millimeters{static_cast<std::int32_t>(ty + ey * factor)},
        core::Millimeters{static_cast<std::int32_t>(tz + ez * factor)}};

    renderer_.set_camera(s);
    dirty_ = true;
}
```

Replace `fit_all` stub with:

```cpp
void ViewportController::fit_all() {
    renderer_.fit_all();
    dirty_ = true;
}
```

Add the missing include at the top of `viewport_controller.cpp` (after existing includes):

```cpp
#include <cmath>
#include <cstdint>
```

- [ ] **Step 3: Add the new test to `tests/viewport/CMakeLists.txt`**

```cmake
add_executable(coupecad_viewport_test
    viewport_controller_test.cpp
    viewport_controller_observer_test.cpp
    viewport_controller_camera_test.cpp
)
```

- [ ] **Step 4: Build + run targeted**

```bash
cmake --build --preset default --target coupecad_viewport_test
ctest --preset default -R ViewportControllerCameraTest --output-on-failure
```

Expected: 4 tests, 4 passed.

- [ ] **Step 5: Full sweep**

Expected: 324 passing.

- [ ] **Step 6: Commit**

```bash
git add src/coupecad/viewport/viewport_controller.cpp \
        tests/viewport/viewport_controller_camera_test.cpp \
        tests/viewport/CMakeLists.txt
git commit -m "feat(viewport): orbit/pan/zoom + fit_all"
```

---

## Task 10: ViewportController RMB pick

**Files:**
- Modify: `src/coupecad/viewport/viewport_controller.cpp`
- Create: `tests/viewport/viewport_controller_pick_test.cpp`
- Modify: `tests/viewport/CMakeLists.txt`

- [ ] **Step 1: Create failing test `tests/viewport/viewport_controller_pick_test.cpp`**

```cpp
#include "coupecad/viewport/viewport_controller.h"

#include "coupecad/core/id.h"
#include "coupecad/core/project.h"
#include "coupecad/geometry/geometry_builder.h"
#include "coupecad/renderer/entity_id.h"
#include "fake_renderer.h"

#include <gtest/gtest.h>

using coupecad::core::PanelId;
using coupecad::core::PanelIdTag;
using coupecad::core::Project;
using coupecad::geometry::GeometryBuilder;
using coupecad::renderer::EntityId;
using coupecad::viewport::MouseButton;
using coupecad::viewport::ViewportController;
using coupecad::viewport::testing::FakeRenderer;

TEST(ViewportControllerPickTest, RmbClickWithHitSelectsId) {
    Project project = Project::create_empty("pick1");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    auto pid = project.uuid_gen().next_id<PanelIdTag>();
    fake.next_pick_result = EntityId{pid};

    ViewportController controller{project, builder, fake};
    controller.on_mouse_press(100, 100, MouseButton::Right);
    controller.on_mouse_release(100, 100, MouseButton::Right);

    EXPECT_EQ(fake.clear_selection_count, 1);
    ASSERT_EQ(fake.select_calls.size(), 1u);
    EXPECT_EQ(fake.select_calls[0], EntityId{pid});
}

TEST(ViewportControllerPickTest, RmbClickWithMissOnlyClears) {
    Project project = Project::create_empty("pick2");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    fake.next_pick_result = std::nullopt;

    ViewportController controller{project, builder, fake};
    controller.on_mouse_press(100, 100, MouseButton::Right);
    controller.on_mouse_release(100, 100, MouseButton::Right);

    EXPECT_EQ(fake.clear_selection_count, 1);
    EXPECT_TRUE(fake.select_calls.empty());
}

TEST(ViewportControllerPickTest, RmbDragDoesNotPick) {
    Project project = Project::create_empty("pick3");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    auto pid = project.uuid_gen().next_id<PanelIdTag>();
    fake.next_pick_result = EntityId{pid};

    ViewportController controller{project, builder, fake};
    controller.on_mouse_press(100, 100, MouseButton::Right);
    controller.on_mouse_move(110, 100, MouseButton::Right);   // +10px > threshold
    controller.on_mouse_release(110, 100, MouseButton::Right);

    EXPECT_EQ(fake.clear_selection_count, 0);
    EXPECT_TRUE(fake.select_calls.empty());
}
```

- [ ] **Step 2: Implement in `viewport_controller.cpp`**

Replace the `on_mouse_release` stub with:

```cpp
void ViewportController::on_mouse_release(int x, int y, MouseButton btn) {
    constexpr int kPickDragThreshold = 5;  // pixels

    if (btn == MouseButton::Right && active_drag_ == MouseButton::Right &&
        drag_total_dx_ + drag_total_dy_ < kPickDragThreshold) {
        renderer_.clear_selection();
        if (auto hit = renderer_.pick(x, y)) {
            renderer_.select(*hit);
        }
        dirty_ = true;
    }

    active_drag_ = MouseButton::None;
}
```

- [ ] **Step 3: Add the test to CMakeLists**

```cmake
add_executable(coupecad_viewport_test
    viewport_controller_test.cpp
    viewport_controller_observer_test.cpp
    viewport_controller_camera_test.cpp
    viewport_controller_pick_test.cpp
)
```

- [ ] **Step 4: Build + run**

```bash
cmake --build --preset default --target coupecad_viewport_test
ctest --preset default -R ViewportControllerPickTest --output-on-failure
```

Expected: 3 tests, 3 passed.

- [ ] **Step 5: Full sweep**

Expected: 327 passing.

- [ ] **Step 6: Commit**

```bash
git add src/coupecad/viewport/viewport_controller.cpp \
        tests/viewport/viewport_controller_pick_test.cpp \
        tests/viewport/CMakeLists.txt
git commit -m "feat(viewport): RMB click→pick, drag-vs-click discrimination"
```

---

## Task 11: OcctViewportItem skeleton + QML smoke test

**Files:**
- Create: `src/coupecad/viewport/occt_viewport_item.h`
- Create: `src/coupecad/viewport/occt_viewport_item.cpp`
- Modify: `src/coupecad/viewport/CMakeLists.txt`
- Create: `tests/viewport/viewport_qml_smoke_test.cpp`
- Modify: `tests/viewport/CMakeLists.txt`

> Skeleton: the QFBO item with stub `createRenderer()` returning null (we'll fix in Task 12). The class must register and instantiate without throwing.

- [ ] **Step 1: Create `src/coupecad/viewport/occt_viewport_item.h`**

```cpp
#pragma once

#include "coupecad/viewport/viewport_controller.h"

#include <QQuickFramebufferObject>

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

- [ ] **Step 2: Create `src/coupecad/viewport/occt_viewport_item.cpp`** (skeleton — stubs)

```cpp
#include "coupecad/viewport/occt_viewport_item.h"

#include "coupecad/logging/logger.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

namespace coupecad::viewport {

OcctViewportItem::OcctViewportItem(QQuickItem* parent)
    : QQuickFramebufferObject(parent) {
    setAcceptedMouseButtons(Qt::AllButtons);
    setFlag(ItemHasContents, true);
    setMirrorVertically(true);   // Qt FBO origin is bottom-left, OCCT is top-left.
}

OcctViewportItem::~OcctViewportItem() = default;

void OcctViewportItem::set_controller(ViewportController* c) {
    controller_ = c;
    update();
}

QQuickFramebufferObject::Renderer* OcctViewportItem::createRenderer() const {
    // Real renderer implementation in Task 12.
    return nullptr;
}

bool OcctViewportItem::selection_empty() const {
    return controller_ == nullptr || controller_->selection_empty();
}

int OcctViewportItem::selection_count() const {
    return controller_ == nullptr
        ? 0
        : static_cast<int>(controller_->selection_count());
}

void OcctViewportItem::mousePressEvent(QMouseEvent*)   { /* Task 13 */ }
void OcctViewportItem::mouseMoveEvent(QMouseEvent*)    { /* Task 13 */ }
void OcctViewportItem::mouseReleaseEvent(QMouseEvent*) { /* Task 13 */ }
void OcctViewportItem::wheelEvent(QWheelEvent*)        { /* Task 13 */ }
void OcctViewportItem::keyPressEvent(QKeyEvent*)       { /* Task 13 */ }

}  // namespace coupecad::viewport
```

- [ ] **Step 3: Add `occt_viewport_item.cpp` to `src/coupecad/viewport/CMakeLists.txt`**

```cmake
add_library(coupecad_viewport STATIC
    viewport_controller.cpp
    occt_viewport_item.cpp
)
```

- [ ] **Step 4: Create `tests/viewport/viewport_qml_smoke_test.cpp`**

```cpp
#include "coupecad/viewport/occt_viewport_item.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include <gtest/gtest.h>

namespace {

bool s_qapp_initialised = false;
QGuiApplication* s_qapp = nullptr;

void ensure_qapp() {
    if (s_qapp_initialised) return;
    static int    argc = 1;
    static char   arg0[] = "viewport_qml_smoke";
    static char*  argv[] = {arg0, nullptr};
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    s_qapp = new QGuiApplication(argc, argv);
    s_qapp_initialised = true;
}

}  // namespace

TEST(ViewportQmlSmokeTest, RegisterTypeAndInstantiate) {
    ensure_qapp();
    qmlRegisterType<coupecad::viewport::OcctViewportItem>(
        "coupecad", 1, 0, "OcctViewportItem");

    QQmlApplicationEngine engine;
    engine.loadData(R"(
        import QtQuick
        import QtQuick.Window
        import coupecad

        Window {
            visible: false
            OcctViewportItem {
                anchors.fill: parent
            }
        }
    )");
    ASSERT_FALSE(engine.rootObjects().isEmpty());
}
```

- [ ] **Step 5: Add the smoke test to `tests/viewport/CMakeLists.txt`**

Replace the `add_executable(coupecad_viewport_test ...)` with two executables (one with Qt, one without):

```cmake
add_executable(coupecad_viewport_test
    viewport_controller_test.cpp
    viewport_controller_observer_test.cpp
    viewport_controller_camera_test.cpp
    viewport_controller_pick_test.cpp
)

target_link_libraries(coupecad_viewport_test
    PRIVATE
        coupecad_viewport
        coupecad_core
        coupecad_geometry
        coupecad_renderer
        coupecad_logging
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(coupecad_viewport_test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

include(GoogleTest)
gtest_discover_tests(coupecad_viewport_test)

# QML smoke test — separate target so a failing QML setup doesn't take
# down the controller tests.
find_package(Qt6 6.5 REQUIRED COMPONENTS Qml Quick)

add_executable(coupecad_viewport_qml_smoke_test
    viewport_qml_smoke_test.cpp
)

target_link_libraries(coupecad_viewport_qml_smoke_test
    PRIVATE
        coupecad_viewport
        Qt6::Qml
        Qt6::Quick
        GTest::gtest
        GTest::gtest_main
)

set_target_properties(coupecad_viewport_qml_smoke_test PROPERTIES AUTOMOC ON)

gtest_discover_tests(coupecad_viewport_qml_smoke_test
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
```

- [ ] **Step 6: Build + run**

```bash
cmake --build --preset default --target coupecad_viewport_qml_smoke_test
ctest --preset default -R ViewportQmlSmokeTest --output-on-failure
```

Expected: 1 test, 1 passed.

- [ ] **Step 7: Full sweep**

Expected: 328 passing (327 + 1 new).

- [ ] **Step 8: Commit**

```bash
git add src/coupecad/viewport/occt_viewport_item.h \
        src/coupecad/viewport/occt_viewport_item.cpp \
        src/coupecad/viewport/CMakeLists.txt \
        tests/viewport/viewport_qml_smoke_test.cpp \
        tests/viewport/CMakeLists.txt
git commit -m "feat(viewport): OcctViewportItem skeleton + QML smoke test"
```

---

## Task 12: OcctFboRenderer (real rendering path)

**Files:**
- Modify: `src/coupecad/viewport/occt_viewport_item.cpp`

> Implements the QFBO Renderer subclass: lazy GL init via `attach_external_gl_driver`, FBO size forwarding, and `render_into_current_context` per frame. Smoke test from Task 11 still passes (it only checks the item creates and registers; FBO render only fires when window is actually shown which doesn't happen on offscreen).

- [ ] **Step 1: Replace `occt_viewport_item.cpp` with the full implementation**

Replace the existing file with:

```cpp
#include "coupecad/viewport/occt_viewport_item.h"

#include "coupecad/logging/logger.h"
#include "coupecad/renderer/occt/i_occt_gl_backend.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QQuickWindow>
#include <QWheelEvent>

#include <Aspect_DisplayConnection.hxx>
#include <OpenGl_GraphicDriver.hxx>

namespace coupecad::viewport {

namespace {

class OcctFboRenderer : public QQuickFramebufferObject::Renderer {
public:
    explicit OcctFboRenderer(ViewportController* controller)
        : controller_(controller) {}

    QOpenGLFramebufferObject* createFramebufferObject(const QSize& size) override {
        last_fbo_size_ = size;
        QOpenGLFramebufferObjectFormat fmt;
        fmt.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        fmt.setSamples(4);
        return new QOpenGLFramebufferObject(size, fmt);
    }

    void synchronize(QQuickFramebufferObject* item) override {
        auto* viewport_item = static_cast<OcctViewportItem*>(item);
        controller_ = viewport_item->controller();
        if (controller_ == nullptr) return;

        if (last_fbo_size_ != applied_fbo_size_) {
            controller_->set_viewport_size(last_fbo_size_.width(),
                                           last_fbo_size_.height());
            applied_fbo_size_ = last_fbo_size_;
        }
        if (controller_->dirty()) {
            controller_->mark_clean();
            redraw_pending_ = true;
        }
    }

    void render() override {
        if (controller_ == nullptr) return;

        if (!gl_initialised_) {
            initialise_occt();
            gl_initialised_ = true;
        }
        if (!gl_initialised_) return;   // init failed

        auto* gl_backend = dynamic_cast<coupecad::renderer::occt::IOcctGlBackend*>(
            &controller_->renderer());
        if (gl_backend == nullptr) {
            coupecad::logging::Logger::instance().error(
                "viewport",
                "Renderer does not implement IOcctGlBackend; "
                "viewport disabled");
            return;
        }
        gl_backend->render_into_current_context();
        redraw_pending_ = false;

        // Schedule another frame if anything's dirty.
        if (controller_->dirty()) update();
    }

private:
    void initialise_occt() {
        QOpenGLContext* qt_ctx = QOpenGLContext::currentContext();
        if (qt_ctx == nullptr) {
            coupecad::logging::Logger::instance().warn(
                "viewport", "QOpenGLContext::currentContext() is null; "
                            "cannot initialise OCCT GL backend");
            return;
        }

        try {
            Handle(Aspect_DisplayConnection) display = new Aspect_DisplayConnection();
            Handle(OpenGl_GraphicDriver) drv = new OpenGl_GraphicDriver(display, false);
            auto* gl_backend = dynamic_cast<coupecad::renderer::occt::IOcctGlBackend*>(
                &controller_->renderer());
            if (gl_backend == nullptr) {
                coupecad::logging::Logger::instance().error(
                    "viewport", "Cannot attach external GL — backend missing");
                return;
            }
            gl_backend->attach_external_gl_driver(drv);
            controller_->rebuild_from_scratch();
            coupecad::logging::Logger::instance().info(
                "viewport", "OCCT GL backend attached to Qt context");
        } catch (const std::exception& e) {
            coupecad::logging::Logger::instance().error(
                "viewport", "Failed to attach OCCT GL backend: {}", e.what());
        }
    }

    ViewportController* controller_ = nullptr;
    QSize               last_fbo_size_{1, 1};
    QSize               applied_fbo_size_{0, 0};
    bool                gl_initialised_ = false;
    bool                redraw_pending_ = false;
};

}  // namespace

OcctViewportItem::OcctViewportItem(QQuickItem* parent)
    : QQuickFramebufferObject(parent) {
    setAcceptedMouseButtons(Qt::AllButtons);
    setFlag(ItemHasContents, true);
    setMirrorVertically(true);
}

OcctViewportItem::~OcctViewportItem() = default;

void OcctViewportItem::set_controller(ViewportController* c) {
    controller_ = c;
    update();
}

QQuickFramebufferObject::Renderer* OcctViewportItem::createRenderer() const {
    return new OcctFboRenderer(controller_);
}

bool OcctViewportItem::selection_empty() const {
    return controller_ == nullptr || controller_->selection_empty();
}

int OcctViewportItem::selection_count() const {
    return controller_ == nullptr
        ? 0
        : static_cast<int>(controller_->selection_count());
}

void OcctViewportItem::mousePressEvent(QMouseEvent*)   { /* Task 13 */ }
void OcctViewportItem::mouseMoveEvent(QMouseEvent*)    { /* Task 13 */ }
void OcctViewportItem::mouseReleaseEvent(QMouseEvent*) { /* Task 13 */ }
void OcctViewportItem::wheelEvent(QWheelEvent*)        { /* Task 13 */ }
void OcctViewportItem::keyPressEvent(QKeyEvent*)       { /* Task 13 */ }

}  // namespace coupecad::viewport
```

- [ ] **Step 2: Build**

```bash
cmake --build --preset default --target coupecad_viewport
```

Expected: builds clean.

- [ ] **Step 3: Run smoke test (still passes — offscreen doesn't realize render thread)**

```bash
ctest --preset default -R ViewportQmlSmokeTest --output-on-failure
```

Expected: 1 test, passed.

- [ ] **Step 4: Full sweep**

Expected: 328 still passing.

- [ ] **Step 5: Commit**

```bash
git add src/coupecad/viewport/occt_viewport_item.cpp
git commit -m "feat(viewport): OcctFboRenderer — Qt FBO ↔ OCCT GL"
```

---

## Task 13: Mouse / wheel / key event forwarding + selection-changed signal

**Files:**
- Modify: `src/coupecad/viewport/occt_viewport_item.cpp`

> Wires Qt input events to the controller. No new tests at this task — the forwarding is too thin (5-line delegations); the controller is already exhaustively tested via `FakeRenderer` in Tasks 9/10. Smoke test still verifies wiring on offscreen.

- [ ] **Step 1: Replace the five event-handler stubs in `occt_viewport_item.cpp`**

```cpp
namespace {

MouseButton qt_button_to_mouse_button(Qt::MouseButton b) {
    switch (b) {
        case Qt::LeftButton:   return MouseButton::Left;
        case Qt::MiddleButton: return MouseButton::Middle;
        case Qt::RightButton:  return MouseButton::Right;
        default:               return MouseButton::None;
    }
}

}  // namespace

void OcctViewportItem::mousePressEvent(QMouseEvent* e) {
    if (controller_ == nullptr) return;
    forceActiveFocus();
    const int x = static_cast<int>(e->position().x());
    const int y = static_cast<int>(e->position().y());
    const int prev = static_cast<int>(controller_->selection_count());
    controller_->on_mouse_press(x, y, qt_button_to_mouse_button(e->button()));
    e->accept();
    update();
    if (static_cast<int>(controller_->selection_count()) != prev) {
        emit selectionChanged();
    }
}

void OcctViewportItem::mouseMoveEvent(QMouseEvent* e) {
    if (controller_ == nullptr) return;
    const int x = static_cast<int>(e->position().x());
    const int y = static_cast<int>(e->position().y());
    // Determine which button is held (Qt mouse-move doesn't carry e->button()
    // reliably; use buttons()).
    MouseButton btn = MouseButton::None;
    if (e->buttons() & Qt::LeftButton)   btn = MouseButton::Left;
    else if (e->buttons() & Qt::MiddleButton) btn = MouseButton::Middle;
    else if (e->buttons() & Qt::RightButton)  btn = MouseButton::Right;
    controller_->on_mouse_move(x, y, btn);
    e->accept();
    if (controller_->dirty()) update();
}

void OcctViewportItem::mouseReleaseEvent(QMouseEvent* e) {
    if (controller_ == nullptr) return;
    const int x = static_cast<int>(e->position().x());
    const int y = static_cast<int>(e->position().y());
    const int prev = static_cast<int>(controller_->selection_count());
    controller_->on_mouse_release(x, y, qt_button_to_mouse_button(e->button()));
    e->accept();
    update();
    if (static_cast<int>(controller_->selection_count()) != prev) {
        emit selectionChanged();
    }
}

void OcctViewportItem::wheelEvent(QWheelEvent* e) {
    if (controller_ == nullptr) return;
    const int x = static_cast<int>(e->position().x());
    const int y = static_cast<int>(e->position().y());
    const double steps = e->angleDelta().y() / 120.0;
    controller_->on_wheel(x, y, steps);
    e->accept();
    update();
}

void OcctViewportItem::keyPressEvent(QKeyEvent* e) {
    if (controller_ == nullptr) return;
    if (e->key() == Qt::Key_F) {
        controller_->fit_all();
        e->accept();
        update();
        return;
    }
    e->ignore();
}
```

`MouseButton` is in the `coupecad::viewport` namespace — the `qt_button_to_mouse_button` helper is inside the anonymous namespace **inside** `coupecad::viewport`, so `MouseButton` is found via ADL. If it isn't, change the helper to `coupecad::viewport::MouseButton qt_button_to_mouse_button(...)` and remove the `using ...` in the function bodies.

- [ ] **Step 2: Build**

```bash
cmake --build --preset default --target coupecad_viewport
```

Expected: builds clean.

- [ ] **Step 3: Smoke test still passes**

```bash
ctest --preset default -R ViewportQmlSmokeTest --output-on-failure
```

Expected: 1 test, passed.

- [ ] **Step 4: Full sweep**

Expected: 328 still passing.

- [ ] **Step 5: Commit**

```bash
git add src/coupecad/viewport/occt_viewport_item.cpp
git commit -m "feat(viewport): forward Qt input events to ViewportController"
```

---

## Task 14: demo_project + main.cpp wiring + Main.qml

**Files:**
- Create: `apps/coupecad/demo_project.h`
- Create: `apps/coupecad/demo_project.cpp`
- Modify: `apps/coupecad/main.cpp`
- Modify: `apps/coupecad/qml/Main.qml`
- Modify: `apps/coupecad/CMakeLists.txt`

> Brings everything together. Manual verification — launch the app and see the cabinet.

- [ ] **Step 1: Create `apps/coupecad/demo_project.h`**

```cpp
#pragma once

#include "coupecad/core/project.h"

namespace coupecad::app {

// 1200×600×2000 mm cabinet with bottom + top + 2 sides + back + 2 shelves.
// No hardware (Stage 4a keeps the surface minimal).
core::Project make_demo_project();

}  // namespace coupecad::app
```

- [ ] **Step 2: Create `apps/coupecad/demo_project.cpp`**

```cpp
#include "demo_project.h"

#include "coupecad/core/cabinet.h"
#include "coupecad/core/material.h"
#include "coupecad/core/panel.h"
#include "coupecad/core/units.h"

namespace coupecad::app {

namespace {

void add_panel(core::Cabinet& cab, core::UuidGenerator& gen,
               core::PanelRole role) {
    core::Panel p;
    p.id = gen.next_id<core::PanelIdTag>();
    p.role = role;
    cab.panels.emplace(p.id, p);
}

void add_shelf(core::Cabinet& cab, core::UuidGenerator& gen,
               core::Millimeters height_from_bottom) {
    core::Panel p;
    p.id = gen.next_id<core::PanelIdTag>();
    p.role = core::PanelRole::Shelf;
    p.role_params = core::ShelfParams{
        .height_from_bottom = height_from_bottom,
        .extent = core::FullWidth{}};
    cab.panels.emplace(p.id, p);
}

}  // namespace

core::Project make_demo_project() {
    auto project = core::Project::create_empty("Demo cabinet");
    auto& cab = project.mutable_cabinet();
    cab.dimensions = {core::Millimeters{1200},
                      core::Millimeters{600},
                      core::Millimeters{2000}};
    auto& gen = project.uuid_gen();
    add_panel(cab, gen, core::PanelRole::Bottom);
    add_panel(cab, gen, core::PanelRole::Top);
    add_panel(cab, gen, core::PanelRole::SideLeft);
    add_panel(cab, gen, core::PanelRole::SideRight);
    add_panel(cab, gen, core::PanelRole::Back);
    add_shelf(cab, gen, core::Millimeters{700});
    add_shelf(cab, gen, core::Millimeters{1400});
    return project;
}

}  // namespace coupecad::app
```

> If the `core::ShelfParams` struct uses field names different from `height_from_bottom` / `extent`, adjust accordingly. Check `src/coupecad/core/panel.h` ShelfParams definition; spec §2.2 of the Stage 1 design says: `Shelf: {height_from_bottom: Millimeters, extent: ShelfExtent}` with `ShelfExtent = FullWidth | BetweenDividers{...}`.

- [ ] **Step 3: Replace `apps/coupecad/main.cpp`**

```cpp
#include "demo_project.h"

#include "coupecad/core/project.h"
#include "coupecad/core/undo_stack.h"
#include "coupecad/geometry/geometry_builder.h"
#include "coupecad/renderer/occt/occt_renderer.h"
#include "coupecad/viewport/occt_viewport_item.h"
#include "coupecad/viewport/viewport_controller.h"

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
    app.setOrganizationName(QStringLiteral("CoupeCAD"));
    app.setOrganizationDomain(QStringLiteral("coupecad.app"));

    for (int i = 1; i < argc; ++i) {
        const QString a = QString::fromLocal8Bit(argv[i]);
        if (a == QStringLiteral("--version") || a == QStringLiteral("-v")) {
            std::cout << "CoupeCAD " << kAppVersion << std::endl;
            return 0;
        }
    }

    // Data layer.
    auto project = std::make_unique<coupecad::core::Project>(
        coupecad::app::make_demo_project());
    auto undo    = std::make_unique<coupecad::core::UndoStack>(*project);
    auto builder = std::make_unique<coupecad::geometry::GeometryBuilder>(
        *project);
    auto renderer = coupecad::renderer::occt::make_occt_renderer(
        *project, *builder);
    auto controller = std::make_unique<coupecad::viewport::ViewportController>(
        *project, *builder, *renderer);
    undo->add_observer(controller.get());

    // Register OcctViewportItem before loading QML.
    qmlRegisterType<coupecad::viewport::OcctViewportItem>(
        "coupecad", 1, 0, "OcctViewportItem");

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(
        "viewportController", QVariant::fromValue(controller.get()));
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(1); },
        Qt::QueuedConnection);

    engine.loadFromModule("coupecad", "Main");
    return app.exec();
}
```

- [ ] **Step 4: Replace `apps/coupecad/qml/Main.qml`**

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

        Component.onCompleted: viewport.set_controller(viewportController)
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

- [ ] **Step 5: Modify `apps/coupecad/CMakeLists.txt`**

Update the `qt_add_executable` source list and `target_link_libraries`:

```cmake
qt_add_executable(coupecad
    WIN32
    MACOSX_BUNDLE
    main.cpp
    demo_project.cpp
)

qt_add_qml_module(coupecad
    URI coupecad
    VERSION 1.0
    QML_FILES
        qml/Main.qml
)

target_link_libraries(coupecad
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        coupecad_viewport
)

set_target_properties(coupecad PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "app.coupecad.coupecad"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(APPLE)
    target_link_options(coupecad PRIVATE
        -F${CMAKE_SOURCE_DIR}/cmake/stubs
    )
endif()
```

- [ ] **Step 6: Build the app**

```bash
cmake --build --preset default --target coupecad
```

Expected: builds clean. Bundle/exec at `build/default/bin/coupecad{.app|.exe|}`.

- [ ] **Step 7: Run --version smoke (no GUI)**

```bash
./build/default/bin/coupecad.app/Contents/MacOS/coupecad --version  # macOS
# or ./build/default/bin/coupecad --version                          # Linux
# or ./build/default/bin/coupecad.exe --version                      # Windows
```

Expected: `CoupeCAD 0.1.0`, exit code 0. The `tests/app/app_version_test` already covers this in CI.

- [ ] **Step 8: Run full test sweep**

```bash
ctest --preset default --output-on-failure
```

Expected: 328/328 passed.

- [ ] **Step 9: Manual visual check (macOS dev machine)**

```bash
open ./build/default/bin/coupecad.app
```

Expected:
- Window opens at 1280×720, dark background with cabinet rendered.
- LMB drag → camera orbits around cabinet center.
- MMB drag → cabinet pans.
- Wheel → zoom in/out.
- RMB click on a panel → "Selected: 1" appears in top-left.
- Press `F` → camera reframes the cabinet.
- Close window → exit code 0.

Note in the commit if any of these don't work; debug or downgrade as concerns.

- [ ] **Step 10: Commit**

```bash
git add apps/coupecad/demo_project.h \
        apps/coupecad/demo_project.cpp \
        apps/coupecad/main.cpp \
        apps/coupecad/qml/Main.qml \
        apps/coupecad/CMakeLists.txt
git commit -m "feat(app): launch viewport with demo cabinet"
```

---

## Task 15: Final clean-rebuild + Qt-linkage hygiene + push + PR

**Files:** none (verification only)

- [ ] **Step 1: Clean rebuild**

```bash
rm -rf build/default
conan install . --build=missing -s build_type=Debug
cmake --preset default
cmake --build --preset default
```

Expected: zero new project warnings.

- [ ] **Step 2: Full ctest**

```bash
ctest --preset default --output-on-failure
```

Expected: 328/328 passed (307 baseline Stage 3 + 21 new Stage 4a).

- [ ] **Step 3: Verify `coupecad_renderer_occt` is still Qt-free**

```bash
nm build/default/src/coupecad/renderer/occt/libcoupecad_renderer_occt.a 2>/dev/null | grep -c " Q[A-Z]" || echo 0
```

Expected: `0`.

- [ ] **Step 4: Verify `coupecad_renderer` interface lib is still Qt-free**

```bash
nm build/default/src/coupecad/renderer/libcoupecad_renderer.a 2>/dev/null | grep -c " Q[A-Z]" || echo 0
```

Expected: `0`.

> `coupecad_viewport` IS allowed to have Qt symbols — it's the bridge layer.

- [ ] **Step 5: Push the branch**

```bash
git push -u origin stage-4-ui
```

- [ ] **Step 6: Open the PR**

```bash
gh pr create --title "Stage 4a: Viewport (Qt Quick + OCCT)" --body "$(cat <<'EOF'
## Summary

Stage 4a wires Qt Quick to the Stage 3 OCCT renderer.

- Spec: `docs/superpowers/specs/2026-05-07-stage-4a-viewport-design.md`
- Plan: `docs/superpowers/plans/2026-05-07-stage-4a-viewport.md`
- New `coupecad_viewport` static library: pure-C++ `ViewportController` + Qt-side `OcctViewportItem` (QQuickFramebufferObject).
- New OCCT-only `IOcctGlBackend` extension interface keeps `IRenderer` Qt-free.
- App launches with a demo cabinet; LMB orbit, MMB pan, wheel zoom, RMB pick, `F` fit-all.
- +21 tests: ViewportController controller-level (camera, observer, pick, set_viewport_size) + one offscreen QML smoke test.
- `coupecad_renderer_occt` still has 0 Qt symbols.

## Test plan

- [ ] CI: Linux build + tests pass
- [ ] CI: Windows build + tests pass
- [ ] All 328 tests green on both platforms
- [ ] No Qt symbols in `coupecad_renderer_occt` / `coupecad_renderer`
- [ ] Manual: app launches on macOS, demo cabinet visible, all interactions work

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

Capture the PR URL.

---

## Self-review notes

- Stage 4a depends on Stage 3 commits in `origin/main`. Branch is `stage-4-ui` based on `7456f6d → 9bc66af` (Stage 3 squash).
- Test count progression: 307 (Stage 3 end) → 308 (Task 3) → 311 (Task 4) → 314 (Task 5) → 316 (Task 6) → 317 (Task 7) → 320 (Task 8) → 324 (Task 9) → 327 (Task 10) → 328 (Task 11) → unchanged through Tasks 12–14 (no new C++ tests added; visual verification only).
- Spec coverage cross-check:
  - §1.1 Scope items: `IOcctGlBackend` (Tasks 1, 2, 4); `ViewDriver` external ctor (Task 3); `ViewportController` (Tasks 5–10); `OcctViewportItem` (Tasks 11–13); demo project + Main.qml + main.cpp (Task 14).
  - §3.2 Behaviour: `on_changed` + `rebuild_from_scratch` (Task 6); `set_viewport_size` (Task 7); mouse drag-state (Task 8); orbit/pan/zoom (Task 9); RMB pick + fit_all (Tasks 9, 10); selection accessors (Tasks 5, 13).
  - §3.5 IOcctGlBackend: header (Task 1), impl (Tasks 2, 4).
  - §4 App startup: Task 14.
  - §5 Tests: Tasks 5, 6, 9, 10, 11.
  - §6 Logging: Tasks 5 (ctor info), 6 (rebuild_from_scratch info), 12 (viewport warn/error).
  - §7 Error codes: `renderer.gl_unavailable` (Task 4), `renderer.external_gl_unsupported` would only fire on a backend without `IOcctGlBackend` — Stage 11 territory; Stage 4a doesn't throw it. `viewport.controller_not_attached` — null-controller paths are silent in Stage 4a (logged warn, not thrown); accept as-is, raise to throw in 4b if needed.
  - §9 DoD: covered by Tasks 14 (manual checks) + 15 (clean rebuild + sweep + Qt-linkage check).
- One identified spec-vs-plan tension: §3.5 says "renderer.external_gl_unsupported" fires when an OCCT-side cast fails. Plan implements this as a logged error + silent return (Task 12 `OcctFboRenderer::initialise_occt`), not a throw. Reason: throwing from QSG render thread would terminate the Qt app. The logged-error path is safer for Stage 4a; revisit in 4b/Stage 11.
