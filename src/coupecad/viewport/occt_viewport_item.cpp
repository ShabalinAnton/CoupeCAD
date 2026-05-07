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
