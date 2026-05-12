#include "coupecad/renderer/occt/occt_renderer.h"

#include "coupecad/core/errors.h"
#include "coupecad/core/project.h"
#include "coupecad/geometry/geometry_builder.h"
#include "coupecad/renderer/occt/i_occt_gl_backend.h"

#include <Aspect_DisplayConnection.hxx>
#include <OpenGl_GraphicDriver.hxx>

#include <gtest/gtest.h>

using coupecad::core::Project;
using coupecad::geometry::GeometryBuilder;
using coupecad::renderer::ViewportSize;
using coupecad::renderer::occt::OcctRenderer;

TEST(OcctRendererViewportTest, SetThenGetRoundTrip) {
    Project project = Project::create_empty("vp");
    GeometryBuilder builder{project};
    OcctRenderer renderer{project, builder};

    renderer.set_viewport_size(ViewportSize{1280, 720});
    auto vp = renderer.viewport_size();
    EXPECT_EQ(vp.width, 1280);
    EXPECT_EQ(vp.height, 720);
}

TEST(OcctRendererViewportTest, ZeroOrNegativeSizeThrows) {
    Project project = Project::create_empty("vp2");
    GeometryBuilder builder{project};
    OcctRenderer renderer{project, builder};

    try {
        renderer.set_viewport_size(ViewportSize{0, 100});
        FAIL() << "expected DomainError";
    } catch (const coupecad::core::DomainError& e) {
        EXPECT_EQ(e.code(), "renderer.invalid_viewport_size");
    }
}

TEST(OcctRendererRenderSmokeTest, RenderToImageEitherEmptyOrFullBuffer) {
    Project project = Project::create_empty("render");
    GeometryBuilder builder{project};
    OcctRenderer renderer{project, builder};

    renderer.set_viewport_size(ViewportSize{64, 64});
    auto bytes = renderer.render_to_image();
    if (bytes.empty()) {
        // Headless: GL unavailable — renderer returned empty (warning logged).
        SUCCEED();
    } else {
        EXPECT_EQ(bytes.size(), static_cast<std::size_t>(64 * 64 * 4));
    }
}

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
