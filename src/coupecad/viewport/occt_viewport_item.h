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
