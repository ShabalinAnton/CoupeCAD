# Stage 4b — Property Panels (Cabinet + Panel inspector) — Design

**Дата:** 2026-05-12
**Статус:** Утверждено для реализации
**Связанные документы:**
- [`2026-05-07-stage-4a-viewport-design.md`](./2026-05-07-stage-4a-viewport-design.md) — viewport (Qt Quick + OCCT)
- [`2026-05-04-stage-3-renderer-design.md`](./2026-05-04-stage-3-renderer-design.md) — `IRenderer` + OCCT-бэкенд
- [`2026-04-20-stage-1-core-domain-design.md`](./2026-04-20-stage-1-core-domain-design.md) — Project/Cabinet/Panel + commands + UndoStack

---

## 1. Цель и объём Stage 4b

Stage 4b добавляет **inspector-панель справа** в окно приложения, привязанную к выделению из viewport (Stage 4a). Без неё выделение в Stage 4a — visual-only (просто подсветка). Stage 4b делает выделение и редактирование cabinet/panel-свойств работающим end-to-end: пользователь меняет ширину шкафа в SpinBox → команда применяется через UndoStack → renderer перестраивается → Ctrl+Z откатывает.

### 1.1 В объёме

- **Новый модуль `src/coupecad/ui/`** (Qt-линкуемая статическая библиотека `coupecad_ui`).
- **`CabinetPropertiesProxy : QObject`** — read/write proxy для `core::Cabinet`. Q_PROPERTYs: name, widthMm, depthMm, heightMm, defaultPanelThicknessMm, defaultBackThicknessMm. Setters диспатчат `SetCabinetDimensions` / `SetCabinetDefaults` / новый `SetCabinetName` через `UndoStack`.
- **`PanelPropertiesProxy : QObject`** — proxy для *единственной* выделенной панели. Read-only: hasPanel, panelIdString, role, roleParamsSummary. Read-write: label, thicknessOverrideMm (+ `hasThicknessOverride`), materialOverrideUuid (+ `hasMaterialOverride`). Q_INVOKABLE: `clear_thickness_override()`, `clear_material_override()`. Диспатчат `SetPanelLabel`/`SetPanelThickness`/`SetPanelMaterial`.
- **`UndoStackProxy : QObject`** — тонкая Qt-обёртка над `core::UndoStack`, чтобы QML `Shortcut` мог вызывать undo/redo. `core::UndoStack` остаётся Qt-free.
- **Промоушн `ViewportController` в `QObject`** — добавляются сигналы `selectionChanged()`, `cabinetChanged()`, `panelChanged(QString panel_id)`. Сигналы испускаются из `on_changed()` (IProjectObserver) и из `on_mouse_release()` (RMB-pick). Прокси-классы подписываются на эти сигналы.
- **Camera precision fix** в `ViewportController` — накапливаем sub-mm дельты pan/orbit в `double` carry-полях, округляем только при вызове `renderer.set_camera`. Покрытие: 2 теста на стабильность мелких drag-шагов.
- **Новый core-command `SetCabinetName`** — паттерн `SetCabinetDefaults`. Тесты apply/revert/validate.
- **QML:** `Main.qml` переходит на `SplitView` (viewport слева, inspector справа). Новые `CabinetPropertiesPanel.qml` и `PanelPropertiesPanel.qml`. Cmd/Ctrl+Z / Cmd/Ctrl+Y bindings через `Shortcut`.
- **Тесты:** `tests/ui/` — модульные тесты proxy-классов через реальный `Project + UndoStack + FakeRenderer + ViewportController`.

### 1.2 Вне объёма

- **Materials catalog editor.** В Stage 4b `materialOverrideUuid` — это просто TextField с UUID-строкой. Material picker (выпадающий список) — Stage 4c.
- **Hardware list / editor** — Stage 4c+.
- **Edge banding editor** (4 стороны × material + thickness) — Stage 4c+.
- **Role-params editing.** В 4b `roleParamsSummary` — read-only компактная строка. Редактирование role/role_params требует UI для switch'а между вариантами `ShelfParams`/`DividerParams`/`FacadeParams`/`CustomParams`/... Slated для Stage 4c+.
- **File menu (.ccad open/save)** — Stage 4c.
- **Undo/Redo buttons** — keyboard shortcuts только в 4b; toolbar — позже.
- **Multi-select editing** — когда выбрано 2+ панели, inspector показывает заглушку или скрывается. Editing для группы — Stage 5+.
- **`PreviewableCommand` для SpinBox-live-preview** — каждый SpinBox-edit пушит дискретную команду; быстрая прокрутка засоряет undo-стек. Интеграция с `begin_preview`/`commit_preview` (Stage 1b) — лучше в Stage 4c, когда появятся слайдеры.

### 1.3 Подзадача 4b внутри Stage 4

После 4a (viewport) и 4b (panels properties) остаются:
- **Stage 4c** — material picker, edge banding editor, hardware list, role-params editing, `.ccad` open/save, undo/redo toolbar.
- **Stage 4d (опц.)** — toolbar/status bar полировка, drag-live-preview через `PreviewableCommand`.

---

## 2. Структура модуля

### 2.1 Файлы и расположение

```
src/coupecad/ui/
    CMakeLists.txt
    cabinet_properties_proxy.{h,cpp}
    panel_properties_proxy.{h,cpp}
    undo_stack_proxy.{h,cpp}

src/coupecad/viewport/
    viewport_controller.{h,cpp}    # ★ promoted to QObject + camera precision fix

src/coupecad/core/commands/
    cabinet_commands.{h,cpp}       # +SetCabinetName

apps/coupecad/
    main.cpp                       # +register proxies, expose UndoStackProxy
    qml/
        Main.qml                   # +SplitView + inspector
        CabinetPropertiesPanel.qml # new
        PanelPropertiesPanel.qml   # new

tests/ui/
    CMakeLists.txt
    cabinet_properties_proxy_test.cpp
    panel_properties_proxy_test.cpp
    undo_stack_proxy_test.cpp

tests/viewport/
    viewport_controller_camera_precision_test.cpp   # new

tests/core/commands/
    cabinet_commands_test.cpp      # +SetCabinetName tests
```

### 2.2 CMake-таргеты

`src/coupecad/ui/CMakeLists.txt`:

```cmake
find_package(Qt6 6.5 REQUIRED COMPONENTS Core)

add_library(coupecad_ui STATIC
    cabinet_properties_proxy.cpp
    panel_properties_proxy.cpp
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

`src/CMakeLists.txt` — добавить `add_subdirectory(coupecad/ui)` после viewport. `apps/coupecad/CMakeLists.txt` — добавить в link `coupecad_ui`.

### 2.3 Зависимости (направленный граф)

```
coupecad_ui          →  coupecad_core
                     →  coupecad_viewport
                     →  coupecad_logging
                     →  Qt6::Core
coupecad_viewport    →  Qt6::Core/Gui/Quick/OpenGL  (уже было)
                     →  coupecad_renderer + coupecad_renderer_occt
                     →  coupecad_core
apps/coupecad        →  coupecad_ui
                     →  coupecad_viewport
                     →  Qt6::Qml/Quick + QtQuick.Controls/Layouts (новое для SplitView)
```

`coupecad_core` остаётся Qt-free (new `SetCabinetName` command не тащит Qt).
`coupecad_renderer_occt` и `coupecad_renderer` тоже остаются 0 Qt symbols — проверяется в финальной задаче плана.

---

## 3. ViewportController QObject promotion + camera precision fix

### 3.1 QObject changes

```cpp
class ViewportController : public QObject, public core::IProjectObserver {
    Q_OBJECT
public:
    ViewportController(core::Project& project,
                       geometry::GeometryBuilder& builder,
                       renderer::IRenderer& renderer,
                       QObject* parent = nullptr);
    // ... existing public methods unchanged ...

signals:
    void selectionChanged();
    void cabinetChanged();
    void panelChanged(const QString& panel_id_str);
};
```

Existing `OcctViewportItem::selectionChanged` signal (declared in Stage 4a `occt_viewport_item.h`) remains, but its emission moves: instead of `mousePressEvent`/`mouseReleaseEvent` doing pre/post comparison, `set_controller(c)` connects `controller_->selectionChanged → this->selectionChanged`. Removes the count-compare logic.

### 3.2 Where signals fire

- `on_changed(cs)`:
  - if `cs.cabinet_changed` → emit `cabinetChanged()`
  - for each `id` in `cs.updated_panels` → emit `panelChanged(QString::fromStdString(id.to_string()))`
  - if any selected entity is in `cs.removed_panels`/`cs.removed_hardware`, the AIS-side selection drops automatically (verified Stage 3); also emit `selectionChanged()`
- `on_mouse_release` (RMB-pick path): existing pre/post `selection_count()` compare moves *inside* the controller; emit `selectionChanged()` if changed (already done implicitly by the OcctViewportItem in 4a — moves here for consistency).

### 3.3 Camera precision

New private state in `ViewportController`:

```cpp
private:
    // Sub-mm accumulators. Reset to 0 whenever set_camera is invoked
    // from outside (fit_all, programmatic camera, etc.) — otherwise the
    // carry is rolled into the next pan/orbit step.
    double eye_carry_x_ = 0.0;
    double eye_carry_y_ = 0.0;
    double eye_carry_z_ = 0.0;
    double target_carry_x_ = 0.0;
    double target_carry_y_ = 0.0;
    double target_carry_z_ = 0.0;
```

Pan-step algorithm changes:
1. Read current camera from renderer (ints).
2. `desired_eye_x_double = current_int + eye_carry_x_ + world_dx;`
3. `new_eye_x_mm = std::lround(desired_eye_x_double);`
4. `eye_carry_x_ = desired_eye_x_double - static_cast<double>(new_eye_x_mm);`
5. Write `new_eye_x_mm` to the camera. Same for y, z and for target.

Orbit and wheel zoom: identical pattern. Each operation that goes through `renderer_.set_camera` flushes accumulator (carry → next iteration).

Reset hook: `fit_all()` zeros all six carries before calling `renderer_.fit_all()`. Similarly any future programmatic `set_camera(...)` API would zero them — we won't add such an API in 4b.

Tests (`viewport_controller_camera_precision_test.cpp`):

- `SmallPanStepsAccumulate` — 1000× `mmb_drag` of 1px each at large zoom (dist = 100mm so 1px → ~0.2mm, would round to 0 without carry); after the loop, eye-target offset should still match initial (pan invariant), but eye + target should have moved by ~200mm cumulative.
- `WheelZoomNoDriftOnNoOp` — `on_wheel(0, 0, 0.0)` × 50 times must leave camera bit-exact identical (`factor = 1.0` after `pow(1.1, 0)`); regression for any accidental NaN/round-trip leak.

### 3.4 Header inclusion impact

`viewport_controller.h` now includes `<QObject>`. `coupecad_viewport` already linked Qt6::Core since Stage 4a, so this is transparent to consumers. Tests under `tests/viewport/` already link `coupecad_viewport` and through it Qt6::Core — no new test linkage needed.

---

## 4. CabinetPropertiesProxy

### 4.1 Header (`src/coupecad/ui/cabinet_properties_proxy.h`)

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

### 4.2 Setter pattern (e.g., `setWidthMm`)

```cpp
void CabinetPropertiesProxy::setWidthMm(int mm) {
    const auto& cab = project_.cabinet();
    if (mm == cab.dimensions.width.value()) return;  // no-op, avoid binding loop

    auto new_dims = cab.dimensions;
    new_dims.width = core::Millimeters{mm};

    try {
        undo_.execute(std::make_unique<core::SetCabinetDimensions>(
            cab.id, new_dims));
    } catch (const core::DomainError& e) {
        coupecad::logging::Logger::instance().warn(
            "ui", "cabinet.width rejected: {}", e.what());
        emit changed();  // re-read model; revert SpinBox
    }
}
```

`SetCabinetDimensions` (Stage 1b) calls `Cabinet::validate()` inside `apply()`, which throws `DomainError{"cabinet.nonpositive_dimensions", ...}` for ≤ 0 values. We catch, log a warning, and `emit changed()` to make QML bindings re-pull the real value (effectively reverting the SpinBox).

### 4.3 Connection wiring (ctor)

```cpp
CabinetPropertiesProxy::CabinetPropertiesProxy(
    core::Project& project, core::UndoStack& undo,
    viewport::ViewportController& controller, QObject* parent)
    : QObject(parent), project_(project), undo_(undo), controller_(controller) {
    QObject::connect(&controller_, &viewport::ViewportController::cabinetChanged,
                     this,         &CabinetPropertiesProxy::on_cabinet_changed);
}

void CabinetPropertiesProxy::on_cabinet_changed() {
    emit changed();
}
```

QML bindings see `changed()` and re-evaluate.

### 4.4 New `SetCabinetName` command

`src/coupecad/core/commands/cabinet_commands.h` gets a new class adjacent to `SetCabinetDefaults`:

```cpp
class SetCabinetName : public Command {
public:
    SetCabinetName(CabinetId target, std::string new_name);
    ChangeSet apply(Project& project) override;
    ChangeSet revert(Project& project) override;
    std::string_view label() const noexcept override { return "Set cabinet name"; }
    CommandKind kind() const noexcept override { return CommandKind::Discrete; }
private:
    CabinetId   target_;
    std::string new_name_;
    std::optional<std::string> previous_name_;   // captured on first apply
};
```

`apply()` validates target_id matches current cabinet, captures previous name, mutates. `revert()` restores. Returns `ChangeSet{.cabinet_changed = true}` so observers re-read.

Tests in `cabinet_commands_test.cpp`:
- `SetCabinetName_AppliesAndReverts`
- `SetCabinetName_RejectsEmptyName` (validate — analog `cabinet.empty_name` code)
- `SetCabinetName_WrongIdThrowsDomainError`
- `SetCabinetName_RevertWithoutApplyThrowsLogicError` (optional — match existing command pattern)

---

## 5. PanelPropertiesProxy

### 5.1 Header (`src/coupecad/ui/panel_properties_proxy.h`)

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
    core::PanelId       currently_tracked_id() const;   // invalid if !has_panel
    const core::Panel*  current_panel() const;          // nullptr if 0 or 2+
    void                emit_changed();

    core::Project&                project_;
    core::UndoStack&              undo_;
    viewport::ViewportController& controller_;
};

}  // namespace coupecad::ui
```

### 5.2 `current_panel()` logic

```cpp
const core::Panel* PanelPropertiesProxy::current_panel() const {
    const auto sel = controller_.selection();
    if (sel.size() != 1) return nullptr;
    const auto* pid_ptr = std::get_if<core::PanelId>(&sel[0]);
    if (pid_ptr == nullptr) return nullptr;   // hardware item, not panel
    const auto it = project_.cabinet().panels.find(*pid_ptr);
    return it == project_.cabinet().panels.end() ? nullptr : &it->second;
}
```

All getters check `current_panel()` first; if nullptr, return defaults (`hasPanel=false`, empty strings, 0, false).

### 5.3 `role_params_summary` format

Visited via `std::visit` over `RoleParams`. Compact human-readable summary, e.g.:
- `NoRoleParams{}` → `""` (or `"-"`)
- `ShelfParams{height=800, FullWidth}` → `"h=800 mm, full-width"`
- `ShelfParams{height=800, BetweenDividers{a, b}}` → `"h=800 mm, between dividers"`
- `DividerVerticalParams{offset=600, Full}` → `"offset=600 mm, full height"`
- `FacadeParams{Full, Left}` → `"full front, hinge left"`
- `CustomParams{pos, size, rot}` → `"custom: pos=(0,0,0), size=(1200×16×600)"`

Implementation in `.cpp`, ~40 lines. Pure read-only formatting; no allocation hotpath since called only on selection change.

### 5.4 Edit dispatchers

```cpp
void PanelPropertiesProxy::setLabel(const QString& value) {
    const auto* p = current_panel();
    if (p == nullptr) return;

    std::optional<std::string> new_label;
    if (!value.isEmpty()) new_label = value.toStdString();

    if (new_label == p->label) return;   // no-op

    try {
        undo_.execute(std::make_unique<core::SetPanelLabel>(p->id, new_label));
    } catch (const core::DomainError& e) {
        coupecad::logging::Logger::instance().warn(
            "ui", "panel.label rejected: {}", e.what());
        emit changed();
    }
}
```

Symmetric for `set_thickness_override_mm` (dispatches `SetPanelThickness` with `Millimeters` or `nullopt`), `set_material_override_uuid` (parses UUID via `core::MaterialId::from_string`, swallows invalid).

### 5.5 Connection wiring (ctor)

```cpp
PanelPropertiesProxy::PanelPropertiesProxy(...)
    : QObject(parent), project_(project), undo_(undo), controller_(controller) {
    QObject::connect(&controller_, &viewport::ViewportController::selectionChanged,
                     this,         &PanelPropertiesProxy::on_selection_changed);
    QObject::connect(&controller_, &viewport::ViewportController::panelChanged,
                     this,         &PanelPropertiesProxy::on_panel_changed);
}

void PanelPropertiesProxy::on_selection_changed() {
    emit changed();   // full refresh; selection may have moved to a new panel
}

void PanelPropertiesProxy::on_panel_changed(const QString& panel_id) {
    const auto* p = current_panel();
    if (p != nullptr && QString::fromStdString(p->id.to_string()) == panel_id) {
        emit changed();
    }
    // else: not our panel, no refresh
}
```

---

## 6. UndoStackProxy

### 6.1 Header (`src/coupecad/ui/undo_stack_proxy.h`)

```cpp
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace coupecad::core { class UndoStack; }

namespace coupecad::ui {

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

### 6.2 Implementation

`undo()` calls `undo_.undo()`, then `emit changed()`. Same for `redo()`. `can_undo()` / `can_redo()` query the stack directly.

**Limitation:** `UndoStackProxy::changed` only fires when undo/redo are *called*. It does *not* react to external mutations (e.g., the user clicks "Edit width" → `undo_.execute(cmd)`; `canUndo` becomes true, but the proxy has no signal yet). For Stage 4b this is acceptable because the keyboard shortcuts are bound regardless; QML doesn't currently surface `canUndo`/`canRedo` to disable buttons. Stage 4c (toolbar) will need a richer signaling story — likely via a new `core::UndoStack::add_state_observer` callback. Tracked in §9 open questions.

---

## 7. QML

### 7.1 `apps/coupecad/qml/Main.qml` (full new content)

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
            }
        }
    }
}
```

### 7.2 `apps/coupecad/qml/CabinetPropertiesPanel.qml`

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property var proxy   // CabinetPropertiesProxy from context

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        // Name
        Label { text: qsTr("Name") }
        TextField {
            Layout.fillWidth: true
            text: proxy.name
            onEditingFinished: proxy.name = text
        }

        // Dimensions
        Label { text: qsTr("Dimensions (mm)") }
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            Label { text: qsTr("Width")  }
            SpinBox { from: 1; to: 100000; value: proxy.widthMm;  onValueModified: proxy.widthMm  = value }
            Label { text: qsTr("Depth")  }
            SpinBox { from: 1; to: 100000; value: proxy.depthMm;  onValueModified: proxy.depthMm  = value }
            Label { text: qsTr("Height") }
            SpinBox { from: 1; to: 100000; value: proxy.heightMm; onValueModified: proxy.heightMm = value }
        }

        // Defaults
        Label { text: qsTr("Defaults") }
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            Label { text: qsTr("Panel thickness") }
            SpinBox { from: 1; to: 200; value: proxy.defaultPanelThicknessMm;
                      onValueModified: proxy.defaultPanelThicknessMm = value }
            Label { text: qsTr("Back thickness") }
            SpinBox { from: 1; to: 200; value: proxy.defaultBackThicknessMm;
                      onValueModified: proxy.defaultBackThicknessMm = value }
        }

        Item { Layout.fillHeight: true }   // spacer
    }
}
```

### 7.3 `apps/coupecad/qml/PanelPropertiesPanel.qml`

Similar structure. Sections: Identity (UUID, role, role-params summary — all read-only) → Label → Thickness override (SpinBox + "Use default" button) → Material override (TextField + "Use default" button).

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

        // Identity
        Label { text: qsTr("ID")
                font.pixelSize: 10; color: "gray" }
        TextEdit {
            Layout.fillWidth: true
            readOnly: true
            selectByMouse: true
            text: proxy.panelIdString
            font.family: "monospace"
            font.pixelSize: 10
        }
        Label { text: qsTr("Role") }
        Label { text: proxy.role; font.bold: true }
        Label { text: qsTr("Role params") }
        Label { Layout.fillWidth: true; wrapMode: Text.WordWrap
                text: proxy.roleParamsSummary }

        // Label
        Label { text: qsTr("Label") }
        TextField {
            Layout.fillWidth: true
            text: proxy.label
            onEditingFinished: proxy.label = text
        }

        // Thickness override
        Label { text: qsTr("Thickness (mm)") }
        RowLayout {
            Layout.fillWidth: true
            SpinBox {
                Layout.fillWidth: true
                from: 1; to: 200
                value: proxy.thicknessOverrideMm
                enabled: proxy.hasThicknessOverride || true   // always editable
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
            font.italic: true; color: "gray"
        }

        // Material override
        Label { text: qsTr("Material override (UUID)") }
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

`qt_add_qml_module(coupecad ...)` in `apps/coupecad/CMakeLists.txt` registers both new `.qml` files as part of the `coupecad` QML module:

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

### 7.4 `apps/coupecad/main.cpp` additions

After the existing controller construction:

```cpp
#include "coupecad/ui/cabinet_properties_proxy.h"
#include "coupecad/ui/panel_properties_proxy.h"
#include "coupecad/ui/undo_stack_proxy.h"

// ... existing data-layer setup ...

auto cabinet_props = std::make_unique<coupecad::ui::CabinetPropertiesProxy>(
    *project, *undo, *controller);
auto panel_props = std::make_unique<coupecad::ui::PanelPropertiesProxy>(
    *project, *undo, *controller);
auto undo_proxy = std::make_unique<coupecad::ui::UndoStackProxy>(*undo);

// ... existing qmlRegisterType(OcctViewportItem) ...

QQmlApplicationEngine engine;
engine.rootContext()->setContextProperty(
    "viewportController", QVariant::fromValue(controller.get()));
engine.rootContext()->setContextProperty(
    "cabinetProperties", QVariant::fromValue(cabinet_props.get()));
engine.rootContext()->setContextProperty(
    "panelProperties",   QVariant::fromValue(panel_props.get()));
engine.rootContext()->setContextProperty(
    "undoStack",         QVariant::fromValue(undo_proxy.get()));
```

`apps/coupecad/CMakeLists.txt` adds `coupecad_ui` to `target_link_libraries`.

---

## 8. Тестирование

### 8.1 `tests/ui/cabinet_properties_proxy_test.cpp`

Tests use real `Project + UndoStack + GeometryBuilder + FakeRenderer + ViewportController`. Hookup pattern:

```cpp
struct Fixture {
    Project project = Project::create_empty("test");
    UndoStack undo{project};
    GeometryBuilder builder{project};
    FakeRenderer fake;
    ViewportController controller{project, builder, fake};
    CabinetPropertiesProxy proxy{project, undo, controller};

    Fixture() {
        undo.add_observer(&controller);
        auto& cab = project.mutable_cabinet();
        cab.dimensions = {Millimeters{1200}, Millimeters{600}, Millimeters{2000}};
    }
};
```

Cases:
- `WidthMmReflectsModel` — initial value matches cabinet dimensions
- `SetWidthMmDispatchesCommand` — `proxy.setWidthMm(1500)` → renderer received `cabinet_changed` ChangeSet; cabinet width is 1500
- `SetWidthMmFiresChangedSignal` — use `QSignalSpy` on `changed()`; verify ≥1 emission after setter
- `SetWidthMmRejectsNonPositive` — `setWidthMm(0)` → no model change; `changed()` still fires (revert path); warn-log captured
- `UndoRestoresWidth` — set, then `undo.undo()` → width back to original; `cabinetChanged` from controller fires; proxy reads back
- Repeat for `setName` (new `SetCabinetName` command), `setDepthMm`, `setHeightMm`, `set_default_panel_thickness_mm`, `set_default_back_thickness_mm` — abbreviated `Smoke` tests (write + read-back).

Test count: 6 detailed (width) + 5 smoke (other setters) = ~11.

### 8.2 `tests/ui/panel_properties_proxy_test.cpp`

Cases:
- `HasPanelFalseWhenNothingSelected`
- `HasPanelTrueAfterSelectOne` — manually populate one panel, call `controller.select(EntityId{pid})` (via the existing IRenderer pathway through FakeRenderer; need to make FakeRenderer's `select` actually update `selection_`), trigger `selectionChanged` via `controller`, expect `proxy.has_panel()` true
- `HasPanelFalseWhenTwoSelected`
- `LabelReadbackMatchesPanelLabel`
- `SetLabelDispatchesSetPanelLabelCommand`
- `SetLabelEmptyStringClearsLabel` (nullopt path)
- `SetThicknessOverrideMmDispatchesCommand`
- `ClearThicknessOverrideDispatchesCommand`
- `RoleStringMatchesPanelRoleName`
- `RoleParamsSummaryFormatsShelfParamsCorrectly`
- `ChangedSignalFiresOnSelectionChange`
- `ChangedSignalFiresOnExternalPanelMutation` — externally `undo_.execute(SetPanelLabel(...))`; via observer the controller emits `panelChanged`; proxy filters by id and fires `changed`.

Total: ~12 tests.

### 8.3 `tests/ui/undo_stack_proxy_test.cpp`

Cases (~4):
- `CanUndoFalseInitially` / `CanRedoFalseInitially`
- `UndoMethodDelegatesToCoreStack` — push a command, call `proxy.undo()`, model reverts
- `RedoMethodDelegatesToCoreStack`
- `UndoEmitsChanged`

### 8.4 `tests/core/commands/cabinet_commands_test.cpp` (+SetCabinetName)

Cases (~4):
- `SetCabinetName_AppliesAndStoresPrevious`
- `SetCabinetName_RevertRestores`
- `SetCabinetName_RejectsEmpty` — `cabinet.empty_name` code (or whatever convention exists; mirror `hardware_spec.empty_name` from Stage 1)
- `SetCabinetName_WrongIdThrows`

### 8.5 `tests/viewport/viewport_controller_camera_precision_test.cpp` (+2)

- `SmallPanStepsAccumulate` — 1000× 1px MMB drag at small distance; verify eye delta ~analytical-sum within ±2 mm
- `WheelZoomNoDriftOnNoOp` — 50× `on_wheel(0,0,0)` leaves camera bit-exact identical

### 8.6 `tests/ui/CMakeLists.txt`

```cmake
add_executable(coupecad_ui_test
    cabinet_properties_proxy_test.cpp
    panel_properties_proxy_test.cpp
    undo_stack_proxy_test.cpp
)

target_link_libraries(coupecad_ui_test
    PRIVATE
        coupecad_ui
        coupecad_viewport
        coupecad_core
        coupecad_geometry
        coupecad_renderer
        coupecad_logging
        GTest::gtest
        GTest::gtest_main
        Qt6::Core    # needed for QSignalSpy
        Qt6::Test    # for QSignalSpy
)

target_include_directories(coupecad_ui_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests/viewport   # reuse FakeRenderer
)

# Preserve gtest-include-order workaround for macOS Homebrew gtest clash.
get_target_property(_gtest_inc GTest::gtest INTERFACE_INCLUDE_DIRECTORIES)
if(_gtest_inc)
    target_include_directories(coupecad_ui_test BEFORE PRIVATE ${_gtest_inc})
endif()

include(GoogleTest)
gtest_discover_tests(coupecad_ui_test)
```

`tests/CMakeLists.txt` += `add_subdirectory(ui)`.

`Qt6::Test` is a Conan-provided or aqtinstall component. Verify availability during plan exec; if missing, write a tiny custom-signal-spy helper instead.

### 8.7 Coverage target

`coupecad_ui` ≥ 65% lines (proxies are mostly delegation; coverage measures conditional branches in setters).

---

## 9. Logging, errors, DoD, open questions

### 9.1 Logging (category `"ui"`)

- `Info`: each proxy ctor (one-line, includes target ids for diagnosability).
- `Debug`: each successful setter call ("cabinet.width set 1200→1500", "panel <id> label cleared").
- `Trace`: `selectionChanged` / `panelChanged` slot invocations.
- `Warn`: swallowed `DomainError` from validation, with full error code and message.

### 9.2 Error codes

No new codes introduced. Reused:
- `cabinet.nonpositive_dimensions`
- `cabinet.nonpositive_panel_thickness`
- `cabinet.nonpositive_back_thickness`
- `panel.nonpositive_thickness`
- `project.panel_material_missing`
- New: `cabinet.empty_name` (for `SetCabinetName` validation; lowercase dot-separated per convention)

### 9.3 Definition of Done

- [ ] `coupecad_ui` builds on Linux + Windows CI
- [ ] App launches; window is a SplitView with viewport+inspector
- [ ] No panel selected → CabinetPropertiesPanel visible with editable fields
- [ ] RMB-click a panel → switches to PanelPropertiesPanel; UUID/role/role-params shown
- [ ] Click empty viewport → reverts to CabinetPropertiesPanel
- [ ] Edit cabinet width via SpinBox → renderer reflects new dimensions
- [ ] Edit panel label → reflected
- [ ] Change panel thickness override → renderer reflects
- [ ] Cmd/Ctrl+Z undoes; Cmd/Ctrl+Y redoes
- [ ] Setting width=0 silently reverts SpinBox (no crash, warn log)
- [ ] Camera-precision tests pass (1000 small-step pan accumulates correctly)
- [ ] All viewport tests still pass
- [ ] No regressions in 328 existing tests (after stage-4a merge baseline)
- [ ] `coupecad_renderer_occt` and `coupecad_renderer` still have 0 Qt symbols
- [ ] Coverage `coupecad_ui` ≥ 65% lines (best-effort, warn-only)

### 9.4 Открытые вопросы / риски

1. **`UndoStackProxy::changed` signal coverage.** Today fires only when undo/redo invoked through the proxy. External `undo_.execute(cmd)` (called from CabinetPropertiesProxy / PanelPropertiesProxy setters) doesn't notify the undo proxy → `canUndo`/`canRedo` Q_PROPERTYs go stale. Stage 4b: keyboard shortcuts work regardless because they ignore `canUndo`. Stage 4c (toolbar) will need `core::UndoStack::add_state_observer` or similar. Tracked here, not fixed now.
2. **`Qt6::Test` availability.** Conan provides it as part of the `qt/*` package; aqtinstall has it as a module. Verify in Task 1 of the plan.
3. **`PreviewableCommand` integration.** SpinBox value-change events push a `Discrete` command each time. Rapidly clicking the SpinBox arrow keys floods undo stack (10 entries for 10 ticks). Stage 4b accepts this; Stage 4c integrates `begin_preview`/`commit_preview` cycle (Stage 1b API) tied to the SpinBox's `pressed`/`released` signals.
4. **`material_override_uuid` accepts any string.** Stage 4c material picker replaces with a dropdown. Until then a typo throws + swallows + reverts.
5. **No multi-select editing UI.** Inspector toggles by `panelProperties.hasPanel`, which is false for 0 or 2+ selected. Could show placeholder "Select a single panel to edit" — defer to 4c polish.
6. **Connection ownership.** `controller_->signal → proxy->slot` uses default direct connection. If the controller signal fires from a non-UI thread (it doesn't today), things break. Controller lives entirely on UI thread per Stage 4a §3.1. No action needed; flag if Stage 5 introduces background commands.
7. **macOS Cmd vs Linux/Windows Ctrl.** `StandardKey.Undo` resolves to `Cmd+Z` on macOS, `Ctrl+Z` elsewhere automatically. Good.
8. **Initial inspector width.** SplitView `preferredWidth: 320` works; not stored across sessions. Stage 4d adds QSettings-backed persistence.

---

## 10. Критерии готовности Stage 4b

- [ ] `coupecad_ui` собирается на Linux + Windows CI.
- [ ] Все методы `CabinetPropertiesProxy` и `PanelPropertiesProxy` покрыты тестами через реальный Project+UndoStack+FakeRenderer.
- [ ] `SetCabinetName` команда работает (apply/revert/validate тесты).
- [ ] Camera precision: 1000-step small-pan тест проходит; wheel-zoom no-drift тест проходит.
- [ ] `ViewportController` теперь QObject; `selectionChanged`/`cabinetChanged`/`panelChanged` сигналы fire'ятся корректно.
- [ ] Манульно (macOS): SplitView отображается, inspector переключается по pick, edit-back работает, undo/redo через Cmd+Z/Y работает.
- [ ] Не сломаны 328 тестов Stage 4a baseline.
- [ ] Public `IRenderer` остаётся Qt-free; OCCT-renderer-libs остаются 0 Qt symbols.
