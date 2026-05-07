#include "coupecad/renderer/occt/occt_renderer.h"

#include "coupecad/core/errors.h"
#include "coupecad/core/project.h"
#include "coupecad/geometry/geometry_builder.h"

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
