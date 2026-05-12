# Stage 4b — Property Panels Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a Qt Quick inspector panel on the right side of the main window that lets the user edit Cabinet properties (when nothing is selected) or the selected Panel's properties (label, thickness override, material override). Edits dispatch through `UndoStack`; Cmd/Ctrl+Z/Y undoes/redoes; renderer refreshes immediately.

**Architecture:** New `coupecad_ui` static library hosting `QObject` proxy classes — `CabinetPropertiesProxy`, `PanelPropertiesProxy`, `UndoStackProxy`. `ViewportController` is promoted to `QObject` and gains `selectionChanged`/`cabinetChanged`/`panelChanged` signals; proxies subscribe via Qt signals. Existing Stage 1b commands (`SetCabinetDimensions`, `SetCabinetDefaults`, `SetPanelLabel`/`Thickness`/`Material`) are the only mutation path; one new command `SetCabinetName` adds cabinet renaming.

**Tech Stack:** C++20, Qt 6.5+ (Core, Qml, Quick, Quick.Controls, Quick.Layouts), GoogleTest, QSignalSpy (Qt6::Test). Plus existing OCCT/Conan stack.

**Связанные документы:**
- Дизайн: [`../specs/2026-05-12-stage-4b-property-panels-design.md`](../specs/2026-05-12-stage-4b-property-panels-design.md)
- Stage 4a viewport: [`../specs/2026-05-07-stage-4a-viewport-design.md`](../specs/2026-05-07-stage-4a-viewport-design.md)
- Stage 1 commands: `src/coupecad/core/commands/`

**Definition of Done (см. spec §10):** `coupecad_ui` builds on Linux + Windows CI; app launches with SplitView; inspector switches between Cabinet and Panel via selection; edits go through commands and undo/redo; camera-precision tests pass; existing 328 tests still pass; renderer libs still 0 Qt symbols.

---

## File Structure

After Stage 4b:

```
src/coupecad/core/commands/
    cabinet_commands.{h,cpp}            # +SetCabinetName (Task 1)
    command.h                           # +CommandKind::SetCabinetName (Task 1)

src/coupecad/viewport/
    viewport_controller.{h,cpp}         # promote to QObject + signals + carry fields

src/coupecad/ui/
    CMakeLists.txt                      # new
    cabinet_properties_proxy.{h,cpp}    # new
    panel_properties_proxy.{h,cpp}      # new
    undo_stack_proxy.{h,cpp}            # new

apps/coupecad/
    CMakeLists.txt                      # +link coupecad_ui, +new QML files
    main.cpp                            # +register proxies as context properties
    qml/
        Main.qml                        # SplitView; viewport + inspector
        CabinetPropertiesPanel.qml      # new
        PanelPropertiesPanel.qml        # new

tests/core/commands/
    cabinet_commands_test.cpp           # +SetCabinetName tests

tests/viewport/
    viewport_controller_camera_precision_test.cpp   # new

tests/ui/
    CMakeLists.txt                      # new
    cabinet_properties_proxy_test.cpp   # new
    panel_properties_proxy_test.cpp     # new
    undo_stack_proxy_test.cpp           # new
```

Modified:
- `src/CMakeLists.txt` — `+add_subdirectory(coupecad/ui)`
- `tests/CMakeLists.txt` — `+add_subdirectory(ui)`
- `src/coupecad/renderer/occt/occt_viewport_item.cpp` — relay controller's `selectionChanged`

---

## Task 1: SetCabinetName core-command

**Files:**
- Modify: `src/coupecad/core/commands/command.h` (+`CommandKind::SetCabinetName`)
- Modify: `src/coupecad/core/commands/cabinet_commands.h` (+class)
- Modify: `src/coupecad/core/commands/cabinet_commands.cpp` (+impl)
- Modify: `tests/core/commands/cabinet_commands_test.cpp` (+tests)

- [ ] **Step 1: Add enum value to `command.h`**

In `src/coupecad/core/commands/command.h`, after the line `CabinetDefaults,` (line 16) add:

```cpp
    SetCabinetName,
```

- [ ] **Step 2: Add class declaration to `cabinet_commands.h`**

Append at the end of the namespace (after `SetCabinetDefaults` class, before the closing `}  // namespace`):

```cpp

// Переименование шкафа. Discrete.
class SetCabinetName : public Command {
public:
    SetCabinetName(CabinetId target, std::string new_name);

    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return "Set cabinet name"; }
    CommandKind kind() const noexcept override { return CommandKind::SetCabinetName; }

private:
    CabinetId target_;
    std::string new_name_;
    std::string old_name_;
    bool applied_ = false;
};
```

Add include at the top of the file (after existing includes):

```cpp
#include <string>
```

- [ ] **Step 3: Add implementation to `cabinet_commands.cpp`**

Append at the end of the `namespace coupecad::core {` block (before the closing `}  // namespace`):

```cpp

SetCabinetName::SetCabinetName(CabinetId target, std::string new_name)
    : target_(target), new_name_(std::move(new_name)) {
    if (new_name_.empty()) {
        throw DomainError{"cabinet.empty_name",
                          "Cabinet name must not be empty"};
    }
}

ChangeSet SetCabinetName::apply(Project& project) {
    auto& c = project.mutable_cabinet();
    if (c.id != target_) {
        throw DomainError{"cabinet.wrong_id",
                          "SetCabinetName target mismatch"};
    }
    if (!applied_) old_name_ = c.name;
    c.name = new_name_;
    applied_ = true;
    ChangeSet cs;
    cs.cabinet_changed = true;
    return cs;
}

ChangeSet SetCabinetName::revert(Project& project) {
    auto& c = project.mutable_cabinet();
    if (c.id != target_) {
        throw DomainError{"cabinet.wrong_id",
                          "SetCabinetName revert target mismatch"};
    }
    c.name = old_name_;
    ChangeSet cs;
    cs.cabinet_changed = true;
    return cs;
}
```

- [ ] **Step 4: Write failing tests in `tests/core/commands/cabinet_commands_test.cpp`**

Append at the end of the file (look at the existing `TEST(SetCabinetDimensionsTest, ...)` pattern to match style):

```cpp
TEST(SetCabinetNameTest, AppliesAndStoresPreviousName) {
    Project project = Project::create_empty("Initial");
    SetCabinetName cmd{project.cabinet().id, "Renamed"};
    auto cs = cmd.apply(project);
    EXPECT_EQ(project.cabinet().name, "Renamed");
    EXPECT_TRUE(cs.cabinet_changed);
}

TEST(SetCabinetNameTest, RevertRestoresName) {
    Project project = Project::create_empty("Initial");
    SetCabinetName cmd{project.cabinet().id, "Renamed"};
    cmd.apply(project);
    cmd.revert(project);
    EXPECT_EQ(project.cabinet().name, "Initial");
}

TEST(SetCabinetNameTest, RejectsEmptyName) {
    try {
        SetCabinetName cmd{CabinetId{}, ""};
        FAIL() << "expected DomainError";
    } catch (const DomainError& e) {
        EXPECT_EQ(e.code(), "cabinet.empty_name");
    }
}

TEST(SetCabinetNameTest, WrongIdThrows) {
    Project project = Project::create_empty("Initial");
    SetCabinetName cmd{CabinetId::from_string("00000000-0000-0000-0000-000000000099"),
                       "Renamed"};
    try {
        cmd.apply(project);
        FAIL() << "expected DomainError";
    } catch (const DomainError& e) {
        EXPECT_EQ(e.code(), "cabinet.wrong_id");
    }
}
```

If `using core::SetCabinetName;` / `using core::DomainError;` etc. are not already at the top of the test file, add them after existing `using` statements.

- [ ] **Step 5: Build + run targeted**

```bash
cmake --build --preset default --target coupecad_core_commands_test
ctest --preset default -R SetCabinetNameTest --output-on-failure
```

Expected: 4 tests, 4 passed.

- [ ] **Step 6: Full sweep**

```bash
ctest --preset default --output-on-failure
```

Expected: 332 passing (328 + 4 new).

- [ ] **Step 7: Commit**

```bash
git add src/coupecad/core/commands/command.h \
        src/coupecad/core/commands/cabinet_commands.h \
        src/coupecad/core/commands/cabinet_commands.cpp \
        tests/core/commands/cabinet_commands_test.cpp
git commit -m "feat(core): SetCabinetName command (apply/revert/validate)"
```

## Task 2: ViewportController → QObject + signal declarations

**Files:**
- Modify: `src/coupecad/viewport/viewport_controller.h`
- Modify: `src/coupecad/viewport/viewport_controller.cpp`

> Declarative changes only — signals are declared but no emit-points are added yet. Signal emission lands in Task 3. Carry fields land in Task 4. Existing 328 tests must remain green at the end of this task.

- [ ] **Step 1: Modify `src/coupecad/viewport/viewport_controller.h`**

Add `#include <QObject>` and `#include <QString>` near the top after existing includes:

```cpp
#include <QObject>
#include <QString>
```

Replace the class declaration `class ViewportController : public core::IProjectObserver {` with:

```cpp
class ViewportController : public QObject, public core::IProjectObserver {
    Q_OBJECT
public:
```

Change the constructor signature to accept an optional Qt parent (so the controller can be parented to a QObject for lifetime management):

```cpp
    ViewportController(core::Project& project,
                       geometry::GeometryBuilder& builder,
                       renderer::IRenderer& renderer,
                       QObject* parent = nullptr);
```

Just before the `private:` section, add:

```cpp
signals:
    void selectionChanged();
    void cabinetChanged();
    void panelChanged(const QString& panel_id_str);

public:
```

(The `public:` after `signals:` resets visibility for the existing private fields that follow.)

Wait — re-read the header: the existing layout is `public:` block, then `private:` at line 63. After adding `signals:` at the end of public, we don't need to flip back to public; just keep `private:` after. Final order:

```cpp
public:
    // ... existing public methods ...

signals:
    void selectionChanged();
    void cabinetChanged();
    void panelChanged(const QString& panel_id_str);

private:
    // ... existing private members ...
};
```

- [ ] **Step 2: Modify `src/coupecad/viewport/viewport_controller.cpp`**

Change the constructor implementation to forward `parent`:

```cpp
ViewportController::ViewportController(core::Project& project,
                                       geometry::GeometryBuilder& builder,
                                       renderer::IRenderer& renderer_in,
                                       QObject* parent)
    : QObject(parent), project_(project), builder_(builder), renderer_(renderer_in) {
    coupecad::logging::Logger::instance().info(
        "viewport", "ViewportController constructed");
}
```

(Replace the existing constructor definition. The destructor stays `= default`.)

- [ ] **Step 3: Build**

```bash
cmake --build --preset default --target coupecad_viewport
```

Expected: builds clean. Qt's MOC processes `Q_OBJECT` because `AUTOMOC` is already enabled on `coupecad_viewport` (Stage 4a Task 5).

- [ ] **Step 4: Full sweep — no regressions**

```bash
ctest --preset default --output-on-failure
```

Expected: 332 still passing. No new tests in this task.

- [ ] **Step 5: Commit**

```bash
git add src/coupecad/viewport/viewport_controller.h \
        src/coupecad/viewport/viewport_controller.cpp
git commit -m "feat(viewport): ViewportController promoted to QObject (signals declared)"
```

## Task 3: Emit signals + OcctViewportItem relay

**Files:**
- Modify: `src/coupecad/viewport/viewport_controller.cpp`
- Modify: `src/coupecad/viewport/occt_viewport_item.cpp`

> Wires the actual signal emissions. After this task, external observers can react to selection/cabinet/panel changes; `OcctViewportItem::selectionChanged` becomes a relay.

- [ ] **Step 1: Modify `on_changed` in `viewport_controller.cpp`**

Replace the existing `on_changed` body with:

```cpp
void ViewportController::on_changed(const core::Project&,
                                    const core::ChangeSet& cs) {
    if (cs.empty()) return;
    builder_.apply_changes(cs);
    renderer_.sync(cs);
    dirty_ = true;

    if (cs.cabinet_changed) {
        emit cabinetChanged();
    }
    for (const auto& id : cs.updated_panels) {
        emit panelChanged(QString::fromStdString(id.to_string()));
    }

    // Removed panels/hardware may have been in the selection set. AIS
    // drops them automatically (Stage 3 §5), so we only signal the
    // delta if anything was removed.
    if (!cs.removed_panels.empty() || !cs.removed_hardware.empty()) {
        emit selectionChanged();
    }
}
```

- [ ] **Step 2: Modify `on_mouse_release` in `viewport_controller.cpp`**

Replace the existing `on_mouse_release` body with:

```cpp
void ViewportController::on_mouse_release(int x, int y, MouseButton btn) {
    constexpr int kPickDragThreshold = 5;  // pixels

    bool selection_did_change = false;

    if (btn == MouseButton::Right && active_drag_ == MouseButton::Right &&
        drag_total_dx_ + drag_total_dy_ < kPickDragThreshold) {
        const std::size_t prev = renderer_.selection().size();
        renderer_.clear_selection();
        if (auto hit = renderer_.pick(x, y)) {
            renderer_.select(*hit);
        }
        dirty_ = true;
        selection_did_change = (renderer_.selection().size() != prev) ||
                               (prev != 0);  // clear-then-no-hit also "changes"
    }

    active_drag_ = MouseButton::None;

    if (selection_did_change) {
        emit selectionChanged();
    }
}
```

- [ ] **Step 3: Modify `OcctViewportItem` to relay the signal**

Open `src/coupecad/viewport/occt_viewport_item.cpp`. Replace `OcctViewportItem::set_controller` with:

```cpp
void OcctViewportItem::set_controller(ViewportController* c) {
    if (controller_ == c) return;
    if (controller_ != nullptr) {
        QObject::disconnect(controller_, &ViewportController::selectionChanged,
                            this,        &OcctViewportItem::selectionChanged);
    }
    controller_ = c;
    if (controller_ != nullptr) {
        QObject::connect(controller_, &ViewportController::selectionChanged,
                         this,        &OcctViewportItem::selectionChanged);
    }
    update();
}
```

Strip the manual `emit selectionChanged()` blocks from `mousePressEvent` and `mouseReleaseEvent` (the controller now owns that signal). The pre/post `selection_count()` capture and final `if (... != prev) emit selectionChanged();` lines go away. Final shape of `mousePressEvent`:

```cpp
void OcctViewportItem::mousePressEvent(QMouseEvent* e) {
    if (controller_ == nullptr) return;
    forceActiveFocus();
    const int x = static_cast<int>(e->position().x());
    const int y = static_cast<int>(e->position().y());
    controller_->on_mouse_press(x, y, qt_button_to_mouse_button(e->button()));
    e->accept();
    update();
}
```

And `mouseReleaseEvent`:

```cpp
void OcctViewportItem::mouseReleaseEvent(QMouseEvent* e) {
    if (controller_ == nullptr) return;
    const int x = static_cast<int>(e->position().x());
    const int y = static_cast<int>(e->position().y());
    controller_->on_mouse_release(x, y, qt_button_to_mouse_button(e->button()));
    e->accept();
    update();
}
```

- [ ] **Step 4: Build**

```bash
cmake --build --preset default --target coupecad_viewport
```

Expected: builds clean.

- [ ] **Step 5: Full sweep — no regressions**

```bash
ctest --preset default --output-on-failure
```

Expected: 332 still passing. Existing tests that check `controller.selection()` after RMB-pick still pass because the renderer state is unchanged; the signal flow merely moves from item to controller.

- [ ] **Step 6: Commit**

```bash
git add src/coupecad/viewport/viewport_controller.cpp \
        src/coupecad/viewport/occt_viewport_item.cpp
git commit -m "feat(viewport): emit selectionChanged/cabinetChanged/panelChanged + relay"
```

## Task 4: Camera precision (sub-mm carry)

**Files:**
- Modify: `src/coupecad/viewport/viewport_controller.h`
- Modify: `src/coupecad/viewport/viewport_controller.cpp`
- Create: `tests/viewport/viewport_controller_camera_precision_test.cpp`
- Modify: `tests/viewport/CMakeLists.txt`

- [ ] **Step 1: Add carry fields to the header**

In `src/coupecad/viewport/viewport_controller.h`, after the existing `int drag_total_dy_ = 0;` field (inside the `private:` block), append:

```cpp

    // Sub-mm accumulators for pan/orbit/zoom. Each emit-camera step
    // rounds the desired double-precision target to int32 mm, then
    // stashes the fractional remainder here for the next step. Reset
    // by fit_all() (any external camera reset).
    double eye_carry_x_    = 0.0;
    double eye_carry_y_    = 0.0;
    double eye_carry_z_    = 0.0;
    double target_carry_x_ = 0.0;
    double target_carry_y_ = 0.0;
    double target_carry_z_ = 0.0;
```

- [ ] **Step 2: Rework `on_mouse_move` (LMB orbit, MMB pan) and `on_wheel` to use carries**

The existing implementations cast `static_cast<std::int32_t>(...)` directly, which truncates. Replace them with the carry-aware versions.

In `src/coupecad/viewport/viewport_controller.cpp`, replace the **LMB orbit** branch inside `on_mouse_move`. Find the block that begins with `if (active_drag_ == MouseButton::Left) {` and ends with `return;`; replace the `s.eye = core::Vec3{ ... };` assignment with:

```cpp
        const double new_eye_x_d = tx + new_ex + eye_carry_x_;
        const double new_eye_y_d = ty + new_ey + eye_carry_y_;
        const double new_eye_z_d = tz + new_ez + eye_carry_z_;
        const std::int32_t new_eye_x = static_cast<std::int32_t>(std::lround(new_eye_x_d));
        const std::int32_t new_eye_y = static_cast<std::int32_t>(std::lround(new_eye_y_d));
        const std::int32_t new_eye_z = static_cast<std::int32_t>(std::lround(new_eye_z_d));
        eye_carry_x_ = new_eye_x_d - static_cast<double>(new_eye_x);
        eye_carry_y_ = new_eye_y_d - static_cast<double>(new_eye_y);
        eye_carry_z_ = new_eye_z_d - static_cast<double>(new_eye_z);

        s.eye = core::Vec3{
            core::Millimeters{new_eye_x},
            core::Millimeters{new_eye_y},
            core::Millimeters{new_eye_z}};
```

Replace the **MMB pan** branch's eye+target assignments. Find the block `if (active_drag_ == MouseButton::Middle) {` and replace its `s.eye = core::Vec3{ ... };` + `s.target = core::Vec3{ ... };` block with:

```cpp
        const double new_eye_x_d    = static_cast<double>(s.eye.x.value())    + world_dx + eye_carry_x_;
        const double new_eye_y_d    = static_cast<double>(s.eye.y.value())    + world_dy + eye_carry_y_;
        const double new_eye_z_d    = static_cast<double>(s.eye.z.value())    + world_dz + eye_carry_z_;
        const double new_target_x_d = static_cast<double>(s.target.x.value()) + world_dx + target_carry_x_;
        const double new_target_y_d = static_cast<double>(s.target.y.value()) + world_dy + target_carry_y_;
        const double new_target_z_d = static_cast<double>(s.target.z.value()) + world_dz + target_carry_z_;

        const std::int32_t new_eye_x    = static_cast<std::int32_t>(std::lround(new_eye_x_d));
        const std::int32_t new_eye_y    = static_cast<std::int32_t>(std::lround(new_eye_y_d));
        const std::int32_t new_eye_z    = static_cast<std::int32_t>(std::lround(new_eye_z_d));
        const std::int32_t new_target_x = static_cast<std::int32_t>(std::lround(new_target_x_d));
        const std::int32_t new_target_y = static_cast<std::int32_t>(std::lround(new_target_y_d));
        const std::int32_t new_target_z = static_cast<std::int32_t>(std::lround(new_target_z_d));

        eye_carry_x_ = new_eye_x_d - static_cast<double>(new_eye_x);
        eye_carry_y_ = new_eye_y_d - static_cast<double>(new_eye_y);
        eye_carry_z_ = new_eye_z_d - static_cast<double>(new_eye_z);
        target_carry_x_ = new_target_x_d - static_cast<double>(new_target_x);
        target_carry_y_ = new_target_y_d - static_cast<double>(new_target_y);
        target_carry_z_ = new_target_z_d - static_cast<double>(new_target_z);

        s.eye = core::Vec3{
            core::Millimeters{new_eye_x},
            core::Millimeters{new_eye_y},
            core::Millimeters{new_eye_z}};
        s.target = core::Vec3{
            core::Millimeters{new_target_x},
            core::Millimeters{new_target_y},
            core::Millimeters{new_target_z}};
```

Replace the body of `on_wheel` with:

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

    const double new_eye_x_d = tx + ex * factor + eye_carry_x_;
    const double new_eye_y_d = ty + ey * factor + eye_carry_y_;
    const double new_eye_z_d = tz + ez * factor + eye_carry_z_;

    const std::int32_t new_eye_x = static_cast<std::int32_t>(std::lround(new_eye_x_d));
    const std::int32_t new_eye_y = static_cast<std::int32_t>(std::lround(new_eye_y_d));
    const std::int32_t new_eye_z = static_cast<std::int32_t>(std::lround(new_eye_z_d));

    eye_carry_x_ = new_eye_x_d - static_cast<double>(new_eye_x);
    eye_carry_y_ = new_eye_y_d - static_cast<double>(new_eye_y);
    eye_carry_z_ = new_eye_z_d - static_cast<double>(new_eye_z);

    s.eye = core::Vec3{
        core::Millimeters{new_eye_x},
        core::Millimeters{new_eye_y},
        core::Millimeters{new_eye_z}};

    renderer_.set_camera(s);
    dirty_ = true;
}
```

- [ ] **Step 3: Reset carries on `fit_all`**

Replace the body of `fit_all`:

```cpp
void ViewportController::fit_all() {
    eye_carry_x_ = eye_carry_y_ = eye_carry_z_ = 0.0;
    target_carry_x_ = target_carry_y_ = target_carry_z_ = 0.0;
    renderer_.fit_all();
    dirty_ = true;
}
```

- [ ] **Step 4: Write the failing test `tests/viewport/viewport_controller_camera_precision_test.cpp`**

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

CameraState small_distance_camera() {
    // 100 mm from target — small distance accentuates the rounding-to-zero
    // problem in the un-carried code (pan_scale = 0.002 * 100 = 0.2 mm/px,
    // so 1px drag would round to 0 without carry).
    CameraState s;
    s.eye    = {Millimeters{0}, Millimeters{-100}, Millimeters{0}};
    s.target = {Millimeters{0}, Millimeters{0},   Millimeters{0}};
    s.up     = {Millimeters{0}, Millimeters{0},   Millimeters{1}};
    s.fov_deg = 45.0;
    return s;
}

}  // namespace

TEST(ViewportControllerCameraPrecisionTest, SmallPanStepsAccumulate) {
    Project project = Project::create_empty("prec1");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    fake.set_camera(small_distance_camera());
    ViewportController controller{project, builder, fake};

    // 1000× MMB drag of 1px right. Expected pan magnitude per step is
    // 0.2 mm in -X (per Stage 4a pan-scale formula); cumulative ~200 mm.
    controller.on_mouse_press(0, 0, MouseButton::Middle);
    for (int i = 1; i <= 1000; ++i) {
        controller.on_mouse_move(i, 0, MouseButton::Middle);
    }
    controller.on_mouse_release(1000, 0, MouseButton::Middle);

    const auto& final_state = fake.camera();
    // Cumulative pan magnitude — both eye and target should have moved.
    // Allow ±5 mm slop for accumulated rounding (target ~200 mm).
    EXPECT_NEAR(static_cast<double>(final_state.eye.x.value()),    -200.0, 5.0);
    EXPECT_NEAR(static_cast<double>(final_state.target.x.value()), -200.0, 5.0);
    // Eye-target vector preserved (pan invariant).
    EXPECT_EQ(final_state.eye.y.value() - final_state.target.y.value(), -100);
}

TEST(ViewportControllerCameraPrecisionTest, WheelZoomNoDriftOnNoOp) {
    Project project = Project::create_empty("prec2");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    const auto initial = small_distance_camera();
    fake.set_camera(initial);
    ViewportController controller{project, builder, fake};

    for (int i = 0; i < 50; ++i) {
        controller.on_wheel(0, 0, 0.0);   // factor = 1.0
    }

    const auto& after = fake.camera();
    EXPECT_EQ(after.eye,    initial.eye);
    EXPECT_EQ(after.target, initial.target);
}

TEST(ViewportControllerCameraPrecisionTest, FitAllResetsCarries) {
    Project project = Project::create_empty("prec3");
    GeometryBuilder builder{project};
    FakeRenderer fake;
    fake.set_camera(small_distance_camera());
    ViewportController controller{project, builder, fake};

    // Drag to accumulate carry.
    controller.on_mouse_press(0, 0, MouseButton::Middle);
    for (int i = 1; i <= 50; ++i) controller.on_mouse_move(i, 0, MouseButton::Middle);
    controller.on_mouse_release(50, 0, MouseButton::Middle);

    controller.fit_all();
    // fit_all delegates to the FakeRenderer which is a no-op; assert via
    // a follow-up zero-pan: should produce zero net motion if carries
    // were correctly reset.
    const auto before = fake.camera();
    controller.on_mouse_press(0, 0, MouseButton::Middle);
    controller.on_mouse_release(0, 0, MouseButton::Middle);    // no move
    EXPECT_EQ(fake.camera().eye,    before.eye);
    EXPECT_EQ(fake.camera().target, before.target);
}
```

- [ ] **Step 5: Add the test to `tests/viewport/CMakeLists.txt`**

Inside the existing `add_executable(coupecad_viewport_test ...)` source list, add `viewport_controller_camera_precision_test.cpp` after `viewport_controller_pick_test.cpp`:

```cmake
add_executable(coupecad_viewport_test
    viewport_controller_test.cpp
    viewport_controller_observer_test.cpp
    viewport_controller_camera_test.cpp
    viewport_controller_pick_test.cpp
    viewport_controller_camera_precision_test.cpp
)
```

(Leave the rest of the file unchanged, including the gtest-include-order workaround block.)

- [ ] **Step 6: Build + run targeted**

```bash
cmake --build --preset default --target coupecad_viewport_test
ctest --preset default -R ViewportControllerCameraPrecisionTest --output-on-failure
```

Expected: 3 tests, 3 passed.

- [ ] **Step 7: Full sweep**

```bash
ctest --preset default --output-on-failure
```

Expected: 335 passing (332 + 3 new).

- [ ] **Step 8: Commit**

```bash
git add src/coupecad/viewport/viewport_controller.h \
        src/coupecad/viewport/viewport_controller.cpp \
        tests/viewport/viewport_controller_camera_precision_test.cpp \
        tests/viewport/CMakeLists.txt
git commit -m "feat(viewport): sub-mm carry accumulators for pan/orbit/zoom"
```

## Task 5: coupecad_ui module skeleton + UndoStackProxy

**Files:**
- Create: `src/coupecad/ui/CMakeLists.txt`
- Create: `src/coupecad/ui/undo_stack_proxy.h`
- Create: `src/coupecad/ui/undo_stack_proxy.cpp`
- Create: `tests/ui/CMakeLists.txt`
- Create: `tests/ui/undo_stack_proxy_test.cpp`
- Modify: `src/CMakeLists.txt` (+ add_subdirectory)
- Modify: `tests/CMakeLists.txt` (+ add_subdirectory)

> Establishes the `coupecad_ui` library with the smallest proxy first. Stages 6-11 fill in the proxies for Cabinet and Panel.

- [ ] **Step 1: Create `src/coupecad/ui/CMakeLists.txt`**

```cmake
find_package(Qt6 6.5 REQUIRED COMPONENTS Core)

add_library(coupecad_ui STATIC
    undo_stack_proxy.cpp
)

target_include_directories(coupecad_ui PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(coupecad_ui
    PUBLIC
        coupecad_core
        coupecad_viewport
        coupecad_logging
        Qt6::Core
)

target_compile_features(coupecad_ui PUBLIC cxx_std_20)

set_target_properties(coupecad_ui PROPERTIES AUTOMOC ON)
```

- [ ] **Step 2: Hook into `src/CMakeLists.txt`** — replace file with:

```cmake
add_subdirectory(coupecad/logging)
add_subdirectory(coupecad/core)
add_subdirectory(coupecad/geometry)
add_subdirectory(coupecad/renderer)
add_subdirectory(coupecad/viewport)
add_subdirectory(coupecad/ui)
```

- [ ] **Step 3: Create `src/coupecad/ui/undo_stack_proxy.h`**

```cpp
#pragma once

#include <QObject>

namespace coupecad::core { class UndoStack; }

namespace coupecad::ui {

// Тонкая Qt-обёртка над core::UndoStack для QML-биндингов и Shortcut'ов.
// core::UndoStack остаётся Qt-free.
class UndoStackProxy : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool canUndo READ can_undo NOTIFY changed)
    Q_PROPERTY(bool canRedo READ can_redo NOTIFY changed)

public:
    explicit UndoStackProxy(core::UndoStack& undo, QObject* parent = nullptr);

    bool can_undo() const;
    bool can_redo() const;

    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

signals:
    void changed();

private:
    core::UndoStack& undo_;
};

}  // namespace coupecad::ui
```

- [ ] **Step 4: Create `src/coupecad/ui/undo_stack_proxy.cpp`**

```cpp
#include "coupecad/ui/undo_stack_proxy.h"

#include "coupecad/core/undo_stack.h"
#include "coupecad/logging/logger.h"

namespace coupecad::ui {

UndoStackProxy::UndoStackProxy(core::UndoStack& undo, QObject* parent)
    : QObject(parent), undo_(undo) {
    coupecad::logging::Logger::instance().info(
        "ui", "UndoStackProxy constructed");
}

bool UndoStackProxy::can_undo() const { return undo_.can_undo(); }
bool UndoStackProxy::can_redo() const { return undo_.can_redo(); }

void UndoStackProxy::undo() {
    if (!undo_.can_undo()) return;
    undo_.undo();
    emit changed();
}

void UndoStackProxy::redo() {
    if (!undo_.can_redo()) return;
    undo_.redo();
    emit changed();
}

}  // namespace coupecad::ui
```

- [ ] **Step 5: Create `tests/ui/CMakeLists.txt`**

```cmake
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Test)

add_executable(coupecad_ui_test
    undo_stack_proxy_test.cpp
)

target_link_libraries(coupecad_ui_test
    PRIVATE
        coupecad_ui
        coupecad_core
        coupecad_logging
        GTest::gtest
        GTest::gtest_main
        Qt6::Core
        Qt6::Test
)

target_include_directories(coupecad_ui_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests/viewport     # reuse FakeRenderer when needed
)

# macOS gtest-include-order workaround (Conan vs Homebrew header clash).
get_target_property(_gtest_inc GTest::gtest INTERFACE_INCLUDE_DIRECTORIES)
if(_gtest_inc)
    target_include_directories(coupecad_ui_test BEFORE PRIVATE ${_gtest_inc})
endif()

include(GoogleTest)
gtest_discover_tests(coupecad_ui_test)
```

- [ ] **Step 6: Hook test target into `tests/CMakeLists.txt`** — replace file with:

```cmake
find_package(GTest REQUIRED)

add_subdirectory(smoke)
add_subdirectory(app)
add_subdirectory(logging)
add_subdirectory(core)
add_subdirectory(geometry)
add_subdirectory(renderer)
add_subdirectory(viewport)
add_subdirectory(ui)
```

- [ ] **Step 7: Create `tests/ui/undo_stack_proxy_test.cpp`**

For tests that use `Q_INVOKABLE` and signal emission, we need a `QCoreApplication` instance. Use a static guard.

Pick a real command to push into the stack — `SetCabinetName` from Task 1 is the simplest discrete command.

```cpp
#include "coupecad/ui/undo_stack_proxy.h"

#include "coupecad/core/commands/cabinet_commands.h"
#include "coupecad/core/project.h"
#include "coupecad/core/undo_stack.h"

#include <QCoreApplication>
#include <QSignalSpy>

#include <gtest/gtest.h>

namespace {

bool s_qapp_initialised = false;
QCoreApplication* s_qapp = nullptr;

void ensure_qapp() {
    if (s_qapp_initialised) return;
    static int    argc = 1;
    static char   arg0[] = "ui_test";
    static char*  argv[] = {arg0, nullptr};
    s_qapp = new QCoreApplication(argc, argv);
    s_qapp_initialised = true;
}

}  // namespace

using coupecad::core::Project;
using coupecad::core::SetCabinetName;
using coupecad::core::UndoStack;
using coupecad::ui::UndoStackProxy;

TEST(UndoStackProxyTest, CanUndoFalseInitially) {
    ensure_qapp();
    Project project = Project::create_empty("Initial");
    UndoStack undo{project};
    UndoStackProxy proxy{undo};
    EXPECT_FALSE(proxy.can_undo());
    EXPECT_FALSE(proxy.can_redo());
}

TEST(UndoStackProxyTest, UndoDelegatesToCoreStack) {
    ensure_qapp();
    Project project = Project::create_empty("Initial");
    UndoStack undo{project};
    UndoStackProxy proxy{undo};

    undo.execute(std::make_unique<SetCabinetName>(project.cabinet().id, "Renamed"));
    ASSERT_EQ(project.cabinet().name, "Renamed");

    proxy.undo();
    EXPECT_EQ(project.cabinet().name, "Initial");
}

TEST(UndoStackProxyTest, RedoDelegatesToCoreStack) {
    ensure_qapp();
    Project project = Project::create_empty("Initial");
    UndoStack undo{project};
    UndoStackProxy proxy{undo};

    undo.execute(std::make_unique<SetCabinetName>(project.cabinet().id, "Renamed"));
    proxy.undo();
    ASSERT_EQ(project.cabinet().name, "Initial");

    proxy.redo();
    EXPECT_EQ(project.cabinet().name, "Renamed");
}

TEST(UndoStackProxyTest, UndoEmitsChanged) {
    ensure_qapp();
    Project project = Project::create_empty("Initial");
    UndoStack undo{project};
    UndoStackProxy proxy{undo};
    undo.execute(std::make_unique<SetCabinetName>(project.cabinet().id, "Renamed"));

    QSignalSpy spy(&proxy, &UndoStackProxy::changed);
    proxy.undo();
    EXPECT_EQ(spy.count(), 1);
}
```

- [ ] **Step 8: Configure + build + run targeted**

```bash
conan install . --build=missing -s build_type=Debug
cmake --preset default
cmake --build --preset default --target coupecad_ui_test
ctest --preset default -R UndoStackProxyTest --output-on-failure
```

Expected: 4 tests, 4 passed.

If `Qt6::Test` is not found, the `find_package(Qt6 6.5 REQUIRED COMPONENTS Core Test)` line will fail at configure. In that case, write a minimal hand-rolled signal-spy as a fallback (see context note at bottom of this task).

- [ ] **Step 9: Full sweep**

```bash
ctest --preset default --output-on-failure
```

Expected: 339 passing (335 + 4 new).

- [ ] **Step 10: Commit**

```bash
git add src/coupecad/ui/ \
        src/CMakeLists.txt \
        tests/ui/ \
        tests/CMakeLists.txt
git commit -m "feat(ui): coupecad_ui module skeleton + UndoStackProxy"
```

> **Context note (Qt6::Test availability):** Qt6::Test is part of the standard Qt distribution and is provided by `aqtinstall` automatically. If for some reason it's missing in CI, fallback: drop `Qt6::Test` from link, replace `QSignalSpy` with a custom callback connected to the signal that increments a counter. Keeps the test green without QtTest.

## Task 6: CabinetPropertiesProxy — read-only Q_PROPERTYs

**Files:**
- Create: `src/coupecad/ui/cabinet_properties_proxy.h`
- Create: `src/coupecad/ui/cabinet_properties_proxy.cpp`
- Modify: `src/coupecad/ui/CMakeLists.txt` (add to sources)
- Create: `tests/ui/cabinet_properties_proxy_test.cpp`
- Modify: `tests/ui/CMakeLists.txt`

> This task lands the header, ctor, and getters — but setters throw `LogicError{"ui.not_implemented_yet"}` for now. Setters come in Tasks 7 and 8.

- [ ] **Step 1: Create `src/coupecad/ui/cabinet_properties_proxy.h`**

```cpp
#pragma once

#include "coupecad/core/project.h"
#include "coupecad/viewport/viewport_controller.h"

#include <QObject>
#include <QString>

namespace coupecad::core { class UndoStack; }

namespace coupecad::ui {

class CabinetPropertiesProxy : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name      READ name      WRITE setName      NOTIFY changed)
    Q_PROPERTY(int     widthMm   READ widthMm   WRITE setWidthMm   NOTIFY changed)
    Q_PROPERTY(int     depthMm   READ depthMm   WRITE setDepthMm   NOTIFY changed)
    Q_PROPERTY(int     heightMm  READ heightMm  WRITE setHeightMm  NOTIFY changed)
    Q_PROPERTY(int     defaultPanelThicknessMm
                 READ default_panel_thickness_mm
                 WRITE set_default_panel_thickness_mm NOTIFY changed)
    Q_PROPERTY(int     defaultBackThicknessMm
                 READ default_back_thickness_mm
                 WRITE set_default_back_thickness_mm NOTIFY changed)

public:
    CabinetPropertiesProxy(core::Project& project,
                           core::UndoStack& undo,
                           viewport::ViewportController& controller,
                           QObject* parent = nullptr);

    QString name() const;
    int     widthMm() const;
    int     depthMm() const;
    int     heightMm() const;
    int     default_panel_thickness_mm() const;
    int     default_back_thickness_mm() const;

    void setName(const QString& value);
    void setWidthMm(int mm);
    void setDepthMm(int mm);
    void setHeightMm(int mm);
    void set_default_panel_thickness_mm(int mm);
    void set_default_back_thickness_mm(int mm);

signals:
    void changed();

private slots:
    void on_cabinet_changed();

private:
    core::Project&                project_;
    core::UndoStack&              undo_;
    viewport::ViewportController& controller_;
};

}  // namespace coupecad::ui
```

- [ ] **Step 2: Create `src/coupecad/ui/cabinet_properties_proxy.cpp`**

```cpp
#include "coupecad/ui/cabinet_properties_proxy.h"

#include "coupecad/core/errors.h"
#include "coupecad/core/undo_stack.h"
#include "coupecad/logging/logger.h"

namespace coupecad::ui {

CabinetPropertiesProxy::CabinetPropertiesProxy(
    core::Project& project, core::UndoStack& undo,
    viewport::ViewportController& controller, QObject* parent)
    : QObject(parent), project_(project), undo_(undo), controller_(controller) {
    QObject::connect(&controller_, &viewport::ViewportController::cabinetChanged,
                     this,         &CabinetPropertiesProxy::on_cabinet_changed);
    coupecad::logging::Logger::instance().info(
        "ui", "CabinetPropertiesProxy constructed");
}

void CabinetPropertiesProxy::on_cabinet_changed() {
    emit changed();
}

QString CabinetPropertiesProxy::name() const {
    return QString::fromStdString(project_.cabinet().name);
}

int CabinetPropertiesProxy::widthMm() const {
    return project_.cabinet().dimensions.width.value();
}

int CabinetPropertiesProxy::depthMm() const {
    return project_.cabinet().dimensions.depth.value();
}

int CabinetPropertiesProxy::heightMm() const {
    return project_.cabinet().dimensions.height.value();
}

int CabinetPropertiesProxy::default_panel_thickness_mm() const {
    return project_.cabinet().default_panel_thickness.value();
}

int CabinetPropertiesProxy::default_back_thickness_mm() const {
    return project_.cabinet().default_back_thickness.value();
}

// Setters land in Tasks 7-8. Stub now so the QObject can be instantiated.
void CabinetPropertiesProxy::setName(const QString& /*value*/) {
    throw core::LogicError{"ui.not_implemented_yet",
                           "setName fills in at Task 8"};
}
void CabinetPropertiesProxy::setWidthMm(int /*mm*/) {
    throw core::LogicError{"ui.not_implemented_yet",
                           "setWidthMm fills in at Task 7"};
}
void CabinetPropertiesProxy::setDepthMm(int /*mm*/) {
    throw core::LogicError{"ui.not_implemented_yet",
                           "setDepthMm fills in at Task 7"};
}
void CabinetPropertiesProxy::setHeightMm(int /*mm*/) {
    throw core::LogicError{"ui.not_implemented_yet",
                           "setHeightMm fills in at Task 7"};
}
void CabinetPropertiesProxy::set_default_panel_thickness_mm(int /*mm*/) {
    throw core::LogicError{"ui.not_implemented_yet",
                           "set_default_panel_thickness_mm fills in at Task 7"};
}
void CabinetPropertiesProxy::set_default_back_thickness_mm(int /*mm*/) {
    throw core::LogicError{"ui.not_implemented_yet",
                           "set_default_back_thickness_mm fills in at Task 7"};
}

}  // namespace coupecad::ui
```

- [ ] **Step 3: Add source to `src/coupecad/ui/CMakeLists.txt`**

```cmake
add_library(coupecad_ui STATIC
    undo_stack_proxy.cpp
    cabinet_properties_proxy.cpp
)
```

- [ ] **Step 4: Write failing tests `tests/ui/cabinet_properties_proxy_test.cpp`**

```cpp
#include "coupecad/ui/cabinet_properties_proxy.h"

#include "coupecad/core/project.h"
#include "coupecad/core/undo_stack.h"
#include "coupecad/core/units.h"
#include "coupecad/geometry/geometry_builder.h"
#include "coupecad/viewport/viewport_controller.h"
#include "fake_renderer.h"

#include <QCoreApplication>

#include <gtest/gtest.h>

namespace {

bool s_qapp_initialised = false;

void ensure_qapp() {
    if (s_qapp_initialised) return;
    static int    argc = 1;
    static char   arg0[] = "cabinet_proxy_test";
    static char*  argv[] = {arg0, nullptr};
    new QCoreApplication(argc, argv);
    s_qapp_initialised = true;
}

}  // namespace

using coupecad::core::Millimeters;
using coupecad::core::Project;
using coupecad::core::UndoStack;
using coupecad::geometry::GeometryBuilder;
using coupecad::ui::CabinetPropertiesProxy;
using coupecad::viewport::ViewportController;
using coupecad::viewport::testing::FakeRenderer;

namespace {

struct Fixture {
    Project project = Project::create_empty("Initial");
    UndoStack undo{project};
    GeometryBuilder builder{project};
    FakeRenderer fake;
    ViewportController controller{project, builder, fake};
    CabinetPropertiesProxy proxy{project, undo, controller};

    Fixture() {
        ensure_qapp();
        auto& cab = project.mutable_cabinet();
        cab.dimensions = {Millimeters{1200}, Millimeters{600}, Millimeters{2000}};
        cab.default_panel_thickness = Millimeters{16};
        cab.default_back_thickness  = Millimeters{4};
    }
};

}  // namespace

TEST(CabinetPropertiesProxyTest, NameReflectsModel) {
    Fixture f;
    EXPECT_EQ(f.proxy.name(), QString("Initial"));
}

TEST(CabinetPropertiesProxyTest, WidthMmReflectsModel) {
    Fixture f;
    EXPECT_EQ(f.proxy.widthMm(), 1200);
    EXPECT_EQ(f.proxy.depthMm(), 600);
    EXPECT_EQ(f.proxy.heightMm(), 2000);
}

TEST(CabinetPropertiesProxyTest, DefaultThicknessesReflectModel) {
    Fixture f;
    EXPECT_EQ(f.proxy.default_panel_thickness_mm(), 16);
    EXPECT_EQ(f.proxy.default_back_thickness_mm(), 4);
}
```

- [ ] **Step 5: Add the test to `tests/ui/CMakeLists.txt`**

Update the executable's source list:

```cmake
add_executable(coupecad_ui_test
    undo_stack_proxy_test.cpp
    cabinet_properties_proxy_test.cpp
)
```

- [ ] **Step 6: Build + run targeted**

```bash
cmake --build --preset default --target coupecad_ui_test
ctest --preset default -R CabinetPropertiesProxyTest --output-on-failure
```

Expected: 3 tests, 3 passed.

- [ ] **Step 7: Full sweep**

Expected: 342 passing (339 + 3 new).

- [ ] **Step 8: Commit**

```bash
git add src/coupecad/ui/cabinet_properties_proxy.h \
        src/coupecad/ui/cabinet_properties_proxy.cpp \
        src/coupecad/ui/CMakeLists.txt \
        tests/ui/cabinet_properties_proxy_test.cpp \
        tests/ui/CMakeLists.txt
git commit -m "feat(ui): CabinetPropertiesProxy — read-only Q_PROPERTYs"
```

## Task 7: CabinetPropertiesProxy — dimension & defaults setters

**Files:**
- Modify: `src/coupecad/ui/cabinet_properties_proxy.cpp`
- Modify: `tests/ui/cabinet_properties_proxy_test.cpp`

- [ ] **Step 1: Append failing tests**

Add to `tests/ui/cabinet_properties_proxy_test.cpp` after the existing tests:

```cpp
#include "coupecad/ui/cabinet_properties_proxy.h"   // already
#include <QSignalSpy>

TEST(CabinetPropertiesProxyTest, SetWidthMmDispatchesCommand) {
    Fixture f;
    f.proxy.setWidthMm(1500);
    EXPECT_EQ(f.project.cabinet().dimensions.width.value(), 1500);
}

TEST(CabinetPropertiesProxyTest, SetWidthMmFiresChangedSignal) {
    Fixture f;
    QSignalSpy spy(&f.proxy, &CabinetPropertiesProxy::changed);
    f.proxy.setWidthMm(1500);
    EXPECT_GE(spy.count(), 1);
}

TEST(CabinetPropertiesProxyTest, SetWidthMmRejectsNonPositive) {
    Fixture f;
    EXPECT_NO_THROW(f.proxy.setWidthMm(0));   // swallowed
    EXPECT_EQ(f.project.cabinet().dimensions.width.value(), 1200);   // unchanged
}

TEST(CabinetPropertiesProxyTest, SetWidthMmNoOpDoesNotDispatch) {
    Fixture f;
    f.proxy.setWidthMm(1200);   // same value
    EXPECT_FALSE(f.undo.can_undo());
}

TEST(CabinetPropertiesProxyTest, UndoRestoresWidth) {
    Fixture f;
    f.proxy.setWidthMm(1500);
    f.undo.undo();
    EXPECT_EQ(f.project.cabinet().dimensions.width.value(), 1200);
    EXPECT_EQ(f.proxy.widthMm(), 1200);
}

TEST(CabinetPropertiesProxyTest, SetDepthMmDispatches) {
    Fixture f;
    f.proxy.setDepthMm(800);
    EXPECT_EQ(f.project.cabinet().dimensions.depth.value(), 800);
}

TEST(CabinetPropertiesProxyTest, SetHeightMmDispatches) {
    Fixture f;
    f.proxy.setHeightMm(2500);
    EXPECT_EQ(f.project.cabinet().dimensions.height.value(), 2500);
}

TEST(CabinetPropertiesProxyTest, SetDefaultPanelThicknessDispatches) {
    Fixture f;
    f.proxy.set_default_panel_thickness_mm(18);
    EXPECT_EQ(f.project.cabinet().default_panel_thickness.value(), 18);
}

TEST(CabinetPropertiesProxyTest, SetDefaultBackThicknessDispatches) {
    Fixture f;
    f.proxy.set_default_back_thickness_mm(6);
    EXPECT_EQ(f.project.cabinet().default_back_thickness.value(), 6);
}
```

- [ ] **Step 2: Replace the stubs in `cabinet_properties_proxy.cpp`**

First add includes near the top after existing includes:

```cpp
#include "coupecad/core/commands/cabinet_commands.h"
```

Replace the 5 stub setters (`setWidthMm`/`setDepthMm`/`setHeightMm`/`set_default_panel_thickness_mm`/`set_default_back_thickness_mm`). Leave `setName` as a stub until Task 8.

```cpp
void CabinetPropertiesProxy::setWidthMm(int mm) {
    const auto& cab = project_.cabinet();
    if (mm == cab.dimensions.width.value()) return;
    auto new_dims = cab.dimensions;
    new_dims.width = core::Millimeters{mm};
    try {
        undo_.execute(std::make_unique<core::SetCabinetDimensions>(
            cab.id, new_dims));
    } catch (const core::DomainError& e) {
        coupecad::logging::Logger::instance().warn(
            "ui", "cabinet.width rejected: {}", e.what());
        emit changed();
    }
}

void CabinetPropertiesProxy::setDepthMm(int mm) {
    const auto& cab = project_.cabinet();
    if (mm == cab.dimensions.depth.value()) return;
    auto new_dims = cab.dimensions;
    new_dims.depth = core::Millimeters{mm};
    try {
        undo_.execute(std::make_unique<core::SetCabinetDimensions>(
            cab.id, new_dims));
    } catch (const core::DomainError& e) {
        coupecad::logging::Logger::instance().warn(
            "ui", "cabinet.depth rejected: {}", e.what());
        emit changed();
    }
}

void CabinetPropertiesProxy::setHeightMm(int mm) {
    const auto& cab = project_.cabinet();
    if (mm == cab.dimensions.height.value()) return;
    auto new_dims = cab.dimensions;
    new_dims.height = core::Millimeters{mm};
    try {
        undo_.execute(std::make_unique<core::SetCabinetDimensions>(
            cab.id, new_dims));
    } catch (const core::DomainError& e) {
        coupecad::logging::Logger::instance().warn(
            "ui", "cabinet.height rejected: {}", e.what());
        emit changed();
    }
}

void CabinetPropertiesProxy::set_default_panel_thickness_mm(int mm) {
    const auto& cab = project_.cabinet();
    if (mm == cab.default_panel_thickness.value()) return;
    try {
        undo_.execute(std::make_unique<core::SetCabinetDefaults>(
            cab.id,
            cab.default_panel_material,
            core::Millimeters{mm},
            cab.default_back_thickness));
    } catch (const core::DomainError& e) {
        coupecad::logging::Logger::instance().warn(
            "ui", "cabinet.default_panel_thickness rejected: {}", e.what());
        emit changed();
    }
}

void CabinetPropertiesProxy::set_default_back_thickness_mm(int mm) {
    const auto& cab = project_.cabinet();
    if (mm == cab.default_back_thickness.value()) return;
    try {
        undo_.execute(std::make_unique<core::SetCabinetDefaults>(
            cab.id,
            cab.default_panel_material,
            cab.default_panel_thickness,
            core::Millimeters{mm}));
    } catch (const core::DomainError& e) {
        coupecad::logging::Logger::instance().warn(
            "ui", "cabinet.default_back_thickness rejected: {}", e.what());
        emit changed();
    }
}
```

Also need to wire the controller's observer to the UndoStack so `cabinetChanged` actually fires. In `Fixture` constructor in the test, after the existing setup, add `undo.add_observer(&controller);`. Update the Fixture:

```cpp
    Fixture() {
        ensure_qapp();
        undo.add_observer(&controller);   // ← add this
        auto& cab = project.mutable_cabinet();
        // ... rest unchanged ...
    }
```

- [ ] **Step 3: Build + run targeted**

```bash
cmake --build --preset default --target coupecad_ui_test
ctest --preset default -R CabinetPropertiesProxyTest --output-on-failure
```

Expected: 12 tests (3 read-only + 9 new), 12 passed.

- [ ] **Step 4: Full sweep**

Expected: 351 passing (342 + 9 new).

- [ ] **Step 5: Commit**

```bash
git add src/coupecad/ui/cabinet_properties_proxy.cpp \
        tests/ui/cabinet_properties_proxy_test.cpp
git commit -m "feat(ui): CabinetPropertiesProxy — dimension + defaults setters"
```

## Task 8: CabinetPropertiesProxy.setName

**Files:**
- Modify: `src/coupecad/ui/cabinet_properties_proxy.cpp`
- Modify: `tests/ui/cabinet_properties_proxy_test.cpp`

- [ ] **Step 1: Append failing test**

```cpp
TEST(CabinetPropertiesProxyTest, SetNameDispatches) {
    Fixture f;
    f.proxy.setName(QString("Renamed"));
    EXPECT_EQ(f.project.cabinet().name, std::string{"Renamed"});
}

TEST(CabinetPropertiesProxyTest, SetNameNoOpForSameValue) {
    Fixture f;
    f.proxy.setName(QString("Initial"));   // same as current
    EXPECT_FALSE(f.undo.can_undo());
}

TEST(CabinetPropertiesProxyTest, SetNameRejectsEmpty) {
    Fixture f;
    EXPECT_NO_THROW(f.proxy.setName(QString("")));   // swallowed
    EXPECT_EQ(f.project.cabinet().name, std::string{"Initial"});
}
```

- [ ] **Step 2: Replace `setName` stub in `cabinet_properties_proxy.cpp`**

```cpp
void CabinetPropertiesProxy::setName(const QString& value) {
    const auto& cab = project_.cabinet();
    const std::string new_name = value.toStdString();
    if (new_name == cab.name) return;
    try {
        undo_.execute(std::make_unique<core::SetCabinetName>(cab.id, new_name));
    } catch (const core::DomainError& e) {
        coupecad::logging::Logger::instance().warn(
            "ui", "cabinet.name rejected: {}", e.what());
        emit changed();
    }
}
```

- [ ] **Step 3: Build + run**

```bash
cmake --build --preset default --target coupecad_ui_test
ctest --preset default -R "CabinetPropertiesProxyTest.SetName" --output-on-failure
```

Expected: 3 new tests passed.

- [ ] **Step 4: Full sweep**

Expected: 354 passing.

- [ ] **Step 5: Commit**

```bash
git add src/coupecad/ui/cabinet_properties_proxy.cpp \
        tests/ui/cabinet_properties_proxy_test.cpp
git commit -m "feat(ui): CabinetPropertiesProxy.setName via SetCabinetName command"
```

## Task 9: PanelPropertiesProxy — read-only state

**Files:**
- Create: `src/coupecad/ui/panel_properties_proxy.h`
- Create: `src/coupecad/ui/panel_properties_proxy.cpp`
- Modify: `src/coupecad/ui/CMakeLists.txt`
- Create: `tests/ui/panel_properties_proxy_test.cpp`
- Modify: `tests/ui/CMakeLists.txt`

> Read-only state first: hasPanel, panelIdString, role, label getter, thickness/material override readback. Setters stub-throw. `role_params_summary` returns a placeholder string; full visitor lands in Task 10b.

> **FakeRenderer note:** The existing FakeRenderer (Stage 4a) only stores selected ids passively in `select()`. To exercise selection scenarios from the proxy, the test calls `fake.select(EntityId{pid})` directly to populate the selection vector. The controller's `selection()` reads back through the renderer, so this works without needing real picking.

- [ ] **Step 1: Create `src/coupecad/ui/panel_properties_proxy.h`**

```cpp
#pragma once

#include "coupecad/core/panel.h"
#include "coupecad/core/project.h"
#include "coupecad/viewport/viewport_controller.h"

#include <QObject>
#include <QString>

namespace coupecad::core { class UndoStack; }

namespace coupecad::ui {

class PanelPropertiesProxy : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool    hasPanel      READ has_panel      NOTIFY changed)
    Q_PROPERTY(QString panelIdString READ panel_id_string NOTIFY changed)
    Q_PROPERTY(QString role          READ role           NOTIFY changed)
    Q_PROPERTY(QString roleParamsSummary
                 READ role_params_summary NOTIFY changed)

    Q_PROPERTY(QString label
                 READ label WRITE setLabel NOTIFY changed)
    Q_PROPERTY(bool    hasThicknessOverride
                 READ has_thickness_override NOTIFY changed)
    Q_PROPERTY(int     thicknessOverrideMm
                 READ thickness_override_mm
                 WRITE set_thickness_override_mm NOTIFY changed)
    Q_PROPERTY(bool    hasMaterialOverride
                 READ has_material_override NOTIFY changed)
    Q_PROPERTY(QString materialOverrideUuid
                 READ material_override_uuid
                 WRITE set_material_override_uuid NOTIFY changed)

public:
    PanelPropertiesProxy(core::Project& project,
                         core::UndoStack& undo,
                         viewport::ViewportController& controller,
                         QObject* parent = nullptr);

    bool    has_panel() const;
    QString panel_id_string() const;
    QString role() const;
    QString role_params_summary() const;
    QString label() const;
    bool    has_thickness_override() const;
    int     thickness_override_mm() const;
    bool    has_material_override() const;
    QString material_override_uuid() const;

    void setLabel(const QString& value);
    void set_thickness_override_mm(int mm);
    void set_material_override_uuid(const QString& uuid_or_empty);

    Q_INVOKABLE void clear_thickness_override();
    Q_INVOKABLE void clear_material_override();

signals:
    void changed();

private slots:
    void on_selection_changed();
    void on_panel_changed(const QString& panel_id);

private:
    const core::Panel* current_panel() const;

    core::Project&                project_;
    core::UndoStack&              undo_;
    viewport::ViewportController& controller_;
};

}  // namespace coupecad::ui
```

- [ ] **Step 2: Create `src/coupecad/ui/panel_properties_proxy.cpp`**

```cpp
#include "coupecad/ui/panel_properties_proxy.h"

#include "coupecad/core/cabinet.h"
#include "coupecad/core/errors.h"
#include "coupecad/core/undo_stack.h"
#include "coupecad/logging/logger.h"

#include <variant>

namespace coupecad::ui {

PanelPropertiesProxy::PanelPropertiesProxy(
    core::Project& project, core::UndoStack& undo,
    viewport::ViewportController& controller, QObject* parent)
    : QObject(parent), project_(project), undo_(undo), controller_(controller) {
    QObject::connect(&controller_, &viewport::ViewportController::selectionChanged,
                     this,         &PanelPropertiesProxy::on_selection_changed);
    QObject::connect(&controller_, &viewport::ViewportController::panelChanged,
                     this,         &PanelPropertiesProxy::on_panel_changed);
    coupecad::logging::Logger::instance().info(
        "ui", "PanelPropertiesProxy constructed");
}

void PanelPropertiesProxy::on_selection_changed() {
    emit changed();
}

void PanelPropertiesProxy::on_panel_changed(const QString& panel_id) {
    const auto* p = current_panel();
    if (p != nullptr && QString::fromStdString(p->id.to_string()) == panel_id) {
        emit changed();
    }
}

const core::Panel* PanelPropertiesProxy::current_panel() const {
    const auto sel = controller_.selection();
    if (sel.size() != 1) return nullptr;
    const auto* pid_ptr = std::get_if<core::PanelId>(&sel[0]);
    if (pid_ptr == nullptr) return nullptr;
    const auto it = project_.cabinet().panels.find(*pid_ptr);
    return it == project_.cabinet().panels.end() ? nullptr : &it->second;
}

bool PanelPropertiesProxy::has_panel() const {
    return current_panel() != nullptr;
}

QString PanelPropertiesProxy::panel_id_string() const {
    const auto* p = current_panel();
    return p == nullptr ? QString{} : QString::fromStdString(p->id.to_string());
}

QString PanelPropertiesProxy::role() const {
    const auto* p = current_panel();
    if (p == nullptr) return QString{};
    return QString::fromUtf8(core::panel_role_name(p->role));
}

QString PanelPropertiesProxy::role_params_summary() const {
    // Full visitor lands in Task 11; placeholder for now.
    return QString{};
}

QString PanelPropertiesProxy::label() const {
    const auto* p = current_panel();
    if (p == nullptr) return QString{};
    return p->label.has_value()
        ? QString::fromStdString(*p->label)
        : QString{};
}

bool PanelPropertiesProxy::has_thickness_override() const {
    const auto* p = current_panel();
    return p != nullptr && p->thickness_override.has_value();
}

int PanelPropertiesProxy::thickness_override_mm() const {
    const auto* p = current_panel();
    if (p == nullptr) return 0;
    if (p->thickness_override.has_value()) {
        return p->thickness_override->value();
    }
    return project_.cabinet().default_panel_thickness.value();
}

bool PanelPropertiesProxy::has_material_override() const {
    const auto* p = current_panel();
    return p != nullptr && p->material_override.has_value();
}

QString PanelPropertiesProxy::material_override_uuid() const {
    const auto* p = current_panel();
    if (p == nullptr || !p->material_override.has_value()) return QString{};
    return QString::fromStdString(p->material_override->to_string());
}

// Setters stubbed; filled in Task 10.
void PanelPropertiesProxy::setLabel(const QString&) {
    throw core::LogicError{"ui.not_implemented_yet", "setLabel in Task 10"};
}
void PanelPropertiesProxy::set_thickness_override_mm(int) {
    throw core::LogicError{"ui.not_implemented_yet", "set_thickness_override_mm in Task 10"};
}
void PanelPropertiesProxy::set_material_override_uuid(const QString&) {
    throw core::LogicError{"ui.not_implemented_yet", "set_material_override_uuid in Task 10"};
}
void PanelPropertiesProxy::clear_thickness_override() {
    throw core::LogicError{"ui.not_implemented_yet", "clear_thickness_override in Task 10"};
}
void PanelPropertiesProxy::clear_material_override() {
    throw core::LogicError{"ui.not_implemented_yet", "clear_material_override in Task 10"};
}

}  // namespace coupecad::ui
```

- [ ] **Step 3: Add source to `src/coupecad/ui/CMakeLists.txt`**

```cmake
add_library(coupecad_ui STATIC
    undo_stack_proxy.cpp
    cabinet_properties_proxy.cpp
    panel_properties_proxy.cpp
)
```

- [ ] **Step 4: Write failing tests `tests/ui/panel_properties_proxy_test.cpp`**

```cpp
#include "coupecad/ui/panel_properties_proxy.h"

#include "coupecad/core/cabinet.h"
#include "coupecad/core/panel.h"
#include "coupecad/core/project.h"
#include "coupecad/core/undo_stack.h"
#include "coupecad/core/units.h"
#include "coupecad/geometry/geometry_builder.h"
#include "coupecad/renderer/entity_id.h"
#include "coupecad/viewport/viewport_controller.h"
#include "fake_renderer.h"

#include <QCoreApplication>

#include <gtest/gtest.h>

namespace {

bool s_qapp_initialised = false;

void ensure_qapp() {
    if (s_qapp_initialised) return;
    static int    argc = 1;
    static char   arg0[] = "panel_proxy_test";
    static char*  argv[] = {arg0, nullptr};
    new QCoreApplication(argc, argv);
    s_qapp_initialised = true;
}

}  // namespace

using coupecad::core::Millimeters;
using coupecad::core::Panel;
using coupecad::core::PanelId;
using coupecad::core::PanelIdTag;
using coupecad::core::PanelRole;
using coupecad::core::Project;
using coupecad::core::UndoStack;
using coupecad::geometry::GeometryBuilder;
using coupecad::renderer::EntityId;
using coupecad::ui::PanelPropertiesProxy;
using coupecad::viewport::ViewportController;
using coupecad::viewport::testing::FakeRenderer;

namespace {

struct Fixture {
    Project project = Project::create_empty("Initial");
    UndoStack undo{project};
    GeometryBuilder builder{project};
    FakeRenderer fake;
    ViewportController controller{project, builder, fake};
    PanelPropertiesProxy proxy{project, undo, controller};

    Fixture() {
        ensure_qapp();
        undo.add_observer(&controller);
        auto& cab = project.mutable_cabinet();
        cab.dimensions = {Millimeters{1200}, Millimeters{600}, Millimeters{2000}};
        cab.default_panel_thickness = Millimeters{16};
    }

    PanelId add_bottom_panel() {
        Panel p;
        p.id = project.uuid_gen().next_id<PanelIdTag>();
        p.role = PanelRole::Bottom;
        project.mutable_cabinet().panels.emplace(p.id, p);
        return p.id;
    }
};

}  // namespace

TEST(PanelPropertiesProxyTest, HasPanelFalseWhenNothingSelected) {
    Fixture f;
    EXPECT_FALSE(f.proxy.has_panel());
}

TEST(PanelPropertiesProxyTest, HasPanelTrueAfterSelectOne) {
    Fixture f;
    const auto pid = f.add_bottom_panel();
    f.fake.select(EntityId{pid});
    EXPECT_TRUE(f.proxy.has_panel());
    EXPECT_EQ(f.proxy.panel_id_string(), QString::fromStdString(pid.to_string()));
}

TEST(PanelPropertiesProxyTest, HasPanelFalseWhenTwoSelected) {
    Fixture f;
    const auto p1 = f.add_bottom_panel();
    const auto p2 = f.add_bottom_panel();
    f.fake.select(EntityId{p1});
    f.fake.select(EntityId{p2});
    EXPECT_FALSE(f.proxy.has_panel());
}

TEST(PanelPropertiesProxyTest, RoleReadbackMatchesPanelRole) {
    Fixture f;
    const auto pid = f.add_bottom_panel();
    f.fake.select(EntityId{pid});
    EXPECT_EQ(f.proxy.role(), QString("Bottom"));
}

TEST(PanelPropertiesProxyTest, LabelReadbackEmptyByDefault) {
    Fixture f;
    const auto pid = f.add_bottom_panel();
    f.fake.select(EntityId{pid});
    EXPECT_EQ(f.proxy.label(), QString{});
}

TEST(PanelPropertiesProxyTest, ThicknessOverrideReadbackUsesDefaultWhenAbsent) {
    Fixture f;
    const auto pid = f.add_bottom_panel();
    f.fake.select(EntityId{pid});
    EXPECT_FALSE(f.proxy.has_thickness_override());
    EXPECT_EQ(f.proxy.thickness_override_mm(), 16);
}

TEST(PanelPropertiesProxyTest, ThicknessOverrideReadbackUsesOverrideWhenSet) {
    Fixture f;
    const auto pid = f.add_bottom_panel();
    f.project.mutable_cabinet().panels.at(pid).thickness_override = Millimeters{18};
    f.fake.select(EntityId{pid});
    EXPECT_TRUE(f.proxy.has_thickness_override());
    EXPECT_EQ(f.proxy.thickness_override_mm(), 18);
}

TEST(PanelPropertiesProxyTest, MaterialOverrideEmptyByDefault) {
    Fixture f;
    const auto pid = f.add_bottom_panel();
    f.fake.select(EntityId{pid});
    EXPECT_FALSE(f.proxy.has_material_override());
    EXPECT_EQ(f.proxy.material_override_uuid(), QString{});
}
```

- [ ] **Step 5: Add test source**

In `tests/ui/CMakeLists.txt`:

```cmake
add_executable(coupecad_ui_test
    undo_stack_proxy_test.cpp
    cabinet_properties_proxy_test.cpp
    panel_properties_proxy_test.cpp
)
```

- [ ] **Step 6: Build + run targeted**

```bash
cmake --build --preset default --target coupecad_ui_test
ctest --preset default -R PanelPropertiesProxyTest --output-on-failure
```

Expected: 8 tests, 8 passed.

- [ ] **Step 7: Full sweep**

Expected: 362 passing.

- [ ] **Step 8: Commit**

```bash
git add src/coupecad/ui/panel_properties_proxy.h \
        src/coupecad/ui/panel_properties_proxy.cpp \
        src/coupecad/ui/CMakeLists.txt \
        tests/ui/panel_properties_proxy_test.cpp \
        tests/ui/CMakeLists.txt
git commit -m "feat(ui): PanelPropertiesProxy — read-only state + observer wiring"
```

## Task 10: PanelPropertiesProxy — setters

**Files:**
- Modify: `src/coupecad/ui/panel_properties_proxy.cpp`
- Modify: `tests/ui/panel_properties_proxy_test.cpp`

- [ ] **Step 1: Append failing tests**

```cpp
TEST(PanelPropertiesProxyTest, SetLabelDispatchesCommand) {
    Fixture f;
    const auto pid = f.add_bottom_panel();
    f.fake.select(EntityId{pid});
    f.proxy.setLabel(QString("My label"));
    EXPECT_EQ(f.project.cabinet().panels.at(pid).label.value_or(""),
              std::string{"My label"});
}

TEST(PanelPropertiesProxyTest, SetLabelEmptyStringClearsLabel) {
    Fixture f;
    const auto pid = f.add_bottom_panel();
    f.project.mutable_cabinet().panels.at(pid).label = std::string{"old"};
    f.fake.select(EntityId{pid});
    f.proxy.setLabel(QString(""));
    EXPECT_FALSE(f.project.cabinet().panels.at(pid).label.has_value());
}

TEST(PanelPropertiesProxyTest, SetThicknessOverrideDispatches) {
    Fixture f;
    const auto pid = f.add_bottom_panel();
    f.fake.select(EntityId{pid});
    f.proxy.set_thickness_override_mm(18);
    EXPECT_EQ(f.project.cabinet().panels.at(pid).thickness_override.value().value(), 18);
}

TEST(PanelPropertiesProxyTest, ClearThicknessOverrideDispatches) {
    Fixture f;
    const auto pid = f.add_bottom_panel();
    f.project.mutable_cabinet().panels.at(pid).thickness_override = Millimeters{18};
    f.fake.select(EntityId{pid});
    f.proxy.clear_thickness_override();
    EXPECT_FALSE(f.project.cabinet().panels.at(pid).thickness_override.has_value());
}

TEST(PanelPropertiesProxyTest, SetMaterialOverrideUuidParsesAndDispatches) {
    Fixture f;
    const auto pid = f.add_bottom_panel();
    f.fake.select(EntityId{pid});
    // Use the existing default material id from project so the command succeeds.
    const auto& default_mat = f.project.cabinet().default_panel_material;
    f.proxy.set_material_override_uuid(
        QString::fromStdString(default_mat.to_string()));
    EXPECT_TRUE(f.project.cabinet().panels.at(pid).material_override.has_value());
    EXPECT_EQ(*f.project.cabinet().panels.at(pid).material_override, default_mat);
}

TEST(PanelPropertiesProxyTest, SetMaterialOverrideEmptyClearsOverride) {
    Fixture f;
    const auto pid = f.add_bottom_panel();
    const auto& default_mat = f.project.cabinet().default_panel_material;
    f.project.mutable_cabinet().panels.at(pid).material_override = default_mat;
    f.fake.select(EntityId{pid});
    f.proxy.set_material_override_uuid(QString{});
    EXPECT_FALSE(f.project.cabinet().panels.at(pid).material_override.has_value());
}

TEST(PanelPropertiesProxyTest, ClearMaterialOverrideDispatches) {
    Fixture f;
    const auto pid = f.add_bottom_panel();
    const auto& default_mat = f.project.cabinet().default_panel_material;
    f.project.mutable_cabinet().panels.at(pid).material_override = default_mat;
    f.fake.select(EntityId{pid});
    f.proxy.clear_material_override();
    EXPECT_FALSE(f.project.cabinet().panels.at(pid).material_override.has_value());
}

TEST(PanelPropertiesProxyTest, SettersAreNoOpWithoutSelection) {
    Fixture f;
    EXPECT_NO_THROW(f.proxy.setLabel(QString("x")));
    EXPECT_NO_THROW(f.proxy.set_thickness_override_mm(20));
    EXPECT_FALSE(f.undo.can_undo());
}
```

- [ ] **Step 2: Replace stubs in `panel_properties_proxy.cpp`**

Add include near the top:

```cpp
#include "coupecad/core/commands/panel_commands.h"
```

Replace the 5 stubs:

```cpp
void PanelPropertiesProxy::setLabel(const QString& value) {
    const auto* p = current_panel();
    if (p == nullptr) return;

    std::optional<std::string> new_label;
    if (!value.isEmpty()) new_label = value.toStdString();
    if (new_label == p->label) return;

    const auto pid = p->id;
    try {
        undo_.execute(std::make_unique<core::SetPanelLabel>(pid, new_label));
    } catch (const core::DomainError& e) {
        coupecad::logging::Logger::instance().warn(
            "ui", "panel.label rejected: {}", e.what());
        emit changed();
    }
}

void PanelPropertiesProxy::set_thickness_override_mm(int mm) {
    const auto* p = current_panel();
    if (p == nullptr) return;
    const auto pid = p->id;
    const std::optional<core::Millimeters> new_value = core::Millimeters{mm};
    if (new_value == p->thickness_override) return;

    try {
        undo_.execute(std::make_unique<core::SetPanelThickness>(pid, new_value));
    } catch (const core::DomainError& e) {
        coupecad::logging::Logger::instance().warn(
            "ui", "panel.thickness rejected: {}", e.what());
        emit changed();
    }
}

void PanelPropertiesProxy::clear_thickness_override() {
    const auto* p = current_panel();
    if (p == nullptr) return;
    if (!p->thickness_override.has_value()) return;

    const auto pid = p->id;
    try {
        undo_.execute(std::make_unique<core::SetPanelThickness>(pid, std::nullopt));
    } catch (const core::DomainError& e) {
        coupecad::logging::Logger::instance().warn(
            "ui", "panel.thickness clear rejected: {}", e.what());
        emit changed();
    }
}

void PanelPropertiesProxy::set_material_override_uuid(const QString& uuid_or_empty) {
    const auto* p = current_panel();
    if (p == nullptr) return;
    const auto pid = p->id;

    std::optional<core::MaterialId> new_value;
    if (!uuid_or_empty.isEmpty()) {
        auto parsed = core::MaterialId::from_string(uuid_or_empty.toStdString());
        if (!parsed.is_valid()) {
            coupecad::logging::Logger::instance().warn(
                "ui", "panel.material_override invalid UUID '{}'",
                uuid_or_empty.toStdString());
            emit changed();
            return;
        }
        new_value = parsed;
    }
    if (new_value == p->material_override) return;

    try {
        undo_.execute(std::make_unique<core::SetPanelMaterial>(pid, new_value));
    } catch (const core::DomainError& e) {
        coupecad::logging::Logger::instance().warn(
            "ui", "panel.material rejected: {}", e.what());
        emit changed();
    }
}

void PanelPropertiesProxy::clear_material_override() {
    const auto* p = current_panel();
    if (p == nullptr) return;
    if (!p->material_override.has_value()) return;
    const auto pid = p->id;
    try {
        undo_.execute(std::make_unique<core::SetPanelMaterial>(pid, std::nullopt));
    } catch (const core::DomainError& e) {
        coupecad::logging::Logger::instance().warn(
            "ui", "panel.material clear rejected: {}", e.what());
        emit changed();
    }
}
```

- [ ] **Step 3: Build + run targeted**

```bash
cmake --build --preset default --target coupecad_ui_test
ctest --preset default -R PanelPropertiesProxyTest --output-on-failure
```

Expected: 16 tests (8 prior + 8 new), 16 passed.

- [ ] **Step 4: Full sweep**

Expected: 370 passing.

- [ ] **Step 5: Commit**

```bash
git add src/coupecad/ui/panel_properties_proxy.cpp \
        tests/ui/panel_properties_proxy_test.cpp
git commit -m "feat(ui): PanelPropertiesProxy — label / thickness / material setters"
```

## Task 11: role_params_summary visitor

**Files:**
- Modify: `src/coupecad/ui/panel_properties_proxy.cpp`
- Modify: `tests/ui/panel_properties_proxy_test.cpp`

- [ ] **Step 1: Append failing tests**

```cpp
TEST(PanelPropertiesProxyTest, RoleParamsSummaryEmptyForBottomPanel) {
    Fixture f;
    const auto pid = f.add_bottom_panel();
    f.fake.select(EntityId{pid});
    EXPECT_EQ(f.proxy.role_params_summary(), QString{});
}

TEST(PanelPropertiesProxyTest, RoleParamsSummaryFormatsShelfFullWidth) {
    Fixture f;
    Panel p;
    p.id = f.project.uuid_gen().next_id<PanelIdTag>();
    p.role = PanelRole::Shelf;
    p.role_params = coupecad::core::ShelfParams{
        .height_from_bottom = Millimeters{800},
        .extent = coupecad::core::ShelfFullWidth{}};
    f.project.mutable_cabinet().panels.emplace(p.id, p);
    f.fake.select(EntityId{p.id});
    EXPECT_EQ(f.proxy.role_params_summary(),
              QString("h=800 mm, full-width"));
}
```

- [ ] **Step 2: Replace `role_params_summary` placeholder with visitor**

In `panel_properties_proxy.cpp`, replace the body of `role_params_summary` with:

```cpp
QString PanelPropertiesProxy::role_params_summary() const {
    const auto* p = current_panel();
    if (p == nullptr) return QString{};

    return std::visit([](const auto& v) -> QString {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, core::NoRoleParams>) {
            return QString{};
        } else if constexpr (std::is_same_v<T, core::ShelfParams>) {
            QString extent_str =
                std::holds_alternative<core::ShelfFullWidth>(v.extent)
                    ? QStringLiteral("full-width")
                    : QStringLiteral("between dividers");
            return QString("h=%1 mm, %2")
                .arg(v.height_from_bottom.value())
                .arg(extent_str);
        } else if constexpr (std::is_same_v<T, core::DividerVerticalParams>) {
            return QString("offset=%1 mm, %2")
                .arg(v.offset_from_left.value())
                .arg(std::holds_alternative<core::VerticalExtentFull>(v.height_extent)
                         ? QStringLiteral("full height")
                         : QStringLiteral("range"));
        } else if constexpr (std::is_same_v<T, core::DividerHorizontalParams>) {
            return QString("offset=%1 mm, %2")
                .arg(v.offset_from_bottom.value())
                .arg(std::holds_alternative<core::DepthExtentFull>(v.depth_extent)
                         ? QStringLiteral("full depth")
                         : QStringLiteral("range"));
        } else {
            return QStringLiteral("(see role-params)");
        }
    }, p->role_params);
}
```

Add include if not already present:

```cpp
#include <type_traits>
```

- [ ] **Step 3: Build + run**

```bash
cmake --build --preset default --target coupecad_ui_test
ctest --preset default -R "PanelPropertiesProxyTest.RoleParamsSummary" --output-on-failure
```

Expected: 2 new tests pass.

- [ ] **Step 4: Full sweep**

Expected: 372 passing.

- [ ] **Step 5: Commit**

```bash
git add src/coupecad/ui/panel_properties_proxy.cpp \
        tests/ui/panel_properties_proxy_test.cpp
git commit -m "feat(ui): role_params_summary visitor for common role variants"
```

## Task 12: main.cpp + Main.qml SplitView wiring

**Files:**
- Modify: `apps/coupecad/main.cpp`
- Modify: `apps/coupecad/qml/Main.qml`
- Modify: `apps/coupecad/CMakeLists.txt`

> Wires the new proxies as QML context properties; promotes Main.qml to a SplitView. The actual inspector content (CabinetPropertiesPanel.qml / PanelPropertiesPanel.qml) lands in Tasks 13–14; for this task, the inspector pane shows a placeholder Label.

- [ ] **Step 1: Replace `apps/coupecad/main.cpp`**

Open the file and add the new includes and proxy construction. Final content:

```cpp
#include "demo_project.h"

#include "coupecad/core/project.h"
#include "coupecad/core/undo_stack.h"
#include "coupecad/geometry/geometry_builder.h"
#include "coupecad/renderer/occt/occt_renderer.h"
#include "coupecad/ui/cabinet_properties_proxy.h"
#include "coupecad/ui/panel_properties_proxy.h"
#include "coupecad/ui/undo_stack_proxy.h"
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

    auto cabinet_props = std::make_unique<coupecad::ui::CabinetPropertiesProxy>(
        *project, *undo, *controller);
    auto panel_props = std::make_unique<coupecad::ui::PanelPropertiesProxy>(
        *project, *undo, *controller);
    auto undo_proxy = std::make_unique<coupecad::ui::UndoStackProxy>(*undo);

    qmlRegisterType<coupecad::viewport::OcctViewportItem>(
        "coupecad", 1, 0, "OcctViewportItem");

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(
        "viewportController", QVariant::fromValue(controller.get()));
    engine.rootContext()->setContextProperty(
        "cabinetProperties",  QVariant::fromValue(cabinet_props.get()));
    engine.rootContext()->setContextProperty(
        "panelProperties",    QVariant::fromValue(panel_props.get()));
    engine.rootContext()->setContextProperty(
        "undoStack",          QVariant::fromValue(undo_proxy.get()));

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

- [ ] **Step 2: Replace `apps/coupecad/qml/Main.qml`**

```qml
import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import coupecad

Window {
    id: root
    width: 1280
    height: 720
    minimumWidth: 800
    minimumHeight: 480
    visible: true
    title: qsTr("CoupeCAD")

    Shortcut { sequence: StandardKey.Undo; onActivated: undoStack.undo() }
    Shortcut { sequence: StandardKey.Redo; onActivated: undoStack.redo() }

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        OcctViewportItem {
            id: viewport
            SplitView.fillWidth: true
            SplitView.minimumWidth: 400
            focus: true
            Component.onCompleted: viewport.set_controller(viewportController)
        }

        Pane {
            SplitView.preferredWidth: 320
            SplitView.minimumWidth: 240
            padding: 12

            ColumnLayout {
                anchors.fill: parent
                spacing: 8

                Label {
                    text: panelProperties.hasPanel
                          ? qsTr("Selected panel")
                          : qsTr("Cabinet")
                    font.pixelSize: 16
                    font.bold: true
                }

                // Inspector content fills in at Tasks 13–14. For now show
                // a placeholder label so the SplitView lays out correctly.
                Label {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: panelProperties.hasPanel
                          ? qsTr("Panel inspector (Task 14)")
                          : qsTr("Cabinet inspector (Task 13)")
                    color: "gray"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}
```

- [ ] **Step 3: Update `apps/coupecad/CMakeLists.txt`**

Add `coupecad_ui` to the link list. Replace the file with:

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
        coupecad_ui
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

- [ ] **Step 4: Build the app**

```bash
cmake --build --preset default --target coupecad
```

Expected: builds clean.

- [ ] **Step 5: Run `--version` smoke**

```bash
./build/default/bin/coupecad.app/Contents/MacOS/coupecad --version 2>&1 || \
  ./build/default/bin/coupecad --version
```

Expected: `CoupeCAD 0.1.0`, exit 0.

- [ ] **Step 6: Full sweep**

```bash
ctest --preset default --output-on-failure
```

Expected: 372 still passing.

- [ ] **Step 7: Commit**

```bash
git add apps/coupecad/main.cpp \
        apps/coupecad/qml/Main.qml \
        apps/coupecad/CMakeLists.txt
git commit -m "feat(app): wire CabinetProperties / PanelProperties / UndoStack proxies + SplitView"
```

## Task 13: CabinetPropertiesPanel.qml

**Files:**
- Create: `apps/coupecad/qml/CabinetPropertiesPanel.qml`
- Modify: `apps/coupecad/qml/Main.qml`
- Modify: `apps/coupecad/CMakeLists.txt` (register the new QML file)

- [ ] **Step 1: Create `apps/coupecad/qml/CabinetPropertiesPanel.qml`**

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property var proxy

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        Label { text: qsTr("Name") }
        TextField {
            Layout.fillWidth: true
            text: proxy.name
            onEditingFinished: proxy.name = text
        }

        Label { text: qsTr("Dimensions (mm)"); Layout.topMargin: 8 }
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            Label  { text: qsTr("Width")  }
            SpinBox {
                Layout.fillWidth: true
                from: 1; to: 100000
                value: proxy.widthMm
                onValueModified: proxy.widthMm = value
            }
            Label  { text: qsTr("Depth")  }
            SpinBox {
                Layout.fillWidth: true
                from: 1; to: 100000
                value: proxy.depthMm
                onValueModified: proxy.depthMm = value
            }
            Label  { text: qsTr("Height") }
            SpinBox {
                Layout.fillWidth: true
                from: 1; to: 100000
                value: proxy.heightMm
                onValueModified: proxy.heightMm = value
            }
        }

        Label { text: qsTr("Defaults"); Layout.topMargin: 8 }
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            Label  { text: qsTr("Panel thickness") }
            SpinBox {
                Layout.fillWidth: true
                from: 1; to: 200
                value: proxy.defaultPanelThicknessMm
                onValueModified: proxy.defaultPanelThicknessMm = value
            }
            Label  { text: qsTr("Back thickness") }
            SpinBox {
                Layout.fillWidth: true
                from: 1; to: 200
                value: proxy.defaultBackThicknessMm
                onValueModified: proxy.defaultBackThicknessMm = value
            }
        }

        Item { Layout.fillHeight: true }   // spacer
    }
}
```

- [ ] **Step 2: Register the QML file in `apps/coupecad/CMakeLists.txt`**

Update the `qt_add_qml_module` block:

```cmake
qt_add_qml_module(coupecad
    URI coupecad
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/CabinetPropertiesPanel.qml
)
```

- [ ] **Step 3: Use it from `apps/coupecad/qml/Main.qml`**

Replace the placeholder Label in the `Pane` with a `Loader` that brings in the inspector when nothing is selected. Replace the existing `Label { ... Loader ... }` part of the inner ColumnLayout with:

```qml
                Loader {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    sourceComponent: panelProperties.hasPanel
                                     ? panelInspectorPlaceholder
                                     : cabinetInspector
                }

                Component {
                    id: cabinetInspector
                    CabinetPropertiesPanel { proxy: cabinetProperties }
                }
                Component {
                    id: panelInspectorPlaceholder
                    Label {
                        text: qsTr("Panel inspector (Task 14)")
                        color: "gray"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
```

(The header `Label` that displays "Cabinet"/"Selected panel" stays untouched.)

- [ ] **Step 4: Build the app**

```bash
cmake --build --preset default --target coupecad
```

Expected: builds clean.

- [ ] **Step 5: Run `--version` smoke**

```bash
./build/default/bin/coupecad.app/Contents/MacOS/coupecad --version 2>&1 || \
  ./build/default/bin/coupecad --version
```

Expected: `CoupeCAD 0.1.0`, exit 0.

- [ ] **Step 6: Full sweep**

```bash
ctest --preset default --output-on-failure
```

Expected: 372 still passing.

- [ ] **Step 7: Commit**

```bash
git add apps/coupecad/qml/CabinetPropertiesPanel.qml \
        apps/coupecad/qml/Main.qml \
        apps/coupecad/CMakeLists.txt
git commit -m "feat(app): CabinetPropertiesPanel.qml — name/dimensions/defaults editor"
```

## Task 14: PanelPropertiesPanel.qml

**Files:**
- Create: `apps/coupecad/qml/PanelPropertiesPanel.qml`
- Modify: `apps/coupecad/qml/Main.qml`
- Modify: `apps/coupecad/CMakeLists.txt`

- [ ] **Step 1: Create `apps/coupecad/qml/PanelPropertiesPanel.qml`**

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property var proxy

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        // Identity (read-only)
        Label { text: qsTr("ID"); font.pixelSize: 10; color: "gray" }
        TextEdit {
            Layout.fillWidth: true
            readOnly: true
            selectByMouse: true
            text: proxy.panelIdString
            font.family: "monospace"
            font.pixelSize: 10
        }

        Label { text: qsTr("Role"); Layout.topMargin: 6 }
        Label { text: proxy.role; font.bold: true }

        Label { text: qsTr("Role params") }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: proxy.roleParamsSummary
            visible: proxy.roleParamsSummary.length > 0
        }

        // Label
        Label { text: qsTr("Label"); Layout.topMargin: 8 }
        TextField {
            Layout.fillWidth: true
            text: proxy.label
            onEditingFinished: proxy.label = text
        }

        // Thickness override
        Label { text: qsTr("Thickness (mm)"); Layout.topMargin: 8 }
        RowLayout {
            Layout.fillWidth: true
            SpinBox {
                id: thicknessSpin
                Layout.fillWidth: true
                from: 1; to: 200
                value: proxy.thicknessOverrideMm
                onValueModified: proxy.thicknessOverrideMm = value
            }
            Button {
                text: qsTr("Use default")
                enabled: proxy.hasThicknessOverride
                onClicked: proxy.clear_thickness_override()
            }
        }
        Label {
            visible: !proxy.hasThicknessOverride
            text: qsTr("(inherited from cabinet)")
            font.italic: true
            color: "gray"
        }

        // Material override
        Label { text: qsTr("Material override (UUID)"); Layout.topMargin: 8 }
        RowLayout {
            Layout.fillWidth: true
            TextField {
                Layout.fillWidth: true
                text: proxy.materialOverrideUuid
                placeholderText: qsTr("(inherited)")
                onEditingFinished: proxy.materialOverrideUuid = text
            }
            Button {
                text: qsTr("Clear")
                enabled: proxy.hasMaterialOverride
                onClicked: proxy.clear_material_override()
            }
        }

        Item { Layout.fillHeight: true }
    }
}
```

- [ ] **Step 2: Register in `apps/coupecad/CMakeLists.txt`**

```cmake
qt_add_qml_module(coupecad
    URI coupecad
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/CabinetPropertiesPanel.qml
        qml/PanelPropertiesPanel.qml
)
```

- [ ] **Step 3: Use it from `apps/coupecad/qml/Main.qml`**

Replace the `panelInspectorPlaceholder` Component (introduced in Task 13) with a real inspector:

```qml
                Component {
                    id: panelInspectorPlaceholder
                    PanelPropertiesPanel { proxy: panelProperties }
                }
```

Rename the Component id from `panelInspectorPlaceholder` to `panelInspector` for clarity:

```qml
                Loader {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    sourceComponent: panelProperties.hasPanel
                                     ? panelInspector
                                     : cabinetInspector
                }

                Component {
                    id: cabinetInspector
                    CabinetPropertiesPanel { proxy: cabinetProperties }
                }
                Component {
                    id: panelInspector
                    PanelPropertiesPanel { proxy: panelProperties }
                }
```

- [ ] **Step 4: Build**

```bash
cmake --build --preset default --target coupecad
```

Expected: builds clean.

- [ ] **Step 5: Full sweep**

```bash
ctest --preset default --output-on-failure
```

Expected: 372 still passing.

- [ ] **Step 6: Manual visual check (macOS dev machine)**

```bash
open ./build/default/bin/coupecad.app
```

Expected:
- Window opens with SplitView (viewport left, inspector right).
- Inspector header reads "Cabinet"; below is the CabinetPropertiesPanel.
- Edit width in SpinBox → cabinet visibly resizes.
- Edit name in TextField, press Enter → name persists.
- RMB-click on a panel → inspector header switches to "Selected panel"; PanelPropertiesPanel appears with UUID, role, etc.
- Edit label, press Enter → label updates.
- Cmd+Z (macOS) undoes the most recent edit. Cmd+Y or Cmd+Shift+Z redoes.
- Setting width=0 → SpinBox reverts (warning logged but no crash).

Document any visual issues but don't try to debug — file as DONE_WITH_CONCERNS.

- [ ] **Step 7: Commit**

```bash
git add apps/coupecad/qml/PanelPropertiesPanel.qml \
        apps/coupecad/qml/Main.qml \
        apps/coupecad/CMakeLists.txt
git commit -m "feat(app): PanelPropertiesPanel.qml — label / thickness / material editor"
```

## Task 15: Final clean rebuild + Qt-linkage hygiene + push + PR

**Files:** none (verification only)

**Working directory:** `/Users/antonshabalin/Projects/CoupeCAD`
**Branch:** `stage-4b-property-panels`

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

Expected: 372 passed, 0 failed.

- [ ] **Step 3: Verify `coupecad_renderer_occt` is still Qt-free**

```bash
nm build/default/src/coupecad/renderer/occt/libcoupecad_renderer_occt.a 2>/dev/null | grep -c " Q[A-Z]" || echo 0
```

Expected: `0`.

- [ ] **Step 4: Verify `coupecad_renderer` is still Qt-free**

```bash
nm build/default/src/coupecad/renderer/libcoupecad_renderer.a 2>/dev/null | grep -c " Q[A-Z]" || echo 0
```

Expected: `0`.

- [ ] **Step 5: Push branch**

```bash
git push -u origin stage-4b-property-panels
```

- [ ] **Step 6: Open PR**

Write the PR body to a temp file then pass via `--body-file`:

```bash
cat > /tmp/stage-4b-pr-body.md <<'EOF'
## Summary

Stage 4b adds an inspector pane to the right of the viewport that lets the user edit Cabinet properties (when nothing is selected) or the selected Panel's properties.

- Spec: `docs/superpowers/specs/2026-05-12-stage-4b-property-panels-design.md`
- Plan: `docs/superpowers/plans/2026-05-12-stage-4b-property-panels.md`
- New `coupecad_ui` static library — `CabinetPropertiesProxy`, `PanelPropertiesProxy`, `UndoStackProxy` (all `QObject`s exposed to QML as context properties).
- `ViewportController` promoted to `QObject` with `selectionChanged`/`cabinetChanged`/`panelChanged` signals.
- Camera precision: sub-mm carry accumulators stop pan/orbit drift at small zoom.
- New core command `SetCabinetName` (apply/revert/validate).
- QML: `SplitView` layout; new `CabinetPropertiesPanel.qml` + `PanelPropertiesPanel.qml`; Cmd/Ctrl+Z/Y bindings.
- +44 tests: ui proxies (29) + camera precision (3) + SetCabinetName (4) + UndoStackProxy (4) — plus +4 from cabinet-commands tests.
- Renderer libs still 0 Qt symbols.

## Test plan

- [ ] CI: Linux build + tests pass
- [ ] CI: Windows build + tests pass
- [ ] All 372 tests green on both platforms
- [ ] No Qt symbols in `coupecad_renderer_occt` / `coupecad_renderer`
- [ ] Manual: SplitView visible, inspector switches between Cabinet/Panel on RMB-pick, Cmd+Z works, width=0 reverts

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF

gh pr create --title "Stage 4b: Property Panels (Cabinet + Panel inspector)" \
    --body-file /tmp/stage-4b-pr-body.md
```

Capture and report the PR URL.

---

## Self-review

Spec coverage (mapping each section of `docs/superpowers/specs/2026-05-12-stage-4b-property-panels-design.md` to tasks):

- §1 Scope → Tasks 1–15 cover all in-scope items.
- §2.1 File layout → Tasks 5–14 create each file in the listed locations.
- §2.2 CMake → Task 5 (ui lib), Task 12 (apps/coupecad).
- §3.1 QObject promotion → Task 2.
- §3.2 Signal emission points → Task 3.
- §3.3 Camera precision → Task 4.
- §3.4 Header inclusion → Task 2.
- §4 CabinetPropertiesProxy → Tasks 6 (read-only), 7 (dim/defaults setters), 8 (setName).
- §4.4 SetCabinetName command → Task 1.
- §5 PanelPropertiesProxy → Tasks 9 (read-only), 10 (setters), 11 (role_params_summary).
- §6 UndoStackProxy → Task 5.
- §7 QML → Tasks 12 (SplitView + main.cpp), 13 (CabinetPropertiesPanel), 14 (PanelPropertiesPanel).
- §8 Testing → Tasks 1, 4, 5, 6, 7, 8, 9, 10, 11 (all proxy + camera tests).
- §9 Logging/errors/DoD → covered across implementation tasks + Task 15 verification.

Type-consistency cross-check:

- `core::Panel.label` is `std::optional<std::string>` (per Stage 1); `PanelPropertiesProxy.setLabel` produces `optional<string>` from QString — consistent (Task 10).
- `core::Panel.thickness_override` is `std::optional<Millimeters>`; setter wraps via `Millimeters{mm}` — consistent (Task 10).
- `core::Panel.material_override` is `std::optional<MaterialId>`; parsed via `MaterialId::from_string` — consistent (Task 10).
- `ShelfParams.extent` is `std::variant<ShelfFullWidth, ShelfBetweenDividers>` — visitor uses `std::holds_alternative<core::ShelfFullWidth>(v.extent)` — consistent with `src/coupecad/core/panel.h:41,44,49` (Task 11).
- `Project::create_empty(name)` returns `Project` by value; demo + `Fixture` construct via `Project::create_empty("...")` — consistent.
- `UndoStack::add_observer(IProjectObserver*)`; controller `ViewportController : public QObject, public core::IProjectObserver` — base-class slot still works (Task 2).
- `ViewportController::selectionChanged` signal connects to `OcctViewportItem::selectionChanged` signal — both `void()` signatures; Qt allows signal-to-signal connections — consistent (Task 3).
- `FakeRenderer::select(EntityId)` appends to internal vector; test fixtures call it directly to set up selection scenarios — consistent (Task 9).

No placeholders. All step code blocks are complete. Test counts trace cleanly from 328 (baseline after Stage 4a merge) → 332 → 335 → 339 → 342 → 351 → 354 → 362 → 370 → 372.
