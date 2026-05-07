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
