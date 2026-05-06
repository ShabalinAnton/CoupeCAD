#pragma once

#include <Aspect_NeutralWindow.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>

namespace coupecad::renderer::occt {

// Владеет OpenGl_GraphicDriver (lazy process-wide), V3d_Viewer, V3d_View
// и Aspect_NeutralWindow для headless-операций.
//
// Headless-fallback: если инициализация GL-драйвера падает (нет
// X-сервера / GL-context'а), V3d-объекты всё равно создаются (они не
// требуют GL для in-memory структур), а gl_available() возвращает
// false. См. spec §5.1.
//
// Не thread-safe. Один экземпляр на OcctRenderer.
class ViewDriver {
public:
    ViewDriver();
    ~ViewDriver();

    ViewDriver(const ViewDriver&) = delete;
    ViewDriver& operator=(const ViewDriver&) = delete;

    const Handle(V3d_Viewer)&           viewer() const noexcept { return viewer_; }
    const Handle(V3d_View)&             view() const noexcept { return view_; }
    const Handle(Aspect_NeutralWindow)& window() const noexcept { return window_; }

    bool gl_available() const noexcept { return gl_available_; }

    void set_viewport_size(int width, int height);
    int  viewport_width() const noexcept { return width_; }
    int  viewport_height() const noexcept { return height_; }

private:
    Handle(OpenGl_GraphicDriver) driver_;
    Handle(V3d_Viewer)           viewer_;
    Handle(V3d_View)             view_;
    Handle(Aspect_NeutralWindow) window_;
    bool                         gl_available_ = false;
    int                          width_  = 1;
    int                          height_ = 1;
};

}  // namespace coupecad::renderer::occt
