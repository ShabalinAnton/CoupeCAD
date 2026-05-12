#pragma once

#include "coupecad/core/commands/change_set.h"
#include "coupecad/core/project.h"
#include "coupecad/renderer/entity_id.h"
#include "coupecad/renderer/i_renderer.h"

#include <QObject>
#include <QString>

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
class ViewportController : public QObject, public core::IProjectObserver {
    Q_OBJECT
public:
    ViewportController(core::Project& project,
                       geometry::GeometryBuilder& builder,
                       renderer::IRenderer& renderer,
                       QObject* parent = nullptr);
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

signals:
    void selectionChanged();
    void cabinetChanged();
    void panelChanged(const QString& panel_id_str);

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
