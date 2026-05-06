#include "coupecad/renderer/occt/view_driver.h"

#include "coupecad/logging/logger.h"

#include <Aspect_DisplayConnection.hxx>

#include <exception>

namespace coupecad::renderer::occt {

namespace {

Handle(OpenGl_GraphicDriver) try_make_gl_driver(bool& ok_out) {
    try {
        Handle(Aspect_DisplayConnection) display = new Aspect_DisplayConnection();
        Handle(OpenGl_GraphicDriver) drv = new OpenGl_GraphicDriver(display, false);
        // InitContext() требует windowing system; в headless-CI бросит.
        // Сохраняем исключение и переходим в logical mode.
        ok_out = true;
        return drv;
    } catch (const std::exception& e) {
        coupecad::logging::Logger::instance().warn(
            "renderer", "renderer.gl_unavailable: OpenGl_GraphicDriver init failed: {}",
            e.what());
        ok_out = false;
        return Handle(OpenGl_GraphicDriver){};
    } catch (...) {
        coupecad::logging::Logger::instance().warn(
            "renderer", "renderer.gl_unavailable: OpenGl_GraphicDriver init failed: <unknown>");
        ok_out = false;
        return Handle(OpenGl_GraphicDriver){};
    }
}

}  // namespace

ViewDriver::ViewDriver() {
    driver_ = try_make_gl_driver(gl_available_);

    // V3d_Viewer и V3d_View строятся даже если driver_ — нулевой handle:
    // OCCT поддерживает их как logical structures; рисование откажет
    // graceful'но в render_to_image, всё остальное (BVH, селекция,
    // позиции, AIS-граф) работает.
    viewer_ = new V3d_Viewer(driver_);
    viewer_->SetDefaultLights();
    viewer_->SetLightOn();

    view_ = viewer_->CreateView();

    // NeutralWindow без нативного хэндла — достаточно для projection-
    // математики picking'а. Размер 1×1 как стартовый.
    window_ = new Aspect_NeutralWindow();
    window_->SetSize(static_cast<Standard_Integer>(width_),
                     static_cast<Standard_Integer>(height_));
    view_->SetWindow(window_);

    // Z-up по умолчанию (см. spec §5.5).
    view_->SetUp(0.0, 0.0, 1.0);

    coupecad::logging::Logger::instance().info(
        "renderer", "ViewDriver constructed (gl_available={})", gl_available_);
}

ViewDriver::~ViewDriver() = default;

void ViewDriver::set_viewport_size(int width, int height) {
    width_  = width;
    height_ = height;
    if (!window_.IsNull()) {
        window_->SetSize(static_cast<Standard_Integer>(width),
                         static_cast<Standard_Integer>(height));
    }
    if (!view_.IsNull()) {
        view_->MustBeResized();
    }
}

}  // namespace coupecad::renderer::occt
