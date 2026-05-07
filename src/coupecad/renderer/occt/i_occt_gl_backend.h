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
